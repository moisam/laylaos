/****************************************************************************
**
** Copyright (C) 2025 Mohammed Isam
** Copyright (C) 2016 Research In Motion
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "laylaosaudioinput.h"
#include "laylaosaudioutils.h"
#include "laylaosaudiodeviceinfo.h"

#include <private/qaudiohelpers_p.h>

#include <fcntl.h>
#include <unistd.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

LaylaOSAudioInput::LaylaOSAudioInput()
    : m_fd(-1)
    , m_audioSource(0)
    , m_notifier(0)
    , m_error(QAudio::NoError)
    , m_state(QAudio::StoppedState)
    , m_bytesRead(0)
    , m_elapsedTimeOffset(0)
    , m_totalTimeValue(0)
    , m_volume(qreal(1.0f))
    , m_bytesAvailable(0)
    , m_bufferSize(0)
    , m_periodSize(0)
    , m_intervalTime(1000)
    , m_pullMode(true)
{
}

LaylaOSAudioInput::~LaylaOSAudioInput()
{
    close();
}

void LaylaOSAudioInput::start(QIODevice *device)
{
    if (m_state != QAudio::StoppedState)
        close();

    if (!m_pullMode && m_audioSource)
        delete m_audioSource;

    m_pullMode = true;
    m_audioSource = device;

    if (open()) {
        setError(QAudio::NoError);
        setState(QAudio::ActiveState);
    } else {
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }
}

QIODevice *LaylaOSAudioInput::start()
{
    if (m_state != QAudio::StoppedState)
        close();

    if (!m_pullMode && m_audioSource)
        delete m_audioSource;

    m_pullMode = false;
    m_audioSource = new InputPrivate(this);
    m_audioSource->open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    if (open()) {
        setError(QAudio::NoError);
        setState(QAudio::IdleState);
    } else {
        delete m_audioSource;
        m_audioSource = 0;

        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }

    return m_audioSource;
}

void LaylaOSAudioInput::stop()
{
    if (m_state == QAudio::StoppedState)
        return;

    setError(QAudio::NoError);
    setState(QAudio::StoppedState);
    close();
}

void LaylaOSAudioInput::reset()
{
    stop();
    m_bytesAvailable = 0;
}

void LaylaOSAudioInput::suspend()
{
    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_STOP, NULL);

    if (m_notifier)
        m_notifier->setEnabled(false);

    setState(QAudio::SuspendedState);
}

void LaylaOSAudioInput::resume()
{
   if (m_fd >= 0)
        ioctl(m_fd, AUDIO_START, NULL);

    if (m_notifier)
        m_notifier->setEnabled(true);

    if (m_pullMode) {
        setState(QAudio::ActiveState);
    } else {
        setState(QAudio::IdleState);
    }
}

int LaylaOSAudioInput::bytesReady() const
{
    return qMax(m_bytesAvailable, 0);
}

int LaylaOSAudioInput::periodSize() const
{
    return m_periodSize;
}

void LaylaOSAudioInput::setBufferSize(int bufferSize)
{
    m_bufferSize = bufferSize;
}

int LaylaOSAudioInput::bufferSize() const
{
    return m_bufferSize;
}

void LaylaOSAudioInput::setNotifyInterval(int milliSeconds)
{
    m_intervalTime = qMax(0, milliSeconds);
}

int LaylaOSAudioInput::notifyInterval() const
{
    return m_intervalTime;
}

qint64 LaylaOSAudioInput::processedUSecs() const
{
    return qint64(1000000) * m_format.framesForBytes(m_bytesRead) / m_format.sampleRate();
}

qint64 LaylaOSAudioInput::elapsedUSecs() const
{
    if (m_state == QAudio::StoppedState)
        return 0;

    return m_clockStamp.elapsed() * qint64(1000);
}

QAudio::Error LaylaOSAudioInput::error() const
{
    return m_error;
}

QAudio::State LaylaOSAudioInput::state() const
{
    return m_state;
}

void LaylaOSAudioInput::setFormat(const QAudioFormat &format)
{
    if (m_state == QAudio::StoppedState)
        m_format = format;
}

QAudioFormat LaylaOSAudioInput::format() const
{
    return m_format;
}

void LaylaOSAudioInput::setVolume(qreal volume)
{
    m_volume = qBound(qreal(0.0), volume, qreal(1.0));
}

qreal LaylaOSAudioInput::volume() const
{
    return m_volume;
}

void LaylaOSAudioInput::userFeed()
{
    if (m_state == QAudio::StoppedState || m_state == QAudio::SuspendedState)
        return;

    deviceReady();
}

bool LaylaOSAudioInput::deviceReady()
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

    if (m_intervalTime && (m_timeStamp.elapsed() + m_elapsedTimeOffset) > m_intervalTime) {
        emit notify();
        m_elapsedTimeOffset = m_timeStamp.elapsed() + m_elapsedTimeOffset - m_intervalTime;
        m_timeStamp.restart();
    }

    return true;
}

bool LaylaOSAudioInput::open()
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

    LaylaOSAudioUtils::formatToChannelParams(m_format, &info, QAudio::AudioInput);

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

    m_clockStamp.restart();
    m_timeStamp.restart();
    m_elapsedTimeOffset = 0;
    m_totalTimeValue = 0;
    m_bytesRead = 0;

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, SIGNAL(activated(int)), SLOT(userFeed()));

    return true;
}

void LaylaOSAudioInput::close()
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

qint64 LaylaOSAudioInput::read(char *data, qint64 len)
{
    QByteArray tempBuffer(m_periodSize, 0);

    const int actualRead = ::read(m_fd, tempBuffer.data(), m_periodSize);

    if (actualRead < 0) {
        return -1;
    } else {
        setError(QAudio::NoError);
        setState(QAudio::ActiveState);
    }

    if (m_volume < 1.0f)
        QAudioHelperInternal::qMultiplySamples(m_volume, m_format, tempBuffer.data(), tempBuffer.data(), actualRead);

    m_bytesRead += actualRead;

    if (m_pullMode) {
        m_audioSource->write(tempBuffer.data(), actualRead);
    } else {
        memcpy(data, tempBuffer.data(), qMin(static_cast<qint64>(actualRead), len));
    }

    m_bytesAvailable = 0;

    return actualRead;
}

void LaylaOSAudioInput::setError(QAudio::Error error)
{
    if (m_error == error)
        return;

    m_error = error;
    emit errorChanged(m_error);
}

void LaylaOSAudioInput::setState(QAudio::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(m_state);
}

InputPrivate::InputPrivate(LaylaOSAudioInput *audio)
    : m_audioDevice(audio)
{
}

qint64 InputPrivate::readData(char *data, qint64 len)
{
    return m_audioDevice->read(data, len);
}

qint64 InputPrivate::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data)
    Q_UNUSED(len)
    return 0;
}

void InputPrivate::trigger()
{
    emit readyRead();
}

QT_END_NAMESPACE
