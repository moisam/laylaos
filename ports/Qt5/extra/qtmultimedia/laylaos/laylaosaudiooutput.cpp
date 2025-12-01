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

#include "laylaosaudiooutput.h"
#include "laylaosaudioutils.h"
#include "laylaosaudiodeviceinfo.h"

#include <private/qaudiohelpers_p.h>

#include <fcntl.h>
#include <unistd.h>
#include <QDebug>


QT_BEGIN_NAMESPACE

LaylaOSAudioOutput::LaylaOSAudioOutput()
    : m_source(0)
    , m_pushSource(false)
    , m_fd(-1)
    , m_notifyInterval(1000)
    , m_error(QAudio::NoError)
    , m_state(QAudio::StoppedState)
    , m_volume(1.0)
    , m_periodSize(0)
    , m_bytesWritten(0)
    , m_intervalOffset(0)
{
    m_timer.setSingleShot(false);
    m_timer.setInterval(20);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(pullData()));
}

LaylaOSAudioOutput::~LaylaOSAudioOutput()
{
    stop();
}

void LaylaOSAudioOutput::start(QIODevice *source)
{
    if (m_state != QAudio::StoppedState)
        stop();

    m_error = QAudio::NoError;
    m_source = source;
    m_pushSource = false;

    if (open()) {
        setState(QAudio::ActiveState);
        m_timer.start();
    } else {
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }
}

QIODevice *LaylaOSAudioOutput::start()
{
    if (m_state != QAudio::StoppedState)
        stop();

    m_error = QAudio::NoError;
    m_source = new LaylaOSPushIODevice(this);
    m_source->open(QIODevice::WriteOnly|QIODevice::Unbuffered);
    m_pushSource = true;

    if (open())
        setState(QAudio::IdleState);
    else {
        setError(QAudio::OpenError);
        setState(QAudio::StoppedState);
    }

    return m_source;
}

void LaylaOSAudioOutput::stop()
{
    if (m_state == QAudio::StoppedState)
        return;

    setError(QAudio::NoError);
    setState(QAudio::StoppedState);
    close();
}

void LaylaOSAudioOutput::reset()
{
    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_FLUSH, NULL);

    stop();
}

void LaylaOSAudioOutput::suspend()
{
    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_STOP, NULL);

    if (state() != QAudio::InterruptedState)
        suspendInternal(QAudio::SuspendedState);
}

void LaylaOSAudioOutput::resume()
{
    if (m_fd >= 0)
        ioctl(m_fd, AUDIO_START, NULL);

    if (state() != QAudio::InterruptedState)
        resumeInternal();
}

int LaylaOSAudioOutput::bytesFree() const
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

int LaylaOSAudioOutput::periodSize() const
{
     return m_periodSize;
}

void LaylaOSAudioOutput::setNotifyInterval(int ms)
{
    m_notifyInterval = ms;
}

int LaylaOSAudioOutput::notifyInterval() const
{
    return m_notifyInterval;
}

qint64 LaylaOSAudioOutput::processedUSecs() const
{
    return qint64(1000000) * m_format.framesForBytes(m_bytesWritten) / m_format.sampleRate();
}

qint64 LaylaOSAudioOutput::elapsedUSecs() const
{
    if (m_state == QAudio::StoppedState)
        return 0;
    else
        return m_startTimeStamp.elapsed() * qint64(1000);
}

QAudio::Error LaylaOSAudioOutput::error() const
{
    return m_error;
}

QAudio::State LaylaOSAudioOutput::state() const
{
    return m_state;
}

void LaylaOSAudioOutput::setFormat(const QAudioFormat &format)
{
    if (m_state == QAudio::StoppedState)
        m_format = format;
}

QAudioFormat LaylaOSAudioOutput::format() const
{
    return m_format;
}

void LaylaOSAudioOutput::setVolume(qreal volume)
{
    m_volume = qBound(qreal(0.0), volume, qreal(1.0));
}

qreal LaylaOSAudioOutput::volume() const
{
    return m_volume;
}

void LaylaOSAudioOutput::setCategory(const QString &category)
{
    m_category = category;
}

QString LaylaOSAudioOutput::category() const
{
    return m_category;
}

void LaylaOSAudioOutput::pullData()
{
    if (m_state == QAudio::StoppedState
            || m_state == QAudio::SuspendedState
            || m_state == QAudio::InterruptedState)
        return;

    const int bytesAvailable = bytesFree();
    const int frames = m_format.framesForBytes(bytesAvailable);

    if (frames == 0 || bytesAvailable < periodSize())
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
        // Got some data to output
        if (m_state != QAudio::ActiveState)
            return;

        const qint64 bytesWritten = write(buffer, bytesRead);
        if (bytesWritten != bytesRead)
            m_source->seek(m_source->pos()-(bytesRead-bytesWritten));

    } else {
        // We're done
        close();
        if (bytesRead != 0)
            setError(QAudio::IOError);
        setState(QAudio::StoppedState);
    }

    if (m_state != QAudio::ActiveState)
        return;

    if (m_notifyInterval > 0 && (m_intervalTimeStamp.elapsed() + m_intervalOffset) > m_notifyInterval) {
        emit notify();
        m_intervalOffset = m_intervalTimeStamp.elapsed() + m_intervalOffset - m_notifyInterval;
        m_intervalTimeStamp.restart();
    }
}

bool LaylaOSAudioOutput::open()
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

    LaylaOSAudioUtils::formatToChannelParams(m_format, &info, QAudio::AudioOutput);

    qDebug() << "LaylaOSAudioOutput::open: " << m_format.sampleRate() << m_format.channelCount() << m_format.sampleSize() << m_format.sampleType();

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
    m_startTimeStamp.restart();
    m_intervalTimeStamp.restart();
    m_intervalOffset = 0;
    m_bytesWritten = 0;

    return true;
}

void LaylaOSAudioOutput::close()
{
    m_timer.stop();

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

void LaylaOSAudioOutput::setError(QAudio::Error error)
{
    if (m_error != error) {
        m_error = error;
        emit errorChanged(error);
    }
}

void LaylaOSAudioOutput::setState(QAudio::State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

qint64 LaylaOSAudioOutput::write(const char *data, qint64 len)
{
    if (m_fd < 0)
        return 0;

    // Make sure we're writing (N * frame) worth of bytes
    const int size = m_format.bytesForFrames(qBound(qint64(0), qint64(bytesFree()), len) / m_format.bytesPerFrame());

    if (size == 0)
        return 0;

    int written = 0;

    if (m_volume < 1.0f) {
        char out[size];
        QAudioHelperInternal::qMultiplySamples(m_volume, m_format, data, out, size);
        written = ::write(m_fd, out, size);
    } else {
        written = ::write(m_fd, data, size);
    }

    if (written > 0) {
        m_bytesWritten += written;
        setError(QAudio::NoError);
        setState(QAudio::ActiveState);
        return written;
    } else {
        close();
        setError(QAudio::FatalError);
        setState(QAudio::StoppedState);
        return 0;
    }
}

void LaylaOSAudioOutput::suspendInternal(QAudio::State suspendState)
{
    m_timer.stop();
    setState(suspendState);
}

void LaylaOSAudioOutput::resumeInternal()
{
    if (m_pushSource) {
        setState(QAudio::IdleState);
    } else {
        setState(QAudio::ActiveState);
        m_timer.start();
    }
}


LaylaOSPushIODevice::LaylaOSPushIODevice(LaylaOSAudioOutput *output)
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
    int retry = 0;
    qint64 written = 0;

    if (m_output->state() == QAudio::ActiveState
     || m_output->state() == QAudio::IdleState) {
        while (written < len) {
            const int writeSize = m_output->write(data + written, len - written);

            if (writeSize <= 0) {
                retry++;
                if (retry > 10)
                    return written;
                else
                    continue;
            }

            retry = 0;
            written += writeSize;
        }
    }

    return written;
}

QT_END_NAMESPACE
