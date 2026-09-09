#include "qlaylaosmediaplayer_p.h"
#include <QVideoSink>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <chrono>

QLaylaOSMediaWorker::QLaylaOSMediaWorker(QLaylaOSMediaPlayer *player)
    : QThread(player)
    , m_player(player)
    , m_stopRequested(false)
{
}

QLaylaOSMediaWorker::~QLaylaOSMediaWorker()
{
    stopDecoding();
}

void QLaylaOSMediaWorker::stopDecoding()
{
    m_stopRequested.store(true, std::memory_order_relaxed);
}

void QLaylaOSMediaWorker::run()
{
    m_stopRequested.store(false, std::memory_order_relaxed);
    
    AVPacket *packet = av_packet_alloc();

    while (!m_stopRequested.load(std::memory_order_relaxed)) {
        
        if (m_player->m_isSeeking) {
            std::unique_lock<std::mutex> lock(m_player->m_seekMutex);
            m_player->m_seekCondition.wait(lock, [this] { return !m_player->m_isSeeking.load(); });
            continue;
        }

        if (av_read_frame(m_player->m_formatContext, packet) < 0) {
            // Signal stop via safe main-thread invokable state tracking
            QMetaObject::invokeMethod(m_player, "stop", Qt::QueuedConnection);
            break;
        }

        if (m_stopRequested.load(std::memory_order_relaxed)) break;

        if (packet->stream_index == m_player->m_videoStreamIndex) {
            if (avcodec_send_packet(m_player->m_videoCodecContext, packet) == 0) {
                AVFrame *frame = av_frame_alloc();
                if (avcodec_receive_frame(m_player->m_videoCodecContext, frame) == 0) {
                    m_player->processVideoFrame(frame, m_player->m_formatContext->streams[m_player->m_videoStreamIndex]);
                }
                av_frame_free(&frame);
            }
        } 
        else if (packet->stream_index == m_player->m_audioStreamIndex) {
            if (avcodec_send_packet(m_player->m_audioCodecContext, packet) == 0) {
                AVFrame *frame = av_frame_alloc();
                if (avcodec_receive_frame(m_player->m_audioCodecContext, frame) == 0) {
                    m_player->processAudioFrame(frame, m_player->m_formatContext->streams[m_player->m_audioStreamIndex]);
                }
                av_frame_free(&frame);
            }
        }
        av_packet_unref(packet);
    }
    av_packet_free(&packet);
}

QLaylaOSMediaPlayer::QLaylaOSMediaPlayer(QMediaPlayer *player)
    : QObject(player)
    , QPlatformMediaPlayer(player)
    , m_frontendPlayer(player)
{
    // Register the custom network protocol configurations for FFmpeg
    avformat_network_init();
}

QLaylaOSMediaPlayer::~QLaylaOSMediaPlayer()
{
    m_customVideoSink = nullptr;
    clearPipeline();
    avformat_network_deinit();
}

void QLaylaOSMediaPlayer::setMedia(const QUrl &media, QIODevice *stream)
{
    clearPipeline();
    m_source = media;
    m_mediaStream = stream;
    QString localFile = media.toLocalFile();

    if (avformat_open_input(&m_formatContext, localFile.toUtf8().constData(), nullptr, nullptr) < 0) {
        return; 
    }

    // Extract Stream Layout Metadata Blocks
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        return;
    }

    // Locate Video and Audio Indices and Identify Codecs
    for (unsigned int i = 0; i < m_formatContext->nb_streams; ++i) {
        const AVCodec *codec = avcodec_find_decoder(m_formatContext->streams[i]->codecpar->codec_id);
        if (!codec) continue;

        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_videoStreamIndex < 0) {
            m_videoStreamIndex = i;
            m_videoCodecContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(m_videoCodecContext, m_formatContext->streams[i]->codecpar);
            avcodec_open2(m_videoCodecContext, codec, nullptr);
        } 
        else if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_audioStreamIndex < 0) {
            m_audioStreamIndex = i;
            m_audioCodecContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(m_audioCodecContext, m_formatContext->streams[i]->codecpar);
            avcodec_open2(m_audioCodecContext, codec, nullptr);
        }
    }

    // Initialize Core Processing Context Subroutines
    if (m_audioCodecContext) {
        initializeAudioPipeline(m_audioCodecContext);
    }

    if (m_videoCodecContext) {
        m_swsContext = sws_getContext(
            m_videoCodecContext->width, m_videoCodecContext->height, m_videoCodecContext->pix_fmt,
            m_videoCodecContext->width, m_videoCodecContext->height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
    }

    if (m_formatContext->duration != AV_NOPTS_VALUE) {
        m_duration = (m_formatContext->duration / AV_TIME_BASE) * 1000;
        emit durationChanged(m_duration);
    }
    updateState(QMediaPlayer::StoppedState);
}

void QLaylaOSMediaPlayer::initializeAudioPipeline(AVCodecContext *audioCodecCtx)
{
    QAudioFormat format;
    format.setSampleRate(44100);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    m_audioSink = new QAudioSink(format, this);
    m_audioDevice = m_audioSink->start();

    AVChannelLayout outLayout;
    av_channel_layout_default(&outLayout, 2);

    swr_alloc_set_opts2(&m_swrContext,
                        &outLayout, AV_SAMPLE_FMT_S16, 44100,
                        &audioCodecCtx->ch_layout, audioCodecCtx->sample_fmt, audioCodecCtx->sample_rate,
                        0, nullptr);
    swr_init(m_swrContext);

    m_audioBufferMaxSize = av_samples_get_buffer_size(nullptr, 2, 44100, AV_SAMPLE_FMT_S16, 0);
    m_audioBuffer = (uint8_t *)av_malloc(m_audioBufferMaxSize);
}

void QLaylaOSMediaPlayer::play()
{
    if (m_state != QMediaPlayer::PlayingState) {
        updateState(QMediaPlayer::PlayingState);
        if (m_audioSink) m_audioSink->resume();

        if (!m_workerThread) {
            m_workerThread = new QLaylaOSMediaWorker(this);
            m_workerThread->start();
        }
    }
}

void QLaylaOSMediaPlayer::pause()
{
    if (m_state == QMediaPlayer::PlayingState) {
        updateState(QMediaPlayer::PausedState);
        if (m_audioSink) m_audioSink->suspend();
        
        if (m_workerThread) {
            m_workerThread->stopDecoding();
            m_workerThread->quit();
            m_workerThread->wait();
            delete m_workerThread;
            m_workerThread = nullptr;
        }
    }
}

void QLaylaOSMediaPlayer::stop()
{
    updateState(QMediaPlayer::StoppedState);
    if (m_workerThread) {
        m_workerThread->stopDecoding();
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
    }
    if (m_audioSink) m_audioSink->stop();
    m_position = 0;
    emit positionChanged(0);
}

void QLaylaOSMediaPlayer::setPosition(qint64 positionMs)
{
    if (!m_formatContext) return;

    bool wasPlaying = (m_state == QMediaPlayer::PlayingState);
    if (wasPlaying) m_isSeeking = true;

    double targetSeconds = positionMs / 1000.0;
    int64_t targetTimestamp = static_cast<int64_t>(targetSeconds / av_q2d(m_formatContext->streams[m_videoStreamIndex]->time_base));

    av_seek_frame(m_formatContext, m_videoStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD);

    if (m_videoCodecContext) avcodec_flush_buffers(m_videoCodecContext);
    if (m_audioCodecContext) avcodec_flush_buffers(m_audioCodecContext);
    if (m_audioSink) m_audioSink->reset();

    m_audioClock.store(targetSeconds);
    m_videoClock.store(targetSeconds);
    m_position = positionMs;
    emit positionChanged(m_position);

    if (wasPlaying) {
        m_isSeeking = false;
        m_seekCondition.notify_all();
    }
}

void QLaylaOSMediaPlayer::processAudioFrame(AVFrame *audioFrame, AVStream *audioStream)
{
    if (!m_swrContext || !m_audioDevice) return;

    int modernSamplesOut = swr_convert(m_swrContext, &m_audioBuffer, audioFrame->nb_samples,
                                       (const uint8_t **)audioFrame->data, audioFrame->nb_samples);
    if (modernSamplesOut <= 0) return;

    int dataSize = modernSamplesOut * 2 * sizeof(int16_t);
    m_audioDevice->write((const char*)m_audioBuffer, dataSize);

    if (audioFrame->pts != AV_NOPTS_VALUE) {
        m_audioClock = audioFrame->pts * av_q2d(audioStream->time_base);
        // Push the active elapsed layout position back to the user GUI timeline
        qint64 curPosMs = static_cast<qint64>(m_audioClock.load() * 1000.0);
        if (curPosMs != m_position) {
            m_position = curPosMs;
            emit positionChanged(m_position);
        }
    }
}

void QLaylaOSMediaPlayer::processVideoFrame(AVFrame *videoFrame, AVStream *videoStream)
{
    if (!m_swsContext || !m_customVideoSink) return;

    QSize targetSize = m_customVideoSink->videoSize();

    if (targetSize.isEmpty() || !targetSize.isValid()) {
        targetSize = QSize(m_videoCodecContext->width, m_videoCodecContext->height);
    }

    static QSize activeSwsSize(0, 0);

    if (!m_swsContext || activeSwsSize != targetSize) {
        if (m_swsContext) {
            sws_freeContext(m_swsContext);
        }

        m_swsContext = sws_getContext(
            m_videoCodecContext->width, m_videoCodecContext->height, m_videoCodecContext->pix_fmt, // Src
            targetSize.width(), targetSize.height(), AV_PIX_FMT_RGBA,                              // Dst
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );

        activeSwsSize = targetSize;
    }

    double videoPTS = 0.0;
    if (videoFrame->pts != AV_NOPTS_VALUE) {
        videoPTS = videoFrame->pts * av_q2d(videoStream->time_base);
    }
    m_videoClock = videoPTS;

    // Execute Master-Clock Alignment Computations
    double clockDelayDiff = m_videoClock - m_audioClock.load();

    if (clockDelayDiff > 0.005) {
        unsigned long sleepTimeMs = static_cast<unsigned long>(clockDelayDiff * 1000.0);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMs));
    } else if (clockDelayDiff < -0.05) {
        return; // Skip drawing this frame to catch up with the audio track
    }

    // Allocate a target memory container buffer managed by Qt
    QSize frameSize(m_videoCodecContext->width, m_videoCodecContext->height);

    QVideoFrameFormat format(frameSize, QVideoFrameFormat::Format_RGBA8888);
    QVideoFrame formatFrame(format);

    if (formatFrame.map(QVideoFrame::WriteOnly)) {
        uint8_t *destData[4] = { formatFrame.bits(0), nullptr, nullptr, nullptr };
        int destLinesize[4] = { formatFrame.bytesPerLine(0), 0, 0, 0 };

        // Convert the frame to RGBA format
        sws_scale(m_swsContext, videoFrame->data, videoFrame->linesize, 0,
                  m_videoCodecContext->height, destData, destLinesize);

        formatFrame.unmap();
    }

    // Deliver the raw software pixel pointer to the UI rendering loop
    m_customVideoSink->setVideoFrame(formatFrame);
}

void QLaylaOSMediaPlayer::updateState(QMediaPlayer::PlaybackState newState)
{
    m_state = newState;
    emit stateChanged(m_state);
}

void QLaylaOSMediaPlayer::clearPipeline()
{
    if (m_workerThread) {
        m_workerThread->stopDecoding();
        m_workerThread->quit();
        
        if (!m_workerThread->wait(1000)) {
            m_workerThread->terminate();
            m_workerThread->wait();
        }
        
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    m_state = QMediaPlayer::StoppedState;
    m_isSeeking = false;
    m_seekCondition.notify_all();

    /*
    if (m_customVideoSink) {
        delete m_customVideoSink;
        m_customVideoSink = nullptr;
    }
    */

    if (m_audioSink) {
        m_audioSink->stop();
        delete m_audioSink;
        m_audioSink = nullptr;
    }

    m_audioDevice = nullptr;

    // Clean up all underlying FFmpeg structures
    if (m_audioBuffer) { av_free(m_audioBuffer); m_audioBuffer = nullptr; }
    if (m_swrContext) { swr_free(&m_swrContext); m_swrContext = nullptr; }
    if (m_swsContext) { sws_freeContext(m_swsContext); m_swsContext = nullptr; }
    if (m_videoCodecContext) { avcodec_free_context(&m_videoCodecContext); m_videoCodecContext = nullptr; }
    if (m_audioCodecContext) { avcodec_free_context(&m_audioCodecContext); m_audioCodecContext = nullptr; }
    if (m_formatContext) { avformat_close_input(&m_formatContext); m_formatContext = nullptr; }

    m_videoStreamIndex = -1;
    m_audioStreamIndex = -1;
    m_audioClock = 0.0;
    m_videoClock = 0.0;
}

