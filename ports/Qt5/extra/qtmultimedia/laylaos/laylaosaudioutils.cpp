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

#include "laylaosaudioutils.h"

QT_BEGIN_NAMESPACE

void LaylaOSAudioUtils::formatToChannelParams(const QAudioFormat &format, audio_info_t *info, QAudio::Mode mode)
{
    bool iscapture = !(mode == QAudio::AudioOutput);
    struct audio_prinfo *prinfo = iscapture ? &info->play : &info->record;

    info->mode = (iscapture ? AUMODE_RECORD : AUMODE_PLAY);
    info->hiwat = 5;
    info->lowat = 3;

    prinfo->sample_rate = format.sampleRate();
    prinfo->channels = format.channelCount();

    switch (format.sampleSize()) {
    case 8:
        switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            prinfo->encoding = AUDIO_ENCODING_SLINEAR;
            prinfo->precision = 8;
            break;
        case QAudioFormat::UnSignedInt:
            prinfo->encoding = AUDIO_ENCODING_ULINEAR;
            prinfo->precision = 8;
            break;
        default:
            break;
        }
        break;

    case 16:
        switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                prinfo->encoding = AUDIO_ENCODING_SLINEAR_LE;
                prinfo->precision = 16;
            } else {
                prinfo->encoding = AUDIO_ENCODING_SLINEAR_BE;
                prinfo->precision = 16;
            }
            break;
        case QAudioFormat::UnSignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                prinfo->encoding = AUDIO_ENCODING_ULINEAR_LE;
                prinfo->precision = 16;
            } else {
                prinfo->encoding = AUDIO_ENCODING_ULINEAR_BE;
                prinfo->precision = 16;
            }
            break;
        default:
            break;
        }
        break;

    case 32:
        switch (format.sampleType()) {
        case QAudioFormat::SignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                prinfo->encoding = AUDIO_ENCODING_SLINEAR_LE;
                prinfo->precision = 32;
            } else {
                prinfo->encoding = AUDIO_ENCODING_SLINEAR_BE;
                prinfo->precision = 32;
            }
            break;
        case QAudioFormat::UnSignedInt:
            if (format.byteOrder() == QAudioFormat::LittleEndian) {
                prinfo->encoding = AUDIO_ENCODING_ULINEAR_LE;
                prinfo->precision = 32;
            } else {
                prinfo->encoding = AUDIO_ENCODING_ULINEAR_BE;
                prinfo->precision = 32;
            }
            break;
        case QAudioFormat::Float:
            break;
        default:
            break;
        }
        break;
    }
}

QT_END_NAMESPACE
