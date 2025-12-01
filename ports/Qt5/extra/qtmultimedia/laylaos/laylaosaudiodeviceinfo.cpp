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

#include "laylaosaudiodeviceinfo.h"
#include "laylaosaudioutils.h"

#include <fcntl.h>
#include <unistd.h>


QT_BEGIN_NAMESPACE

LaylaOSAudioDeviceInfo::LaylaOSAudioDeviceInfo(const QString &deviceName, QAudio::Mode mode)
    : m_name(deviceName),
      m_mode(mode)
{
}

LaylaOSAudioDeviceInfo::~LaylaOSAudioDeviceInfo()
{
}

QAudioFormat LaylaOSAudioDeviceInfo::preferredFormat() const
{
    QAudioFormat format;
    if (m_mode == QAudio::AudioOutput) {
        format.setSampleRate(44100);
        format.setChannelCount(2);
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);
        format.setSampleSize(16);
        format.setCodec(QLatin1String("audio/pcm"));
    } else {
        format.setSampleRate(8000);
        format.setChannelCount(1);
        format.setSampleType(QAudioFormat::UnSignedInt);
        format.setSampleSize(8);
        format.setCodec(QLatin1String("audio/pcm"));
        if (!isFormatSupported(format)) {
            format.setChannelCount(2);
            format.setSampleSize(16);
            format.setSampleType(QAudioFormat::SignedInt);
        }
    }
    return format;
}

bool LaylaOSAudioDeviceInfo::isFormatSupported(const QAudioFormat &format) const
{
    if (!format.codec().startsWith(QLatin1String("audio/pcm")))
        return false;

    int fd = open(DEFAULT_DEVICE_PATH, ((m_mode == QAudio::AudioOutput) ? O_RDONLY : O_WRONLY) | O_CLOEXEC);

    if (fd < 0)
        return false;

    audio_info_t info;

    AUDIO_INITINFO(&info);
    LaylaOSAudioUtils::formatToChannelParams(format, &info, m_mode);

    int res = (ioctl(fd, AUDIO_SETINFO, &info) == 0);

    close(fd);

    return res;
}

QString LaylaOSAudioDeviceInfo::deviceName() const
{
    return m_name;
}

QStringList LaylaOSAudioDeviceInfo::supportedCodecs()
{
    return QStringList() << QLatin1String("audio/pcm");
}

QList<int> LaylaOSAudioDeviceInfo::supportedSampleRates()
{
    return QList<int>() << 8000 << 11025 << 22050 << 44100 << 48000;
}

QList<int> LaylaOSAudioDeviceInfo::supportedChannelCounts()
{
    return QList<int>() << 1 << 2;
}

QList<int> LaylaOSAudioDeviceInfo::supportedSampleSizes()
{
    return QList<int>() << 8 << 16 << 32;
}

QList<QAudioFormat::Endian> LaylaOSAudioDeviceInfo::supportedByteOrders()
{
    /*
     * Currently we only support host endianness.
     */
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
    return QList<QAudioFormat::Endian>() << QAudioFormat::BigEndian;
#else
    return QList<QAudioFormat::Endian>() << QAudioFormat::LittleEndian;
#endif
}

QList<QAudioFormat::SampleType> LaylaOSAudioDeviceInfo::supportedSampleTypes()
{
    /*
     * Currently we only support signed integer samples.
     */
    return QList<QAudioFormat::SampleType>() << QAudioFormat::SignedInt;
}

QT_END_NAMESPACE
