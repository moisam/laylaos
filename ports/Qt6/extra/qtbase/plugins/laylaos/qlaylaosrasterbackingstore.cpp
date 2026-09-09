// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosrasterbackingstore.h"
#include "qlaylaoswindow.h"

#include <QDebug>

#include <gui/gc.h>
#include <gui/rgb.h>
#include <gui/screen.h>
#include <gui/client/window.h>

#define GLOB            __global_gui_data

#ifdef __x86_64__
#include <emmintrin.h>
#endif

QT_BEGIN_NAMESPACE

QLaylaOSRasterBackingStore::QLaylaOSRasterBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
    , usesNativeFormat(false)
{
    //qDebug() << "QLaylaOSRasterBackingStore::QLaylaOSRasterBackingStore:";
    m_gc.buffer = nullptr;
    m_gc.buffer_size = 0;
}

QLaylaOSRasterBackingStore::~QLaylaOSRasterBackingStore()
{
    //qDebug() << "QLaylaOSRasterBackingStore::~QLaylaOSRasterBackingStore:";
    if(m_gc.buffer) free(m_gc.buffer);
    m_gc.buffer = nullptr;
}

QPaintDevice *QLaylaOSRasterBackingStore::paintDevice()
{
    //qDebug() << "QLaylaOSRasterBackingStore::paintDevice:";
    if (!m_bufferSize.isEmpty() && m_gc.buffer)
        return m_buffer.image();

    return nullptr;
}

void QLaylaOSRasterBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    //qDebug() << "QLaylaOSRasterBackingStore::flush:";
    if (!window || !m_gc.buffer)
        return;

    QLaylaOSWindow *targetWindow = static_cast<QLaylaOSWindow*>(window->handle());
    struct window_t *lwin = targetWindow->nativeHandle();

    char *src, *dest;
    int x1, x2, y1, y2;
    int w = m_gc.w;
    int h = m_gc.h;
    int lwin_bpp = lwin->gc->pixel_width;
    bool transparent = targetWindow->hasTransparentBackground();

    if (w != lwin->w || h != lwin->h) {
        return;
    }

    if (transparent && !(lwin->flags & WINDOW_TRANSPARENT)) {
        qDebug() << "QLaylaOSRasterBackingStore::flush: transparent " << transparent;
        window_set_transparent(lwin, 1);
    }

    for (const QRect &rect : region) {
        x1 = rect.x() + offset.x();
        x2 = rect.right() + offset.x();
        y1 = rect.y() + offset.y();
        y2 = rect.bottom() + offset.y();

        if (x1 < 0) x1 = 0;
        if (x2 < 0) x2 = 0;
        if (y1 < 0) y1 = 0;
        if (y2 < 0) y2 = 0;

        if (x1 >= w) x1 = w - 1;
        if (x2 >= w) x2 = w - 1;
        if (y1 >= h) y1 = h - 1;
        if (y2 >= h) y2 = h - 1;

        if (x1 >= x2 || y1 >= y2) continue;

        // source buffer uses same pixel format as destination, so copy directly
        if (usesNativeFormat) {
            //fprintf(stderr, "Using native pixel format\n");
            int bytes_tocopy = (x2 - x1 + 1) * lwin_bpp;

            src = ((char *)m_gc.buffer + (m_gc.pitch * y1) + (m_gc.pixel_width * x1));
            dest = ((char *)lwin->gc->buffer + (lwin->gc->pitch * y1) + (lwin_bpp * x1));

            for ( ; y1 <= y2; y1++) {
                __builtin_memcpy(dest, src, bytes_tocopy);
                src += m_gc.pitch;
                dest += lwin->gc->pitch;
            }

            continue;
        }

        // source buffer is RGBA while destination is something else
        gc_copy_part_gc(lwin->gc, &m_gc, x1, y1, x1, y1, x2 - x1 + 1, y2 - y1 + 1, 0);
    }

    window_invalidate(lwin);
    //qDebug() << "QLaylaOSRasterBackingStore::flush: done";
}

QImage::Format QLaylaOSRasterBackingStore::nativeToQtFormat(int f)
{
    switch (f) {
        //case SCREEN_COLOR_FORMAT_ARGB: return QImage::Format_ARGB32;
        //case SCREEN_COLOR_FORMAT_ABGR: return QImage::Format_RGBA8888;
        //case SCREEN_COLOR_FORMAT_RGB: return QImage::Format_BGR888;
        //case SCREEN_COLOR_FORMAT_BGR: return QImage::Format_RGB888;
        default: return QImage::Format_Invalid;
    }
}

void QLaylaOSRasterBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    //qDebug() << "QLaylaOSRasterBackingStore::resize:";
    Q_UNUSED(staticContents);

    if (m_bufferSize == size)
        return;

    m_gc.w = size.width(); 
    m_gc.h = size.height(); 
    m_gc.screen = &m_screen;

    QImage::Format format = nativeToQtFormat(get_screen_color_format(&GLOB.screen));
    //qDebug() << "QLaylaOSRasterBackingStore::resize: format " << format;

    if (format == QImage::Format_Invalid) {
        usesNativeFormat = false;
        format = QImage::Format_RGB32;
        m_gc.pixel_width = 4;
        m_screen.pixel_width = 4;
        m_screen.red_pos = 16;
        m_screen.green_pos = 8;
        m_screen.blue_pos = 0;
        m_screen.rgb_mode = 1;
    } else {
        usesNativeFormat = true;
        m_gc.pixel_width = GLOB.screen.pixel_width;
        m_screen.pixel_width = GLOB.screen.pixel_width;
        m_screen.red_pos = GLOB.screen.red_pos;
        m_screen.green_pos = GLOB.screen.green_pos;
        m_screen.blue_pos = GLOB.screen.blue_pos;
        m_screen.rgb_mode = GLOB.screen.rgb_mode;
    }

    m_gc.pitch = ((int)size.width() * m_gc.pixel_width);
    //qDebug() << "QLaylaOSRasterBackingStore::resize: m_gc.pitch " << m_gc.pitch << ", m_gc.buffer " << m_gc.buffer;

    if(m_gc.buffer) free(m_gc.buffer);
    m_gc.buffer = nullptr;

    m_gc.buffer_size = m_gc.pitch * m_gc.h;

    if (!(m_gc.buffer = (uint8_t *)malloc(m_gc.buffer_size))) {
        m_gc.buffer_size = 0;
        return;
    }

    m_buffer = QLaylaOSBuffer((void *)m_gc.buffer, format, m_gc.w, m_gc.h, m_gc.pitch);
    m_bufferSize = size;
}

QImage QLaylaOSRasterBackingStore::toImage() const
{
    //qDebug() << "QLaylaOSRasterBackingStore::toImage:";
    if (!m_bufferSize.isEmpty() && m_gc.buffer)
        return *m_buffer.image();

    return QImage();
}

QT_END_NAMESPACE
