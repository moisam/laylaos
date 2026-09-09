// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosaudiodevices_p.h"
#include "qmediadevices.h"
#include "private/qcameradevice_p.h"
#include "qcameradevice.h"

#include "qlaylaosaudiosource_p.h"
#include "qlaylaosaudiosink_p.h"
#include "qlaylaosaudiodevice_p.h"

#include <qdir.h>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

QLaylaOSAudioDevices::QLaylaOSAudioDevices()
    : QPlatformAudioDevices()
{
}

QList<QAudioDevice> QLaylaOSAudioDevices::findAudioInputs() const
{
    QList<QAudioDevice> devices;

    devices << QAudioDevicePrivate::createQAudioDevice(
                    std::make_unique<LaylaOSAudioDeviceInfo>("dsp", QAudioDevice::Input));

    return devices;
}

QList<QAudioDevice> QLaylaOSAudioDevices::findAudioOutputs() const
{
    QList<QAudioDevice> devices;

    devices << QAudioDevicePrivate::createQAudioDevice(
                    std::make_unique<LaylaOSAudioDeviceInfo>("dsp", QAudioDevice::Output));

    return devices;
}

QPlatformAudioSource *QLaylaOSAudioDevices::createAudioSource(const QAudioDevice &deviceInfo,
                                                              const QAudioFormat &fmt,
                                                              QObject *parent)
{
    return new QLaylaOSAudioSource(deviceInfo, fmt, parent);
}

QPlatformAudioSink *QLaylaOSAudioDevices::createAudioSink(const QAudioDevice &deviceInfo,
                                                          const QAudioFormat &fmt,
                                                          QObject *parent)
{
    return new QLaylaOSAudioSink(deviceInfo, fmt, parent);
}

QT_END_NAMESPACE
