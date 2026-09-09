// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2016 Research In Motion
// Copyright (C) 2021 The Qt Company
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosvideosink_p.h"

QT_BEGIN_NAMESPACE

QLaylaOSVideoSink::QLaylaOSVideoSink(QVideoSink *parent)
    : QPlatformVideoSink(parent)
{
}

void QLaylaOSVideoSink::setRhi(QRhi *rhi)
{
    m_rhi = rhi;
}

QRhi *QLaylaOSVideoSink::rhi() const
{
    return m_rhi;
}

QT_END_NAMESPACE

#include "moc_qlaylaosvideosink_p.cpp"
