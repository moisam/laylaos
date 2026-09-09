// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSBUFFER_H
#define QLAYLAOSBUFFER_H

#include <QtGui/QImage>

#include <gui/gui.h>
#include <gui/bitmap.h>

QT_BEGIN_NAMESPACE

class QLaylaOSBuffer
{
public:
    QLaylaOSBuffer();
    QLaylaOSBuffer(void *buffer, QImage::Format format, int w, int h, int stride);
    ~QLaylaOSBuffer();

    const QImage *image() const;
    QImage *image();

    QRect rect() const;

private:
    void *m_buffer;
    QImage m_image;
};

QT_END_NAMESPACE

#endif
