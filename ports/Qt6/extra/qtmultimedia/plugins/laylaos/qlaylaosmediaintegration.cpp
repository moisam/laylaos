// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2021 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosmediaintegration_p.h"
//#include "qlaylaosmediacapturesession_p.h"
//#include "qlaylaosmediarecorder_p.h"
#include "qlaylaosformatinfo_p.h"
//#include "qlaylaosvideodevices_p.h"
#include "qlaylaosvideosink_p.h"
#include "qlaylaosmediaplayer_p.h"
//#include "qlaylaosimagecapture_p.h"
//#include "qlaylaosplatformcamera_p.h"
#include <QtMultimedia/private/qplatformmediaplugin_p.h>

QT_BEGIN_NAMESPACE

class QLaylaOSMediaPlugin : public QPlatformMediaPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QPlatformMediaPlugin_iid FILE "laylaos.json")

public:
    QLaylaOSMediaPlugin()
      : QPlatformMediaPlugin()
    {}

    QPlatformMediaIntegration* create(const QString &name) override
    {
        if (name == u"laylaos")
            return new QLaylaOSMediaIntegration;
        return nullptr;
    }
};

QLaylaOSMediaIntegration::QLaylaOSMediaIntegration() : QPlatformMediaIntegration(QLatin1String("laylaos")) { }

QPlatformMediaFormatInfo *QLaylaOSMediaIntegration::createFormatInfo()
{
    return new QLaylaOSFormatInfo;
}

QPlatformVideoDevices *QLaylaOSMediaIntegration::createVideoDevices()
{
    //return new QLaylaOSVideoDevices(this);
    qDebug() << "QLaylaOSMediaIntegration: createVideoDevices() is not supported yet";
    return nullptr;
}

q23::expected<QPlatformVideoSink *, QString> QLaylaOSMediaIntegration::createVideoSink(QVideoSink *sink)
{
    return new QLaylaOSVideoSink(sink);
}

q23::expected<QPlatformMediaPlayer *, QString> QLaylaOSMediaIntegration::createPlayer(QMediaPlayer *parent)
{
    return new QLaylaOSMediaPlayer(parent);
}

q23::expected<QPlatformMediaCaptureSession *, QString> QLaylaOSMediaIntegration::createCaptureSession()
{
    //return new QQnxMediaCaptureSession();
    qDebug() << "QLaylaOSMediaIntegration: createCaptureSession() is not supported yet";
    return nullptr;
}

q23::expected<QPlatformMediaRecorder *, QString> QLaylaOSMediaIntegration::createRecorder(QMediaRecorder *parent)
{
    //return new QQnxMediaRecorder(parent);
    qDebug() << "QLaylaOSMediaIntegration: createRecorder() is not supported yet";
    return nullptr;
}

q23::expected<QPlatformCamera *, QString> QLaylaOSMediaIntegration::createCamera(QCamera *parent)
{
    //return new QQnxPlatformCamera(parent);
    qDebug() << "QLaylaOSMediaIntegration: createCamera() is not supported yet";
    return nullptr;
}

q23::expected<QPlatformImageCapture *, QString> QLaylaOSMediaIntegration::createImageCapture(QImageCapture *parent)
{
    //return new QQnxImageCapture(parent);
    qDebug() << "QLaylaOSMediaIntegration: createImageCapture() is not supported yet";
    return nullptr;
}

QT_END_NAMESPACE

#include "qlaylaosmediaintegration.moc"
