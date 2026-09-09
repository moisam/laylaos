// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef LAYLAOSAUDIOUTILS_H
#define LAYLAOSAUDIOUTILS_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtMultimedia/private/qaudiosystem_p.h>
#include <QtMultimedia/private/qaudiodevice_p.h>

#include <memory>
#include <optional>

#include <sys/audioio.h>

QT_BEGIN_NAMESPACE

namespace LaylaOSAudioUtils
{
    void formatToChannelParams(const QAudioFormat &format, audio_info_t *info, QAudioDevice::Mode mode);
}

QT_END_NAMESPACE

#endif
