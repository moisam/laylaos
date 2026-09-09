// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosbuffer.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

QLaylaOSBuffer::QLaylaOSBuffer()
    : m_buffer(nullptr)
{
}

QLaylaOSBuffer::QLaylaOSBuffer(void *buffer, QImage::Format format, int w, int h, int stride)
    : m_buffer(buffer)
{
    // wrap buffer in an image
    m_image = QImage((uchar *)buffer, w, h, stride, format);
}

QLaylaOSBuffer::~QLaylaOSBuffer()
{
    m_buffer = nullptr;
}

const QImage *QLaylaOSBuffer::image() const
{
    return (m_buffer != nullptr) ? &m_image : nullptr;
}

QImage *QLaylaOSBuffer::image()
{
    return (m_buffer != nullptr) ? &m_image : nullptr;
}

QRect QLaylaOSBuffer::rect() const
{
    return m_image.rect();
}

QT_END_NAMESPACE
