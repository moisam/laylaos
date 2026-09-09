// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosaudioutils_p.h"

QT_BEGIN_NAMESPACE

namespace LaylaOSAudioUtils
{

void formatToChannelParams(const QAudioFormat &format, audio_info_t *info, QAudioDevice::Mode mode)
{
    bool iscapture = !(mode == QAudioDevice::Output);
    struct audio_prinfo *prinfo = iscapture ? &info->play : &info->record;

    info->mode = (iscapture ? AUMODE_RECORD : AUMODE_PLAY);
    info->hiwat = 5;
    info->lowat = 3;

    prinfo->sample_rate = format.sampleRate();
    prinfo->channels = format.channelCount();

    switch (format.sampleFormat()) {
        case QAudioFormat::UInt8:
            prinfo->encoding = AUDIO_ENCODING_ULINEAR;
            prinfo->precision = 8;
            break;

        case QAudioFormat::Int16:
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            prinfo->encoding = AUDIO_ENCODING_SLINEAR_LE;
            prinfo->precision = 16;
#else
            prinfo->encoding = AUDIO_ENCODING_SLINEAR_BE;
            prinfo->precision = 16;
#endif
        break;

        case QAudioFormat::Int32:
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            prinfo->encoding = AUDIO_ENCODING_SLINEAR_LE;
            prinfo->precision = 32;
#else
            prinfo->encoding = AUDIO_ENCODING_SLINEAR_BE;
            prinfo->precision = 32;
#endif
        break;

    case QAudioFormat::Float:
        qDebug() << "LaylaOSAudioUtils: float sample format is not supported";
        break;

    case QAudioFormat::Unknown:
        qDebug() << "LaylaOSAudioUtils: unknown sample format";
        break;
    }
}

} // namespace LaylaOSAudioUtils

QT_END_NAMESPACE
