// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosaudiosink_p.h"
#include "qlaylaosaudiodevice_p.h"

#include <private/qaudiohelpers_p.h>

#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Wvla"

QT_BEGIN_NAMESPACE

QLaylaOSAudioSink::QLaylaOSAudioSink(QAudioDevice device, const QAudioFormat &format, QObject *parent)
    : QPlatformAudioSink(std::move(device), format, parent),
      m_source(0),
      m_pushSource(false),
      m_timer(new QTimer(this)),
      m_state(QAudio::StoppedState),
      m_suspendedInState(QAudio::IdleState),
      m_periodSize(0),
      m_bytesWritten(0),
      m_requestedBufferSize(0),
      m_fd(-1)
{
    m_timer->setSingleShot(false);
    m_timer->setInterval(20);
    connect(m_timer, &QTimer::timeout, this, &QLaylaOSAudioSink::pullData);

    m_requestedBufferSize = 4096;
}

QLaylaOSAudioSink::~QLaylaOSAudioSink()
{
    stop();
}

void QLaylaOSAudioSink::start(QIODevice *source)
{
    if (m_state != QAudio::StoppedState)
        stop();

    m_source = source;
    m_pushSource = false;

    if (open()) {
        changeState(QAudio::ActiveState, QAudio::NoError);
        m_timer->start();
    } else {
        changeState(QAudio::StoppedState, QAudio::OpenError);
    }
}

QIODevice *QLaylaOSAudioSink::start()
{
    if (m_state != QAudio::StoppedState)
        stop();

    m_source = new LaylaOSPushIODevice(this);
    m_source->open(QIODevice::WriteOnly|QIODevice::Unbuffered);
    m_pushSource = true;

    if (open()) {
        changeState(QAudio::IdleState, QAudio::NoError);
    } else {
        changeState(QAudio::StoppedState, QAudio::OpenError);
    }

    return m_source;
}

void QLaylaOSAudioSink::stop()
{
    if (m_state == QAudio::StoppedState)
        return;

    changeState(QAudio::StoppedState, QAudio::NoError);

    close();
}

void QLaylaOSAudioSink::reset()
{
    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_FLUSH, NULL);

    stop();
}

void QLaylaOSAudioSink::suspend()
{
    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_STOP, NULL);

    suspendInternal(QAudio::SuspendedState);
}

void QLaylaOSAudioSink::resume()
{
    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_START, NULL);

    resumeInternal();
}

void QLaylaOSAudioSink::setBufferSize(qsizetype bufferSize)
{
    m_requestedBufferSize = std::clamp<qsizetype>(bufferSize, 0, std::numeric_limits<int>::max());
}

qsizetype QLaylaOSAudioSink::bufferSize() const
{
    return m_requestedBufferSize;
}

qsizetype QLaylaOSAudioSink::bytesFree() const
{
    if (m_state != QAudio::ActiveState && m_state != QAudio::IdleState)
        return 0;

    audio_info_t info;

    AUDIO_INITINFO(&info);
    info.mode = AUMODE_PLAY;

    if (m_fd < 0 || ioctl(m_fd, AUDIO_GETINFO, &info) < 0)
        return 0;

    return info.blocksize;
}

qint64 QLaylaOSAudioSink::processedUSecs() const
{
    return qint64(1000000) * m_format.framesForBytes(m_bytesWritten) / m_format.sampleRate();
}

QAudio::State QLaylaOSAudioSink::state() const
{
    return m_state;
}

void QLaylaOSAudioSink::updateState()
{
    audio_info_t info;

    AUDIO_INITINFO(&info);
    info.mode = AUMODE_PLAY;

    if (m_fd < 0 || ioctl(m_fd, AUDIO_GETINFO, &info) < 0)
        return;

    if (state() == QAudio::ActiveState && info.play.active == 0)
        changeState(QAudio::IdleState, QAudio::NoError);
    else if (state() == QAudio::IdleState && info.play.active != 0)
        changeState(QAudio::ActiveState, QAudio::NoError);
}

void QLaylaOSAudioSink::pullData()
{
    if (m_state == QAudio::StoppedState
            || m_state == QAudio::SuspendedState)
        return;

    const int bytesAvailable = bytesFree();
    const int frames = m_format.framesForBytes(bytesAvailable);

    if (frames == 0 || bytesAvailable < m_periodSize)
        return;

    // The buffer is placed on the stack so no more than 64K or 1 frame
    // whichever is larger.
    const int maxFrames = qMax(m_format.framesForBytes(64 * 1024), 1);
    const int bytesRequested = m_format.bytesForFrames(qMin(frames, maxFrames));

    char buffer[bytesRequested];
    const int bytesRead = m_source->read(buffer, bytesRequested);

    // reading can take a while and stream may have been stopped
    if (m_fd < 0)
        return;

    if (bytesRead > 0) {
        const qint64 bytesWritten = write(buffer, bytesRead);

        if (bytesWritten <= 0) {
            close();
            changeState(QAudio::StoppedState, QAudio::FatalError);
        } else if (bytesWritten != bytesRead) {
            m_source->seek(m_source->pos()-(bytesRead-bytesWritten));
        }
    } else {
        // We're done
        if (bytesRead == 0)
            changeState(QAudio::IdleState, QAudio::NoError);
        else
            changeState(QAudio::IdleState, QAudio::IOError);
    }
}

bool QLaylaOSAudioSink::open()
{
    if (!m_format.isValid() || m_format.sampleRate() <= 0) {
        if (!m_format.isValid())
            qWarning("LaylaOSAudioOutput: open error, invalid format.");
        else
            qWarning("LaylaOSAudioOutput: open error, invalid sample rate (%d).", m_format.sampleRate());

        return false;
    }

    audio_info_t info;
    int errorCode;

    AUDIO_INITINFO(&info);
    info.mode = AUMODE_PLAY;

    if (m_fd < 0) {
        m_fd = ::open(DEFAULT_DEVICE_PATH, O_WRONLY | O_CLOEXEC);

        if (m_fd < 0) {
            qWarning("LaylaOSAudioInput: open error, couldn't open card");
            return false;
        }
    }

    if ((errorCode = ioctl(m_fd, AUDIO_GETINFO, &info)) < 0) {
        qWarning("LaylaOSAudioOutput: open error, couldn't get channel info (err %d)", errorCode);
        close();
        return false;
    }

    LaylaOSAudioUtils::formatToChannelParams(m_format, &info, QAudioDevice::Output);

    //qDebug() << "LaylaOSAudioOutput::open: " << m_format.sampleRate() << m_format.channelCount() << m_format.sampleSize() << m_format.sampleType();

    if ((errorCode = ioctl(m_fd, AUDIO_SETINFO, &info)) < 0) {
        qWarning("LaylaOSAudioOutput: open error, couldn't set channel info (err %d)", errorCode);
        close();
        return false;
    }

    if ((errorCode = ioctl(m_fd, AUDIO_GETINFO, &info)) < 0) {
        qWarning("LaylaOSAudioOutput: open error, couldn't get channel setup (err %d)", errorCode);
        close();
        return false;
    }

    m_periodSize = qMin(2048, (int)info.blocksize);
    m_bytesWritten = 0;

    return true;
}

void QLaylaOSAudioSink::close()
{
    if (!m_pushSource)
        m_timer->stop();

    if (m_fd >= 0) {
        ioctl(m_fd, AUDIO_FLUSH, NULL);
        ::close(m_fd);
        m_fd = -1;
    }

    if (m_pushSource) {
        delete m_source;
        m_source = 0;
    }
}

void QLaylaOSAudioSink::changeState(QAudio::State state, QAudio::Error error)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }

    setError(error);
}

qint64 QLaylaOSAudioSink::pushData(const char *data, qint64 len)
{
    const QAudio::State s = state();

    if (s == QAudio::StoppedState || s == QAudio::SuspendedState)
        return 0;

    if (s == QAudio::IdleState)
        changeState(QAudio::ActiveState, QAudio::NoError);

    qint64 totalWritten = 0;

    int retry = 0;

    constexpr int maxRetries = 10;

    while (totalWritten < len) {
        const int bytesWritten = write(data + totalWritten, len - totalWritten);

        if (bytesWritten <= 0) {
            ++retry;

            if (retry >= maxRetries) {
                close();
                changeState(QAudio::StoppedState, QAudio::FatalError);

                return totalWritten;
            } else {
                continue;
            }
        }

        retry = 0;

        totalWritten += bytesWritten;
    }

    return totalWritten;
}

qint64 QLaylaOSAudioSink::write(const char *data, qint64 len)
{
    if (m_fd < 0)
        return 0;

    // Make sure we're writing (N * frame) worth of bytes
    const int size = m_format.bytesForFrames(qBound(qint64(0), qint64(bytesFree()), len) / m_format.bytesPerFrame());

    if (size == 0)
        return 0;

    int written = 0;

    if (volume() < 1.0f) {
        char out[size];
        QAudioHelperInternal::qMultiplySamples(volume(), m_format, data, out, size);
        written = ::write(m_fd, out, size);
    } else {
        written = ::write(m_fd, data, size);
    }

    if (written > 0) {
        m_bytesWritten += written;
        return written;
    }

    return 0;
}

void QLaylaOSAudioSink::suspendInternal(QAudio::State suspendState)
{
    if (!m_pushSource)
        m_timer->stop();

    m_suspendedInState = m_state;
    changeState(suspendState, QAudio::NoError);
}

void QLaylaOSAudioSink::resumeInternal()
{
    changeState(m_suspendedInState, QAudio::NoError);

    if (!m_pushSource)
        m_timer->start();
}

LaylaOSPushIODevice::LaylaOSPushIODevice(QLaylaOSAudioSink *output)
    : QIODevice(output),
      m_output(output)
{
}

LaylaOSPushIODevice::~LaylaOSPushIODevice()
{
}

qint64 LaylaOSPushIODevice::readData(char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return 0;
}

qint64 LaylaOSPushIODevice::writeData(const char *data, qint64 len)
{
    return m_output->pushData(data, len);
}

bool LaylaOSPushIODevice::isSequential() const
{
    return true;
}

QT_END_NAMESPACE
