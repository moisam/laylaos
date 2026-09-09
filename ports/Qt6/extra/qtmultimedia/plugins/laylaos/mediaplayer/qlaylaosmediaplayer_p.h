// Copyright (C) 2026 Mohammed Isam
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
#ifndef QLaylaOSMediaPlayer_H
#define QLaylaOSMediaPlayer_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qplatformmediaplayer_p.h>
#include <QAudioSink>
#include <QIODevice>
#include <QUrl>
#include <QSize>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

QT_BEGIN_NAMESPACE

class QLaylaOSMediaPlayer;
class QLaylaOSMediaWorker;

class QLaylaOSMediaWorker : public QThread
{
    Q_OBJECT
public:
    QLaylaOSMediaWorker(QLaylaOSMediaPlayer *player);
    ~QLaylaOSMediaWorker() override;

    void stopDecoding();

protected:
    void run() override;

private:
    QLaylaOSMediaPlayer *m_player;
    std::atomic<bool> m_stopRequested;
};

class QLaylaOSMediaPlayer : public QObject, public QPlatformMediaPlayer
{
    Q_OBJECT

    friend class QLaylaOSMediaWorker;

public:
    explicit QLaylaOSMediaPlayer(QMediaPlayer *parent = nullptr);
    ~QLaylaOSMediaPlayer();

    qint64 duration() const override { return m_duration; }
    float bufferProgress() const override { return 1.0f; } // Fully loaded for local files
    QMediaTimeRange availablePlaybackRanges() const override { return QMediaTimeRange(0, m_duration); }
    
    QUrl media() const override { return m_source; }
    const QIODevice *mediaStream() const override { return m_mediaStream; }
    
    void setMedia(const QUrl &media, QIODevice *stream) override;
    
    void setVideoSink(QVideoSink *sink) override { m_customVideoSink = sink; }

    void play() override;
    void pause() override;
    void stop() override;

    qint64 position() const override { return m_position; }
    void setPosition(qint64 position) override;

    qreal playbackRate() const override { return m_rate; }
    void setPlaybackRate(qreal rate) override { m_rate = rate; }

    int volume() const { return m_volume; }
    void setVolume(int volume) { m_volume = volume; }

    bool isMuted() const { return m_muted; }
    void setMuted(bool muted) { m_muted = muted; }

    QMediaPlayer::PlaybackState playbackState() const { return m_state; }

private:
    void initializeAudioPipeline(AVCodecContext *audioCodecCtx);
    void processVideoFrame(AVFrame *videoFrame, AVStream *videoStream);
    void processAudioFrame(AVFrame *audioFrame, AVStream *audioStream);
    void updateState(QMediaPlayer::PlaybackState newState);
    void clearPipeline();

    QMediaPlayer *m_frontendPlayer = nullptr;

    // Structural State Metadata
    qint64 m_duration = 0;
    QIODevice *m_mediaStream = nullptr;
    QVideoSink *m_customVideoSink = nullptr;
    QUrl m_source;
    qint64 m_position = 0;
    qreal m_rate = 1.0f;
    int m_volume = 100;
    bool m_muted = false;
    QMediaPlayer::PlaybackState m_state = QMediaPlayer::StoppedState;

    // Thread & Thread-Safety Objects
    QLaylaOSMediaWorker *m_workerThread = nullptr;
    std::atomic<bool> m_isSeeking{false};
    std::mutex m_seekMutex;
    std::condition_variable m_seekCondition;

    // Master-Clock Synchronization Trackers (in seconds)
    std::atomic<double> m_audioClock{0.0};
    std::atomic<double> m_videoClock{0.0};

    // FFmpeg Core Structure Elements
    AVFormatContext *m_formatContext = nullptr;
    AVCodecContext  *m_videoCodecContext = nullptr;
    AVCodecContext  *m_audioCodecContext = nullptr;
    int m_videoStreamIndex = -1;
    int m_audioStreamIndex = -1;

    // Video Rescaling Contexts (FFmpeg -> QPA Blit Sink)
    SwsContext *m_swsContext = nullptr;

    // Native LaylaOS Audio Sink Pipeline Integrators
    SwrContext *m_swrContext = nullptr;
    QAudioSink *m_audioSink = nullptr;
    QIODevice  *m_audioDevice = nullptr;
    uint8_t    *m_audioBuffer = nullptr;
    int         m_audioBufferMaxSize = 0;
};

QT_END_NAMESPACE

#endif
