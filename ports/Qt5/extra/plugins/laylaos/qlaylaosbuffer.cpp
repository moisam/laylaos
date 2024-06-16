/***************************************************************************
**
** Copyright (C) 2024 Mohammed Isam <mohammed_isam1984@yahoo.com>
** Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qlaylaosbuffer.h"

#include <QDebug>

QT_BEGIN_NAMESPACE

QLaylaOSBuffer::QLaylaOSBuffer()
    : m_buffer(nullptr)
{
}

QLaylaOSBuffer::QLaylaOSBuffer(struct bitmap32_t *buffer)
    : m_buffer(buffer)
{
    int bpl = (((int)m_buffer->width * 32) + 7) / 8;

    bpl = (bpl + 4) & ~3;   // align to 4 bytes

    // wrap buffer in an image
    m_image = QImage((uchar*)(m_buffer->data),
                     (int)m_buffer->width, (int)m_buffer->height,
                     bpl,
                     QImage::Format_RGBA8888);
}

struct bitmap32_t *QLaylaOSBuffer::nativeBuffer() const
{
    return m_buffer;
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
