// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// Copyright (C) 2021 The Qt Company
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosaudiodevice_p.h"
#include "qlaylaosaudioutils_p.h"

#include <array>

#include <fcntl.h>
#include <unistd.h>

using namespace LaylaOSAudioUtils;

QT_BEGIN_NAMESPACE

namespace {

QAudioDevicePrivate::AudioDeviceFormat
createLaylaOSAudioDeviceFormatFromDeviceName(QAudioDevice::Mode mode)
{
    QAudioDevicePrivate::AudioDeviceFormat format;

    if (mode == QAudioDevice::Output) {
        format.preferredFormat.setSampleRate(44100);
        format.preferredFormat.setChannelCount(2);
        format.preferredFormat.setSampleFormat(QAudioFormat::Int16);
    } else {
        format.preferredFormat.setSampleRate(8000);
        format.preferredFormat.setChannelCount(1);
        format.preferredFormat.setSampleFormat(QAudioFormat::UInt8);
    }

    format.minimumChannelCount = 1;
    format.maximumChannelCount = 2;
    format.minimumSampleRate = 8000;
    format.maximumSampleRate = 48000;

    format.supportedSampleFormats << QAudioFormat::UInt8;
    format.supportedSampleFormats << QAudioFormat::Int16;
    format.supportedSampleFormats << QAudioFormat::Int32;

    return format;
}

} // namespace

LaylaOSAudioDeviceInfo::LaylaOSAudioDeviceInfo(const QByteArray &deviceName, QAudioDevice::Mode mode)
    : QAudioDevicePrivate(deviceName, mode, QString::fromUtf8(deviceName),
                          deviceName.contains("Preferred"),
                          createLaylaOSAudioDeviceFormatFromDeviceName(mode))
{
}

LaylaOSAudioDeviceInfo::~LaylaOSAudioDeviceInfo()
{
}

bool LaylaOSAudioDeviceInfo::isFormatSupported(const QAudioFormat &format) const
{
    int fd = open(DEFAULT_DEVICE_PATH, ((mode == QAudioDevice::Output) ? O_RDONLY : O_WRONLY) | O_CLOEXEC);
    qDebug() << "LaylaOSAudioDeviceInfo::isFormatSupported: format " << format;

    if (fd < 0)
        return false;

    audio_info_t info;

    AUDIO_INITINFO(&info);
    formatToChannelParams(format, &info, mode);

    int res = (ioctl(fd, AUDIO_SETINFO, &info) == 0);

    close(fd);

    return res;
}

QT_END_NAMESPACE
