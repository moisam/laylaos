// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosaudiosource_p.h"
#include "qlaylaosaudiodevice_p.h"

#include <private/qaudiohelpers_p.h>

#include <QDebug>
#include <fcntl.h>
#include <unistd.h>

QT_BEGIN_NAMESPACE

QLaylaOSAudioSource::QLaylaOSAudioSource(QAudioDevice device, const QAudioFormat &format, QObject *parent)
    : QPlatformAudioSource(std::move(device), format, parent),
      m_fd(-1),
      m_audioSource(0),
      m_notifier(0),
      m_state(QAudio::StoppedState),
      m_bytesRead(0),
      m_elapsedTimeOffset(0),
      m_totalTimeValue(0),
      m_bytesAvailable(0),
      m_bufferSize(0),
      m_periodSize(0),
      m_pullMode(true)
{
}

QLaylaOSAudioSource::~QLaylaOSAudioSource()
{
    close();
}

void QLaylaOSAudioSource::start(QIODevice *device)
{
    if (m_state != QAudio::StoppedState)
        close();

    if (!m_pullMode && m_audioSource)
        delete m_audioSource;

    m_pullMode = true;
    m_audioSource = device;

    if (open())
        changeState(QAudio::ActiveState, QAudio::NoError);
    else
        changeState(QAudio::StoppedState, QAudio::OpenError);
}

QIODevice *QLaylaOSAudioSource::start()
{
    if (m_state != QAudio::StoppedState)
        close();

    if (!m_pullMode && m_audioSource)
        delete m_audioSource;

    m_pullMode = false;
    m_audioSource = new InputPrivate(this);
    m_audioSource->open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    if (open()) {
        changeState(QAudio::IdleState, QAudio::NoError);
    } else {
        delete m_audioSource;
        m_audioSource = 0;

        changeState(QAudio::StoppedState, QAudio::OpenError);
    }

    return m_audioSource;
}

void QLaylaOSAudioSource::stop()
{
    if (m_state == QAudio::StoppedState)
        return;

    changeState(QAudio::StoppedState, QAudio::NoError);
    close();
}

void QLaylaOSAudioSource::reset()
{
    stop();
    m_bytesAvailable = 0;
}

void QLaylaOSAudioSource::suspend()
{
    if (m_state == QAudio::StoppedState)
        return;

    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_STOP, NULL);

    if (m_notifier)
        m_notifier->setEnabled(false);

    changeState(QAudio::SuspendedState, QAudio::NoError);
}

void QLaylaOSAudioSource::resume()
{
    if (m_state == QAudio::StoppedState)
        return;

   if (m_fd >= 0)
        ioctl(m_fd, AUDIO_START, NULL);

    if (m_notifier)
        m_notifier->setEnabled(true);

    if (m_pullMode)
        changeState(QAudio::ActiveState, QAudio::NoError);
    else
        changeState(QAudio::IdleState, QAudio::NoError);
}

qsizetype QLaylaOSAudioSource::bytesReady() const
{
    return qMax(m_bytesAvailable, 0);
}

void QLaylaOSAudioSource::setBufferSize(qsizetype bufferSize)
{
    m_bufferSize = bufferSize;
}

qsizetype QLaylaOSAudioSource::bufferSize() const
{
    return m_bufferSize;
}

qint64 QLaylaOSAudioSource::processedUSecs() const
{
    return qint64(1000000) * m_format.framesForBytes(m_bytesRead) / m_format.sampleRate();
}

QAudio::State QLaylaOSAudioSource::state() const
{
    return m_state;
}

void QLaylaOSAudioSource::userFeed()
{
    if (m_state == QAudio::StoppedState || m_state == QAudio::SuspendedState)
        return;

    deviceReady();
}

bool QLaylaOSAudioSource::deviceReady()
{
    if (m_pullMode) {
        // reads some audio data and writes it to QIODevice
        read(0, 0);
    } else {
        m_bytesAvailable = m_periodSize;

        // emits readyRead() so user will call read() on QIODevice to get some audio data
        if (m_audioSource != 0) {
            InputPrivate *input = qobject_cast<InputPrivate*>(m_audioSource);
            input->trigger();
        }
    }

    if (m_state != QAudio::ActiveState)
        return true;

    return true;
}

bool QLaylaOSAudioSource::open()
{
    if (!m_format.isValid() || m_format.sampleRate() <= 0) {
        if (!m_format.isValid())
            qWarning("LaylaOSAudioInput: open error, invalid format.");
        else
            qWarning("LaylaOSAudioInput: open error, invalid sample rate (%d).", m_format.sampleRate());

        return false;
    }

    audio_info_t info;
    int errorCode;

    AUDIO_INITINFO(&info);
    info.mode = AUMODE_RECORD;

    if (m_fd < 0) {
        m_fd = ::open(DEFAULT_DEVICE_PATH, O_RDONLY | O_CLOEXEC, 0);

        if (m_fd < 0) {
            qWarning("LaylaOSAudioInput: open error, couldn't open card");
            return false;
        }
    }

    if ((errorCode = ioctl(m_fd, AUDIO_GETINFO, &info)) < 0) {
        qWarning("LaylaOSAudioInput: open error, couldn't get channel info (err %d)", errorCode);
        close();
        return false;
    }

    LaylaOSAudioUtils::formatToChannelParams(m_format, &info, QAudioDevice::Input);

    if ((errorCode = ioctl(m_fd, AUDIO_SETINFO, &info)) < 0) {
        qWarning("LaylaOSAudioInput: open error, couldn't set channel info (err %d)", errorCode);
        close();
        return false;
    }

    if ((errorCode = ioctl(m_fd, AUDIO_GETINFO, &info)) < 0) {
        qWarning("LaylaOSAudioInput: open error, couldn't get channel setup (err %d)", errorCode);
        close();
        return false;
    }

    m_periodSize = qMin(2048, (int)info.blocksize);

    m_elapsedTimeOffset = 0;
    m_totalTimeValue = 0;
    m_bytesRead = 0;

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, SIGNAL(activated(int)), SLOT(userFeed()));

    return true;
}

void QLaylaOSAudioSource::close()
{
    if (m_fd >= 0) {
        ioctl(m_fd, AUDIO_FLUSH, NULL);
        ::close(m_fd);
        m_fd = -1;
    }

    if (m_notifier) {
        delete m_notifier;
        m_notifier = 0;
    }

    if (!m_pullMode && m_audioSource) {
        delete m_audioSource;
        m_audioSource = 0;
    }
}

qint64 QLaylaOSAudioSource::read(char *data, qint64 len)
{
    if (!m_pullMode && m_bytesAvailable == 0)
        return 0;

    QByteArray tempBuffer(m_periodSize, 0);

    const int actualRead = ::read(m_fd, tempBuffer.data(), m_periodSize);

    if (actualRead < 0) {
        close();
        changeState(QAudio::StoppedState, QAudio::FatalError);
        return -1;
    } else {
        changeState(QAudio::ActiveState, QAudio::NoError);
    }

    if (volume() < 1.0f)
        QAudioHelperInternal::qMultiplySamples(volume(), m_format, tempBuffer.data(), tempBuffer.data(), actualRead);

    m_bytesRead += actualRead;

    if (m_pullMode) {
        m_audioSource->write(tempBuffer.data(), actualRead);
    } else {
        memcpy(data, tempBuffer.data(), qMin(static_cast<qint64>(actualRead), len));
    }

    m_bytesAvailable = 0;

    return actualRead;
}

void QLaylaOSAudioSource::changeState(QAudio::State state, QAudio::Error error)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }

    setError(error);
}

InputPrivate::InputPrivate(QLaylaOSAudioSource *audio)
    : m_audioDevice(audio)
{
}

qint64 InputPrivate::readData(char *data, qint64 len)
{
    return m_audioDevice->read(data, len);
}

qint64 InputPrivate::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return 0;
}

qint64 InputPrivate::bytesAvailable() const
{
    return m_audioDevice->m_bytesAvailable + QIODevice::bytesAvailable();
}

bool InputPrivate::isSequential() const
{
    return true;
}

void InputPrivate::trigger()
{
    emit readyRead();
}

QT_END_NAMESPACE
