// Copyright (C) 2024-2026 Mohammed Isam
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaosscreen.h"
#include "qlaylaoscursor.h"

#include <qpa/qwindowsysteminterface.h>

#include <gui/gui-global.h>
#include <gui/screen.h>
#include <gui/client/window.h>
#include <gui/client/screenshot.h>
#include <gui/rect.h>

#include <emmintrin.h>

#define GLOB            __global_gui_data

void QLaylaOSScreen::checkScreenRes()
{
    Rect r;

    if(!get_desktop_bounds(&r))
        m_usableScreen = QRect(0, 0, GLOB.screen.w, GLOB.screen.h);
    else
        m_usableScreen = QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
}

QLaylaOSScreen::QLaylaOSScreen()
    : m_cursor(new QLaylaOSCursor)
{
    checkScreenRes();
}

QLaylaOSScreen::~QLaylaOSScreen()
{
    delete m_cursor;
    m_cursor = nullptr;
}

static void rgba_to_argb_sse(uint32_t *p, int width, int height)
{
    size_t total_pixels = (size_t)width * (size_t)height;
    size_t x = 0;

    __m128i v_alpha_mask = _mm_set1_epi32(0xFF000000);

    for (; x <= total_pixels - 4; x += 4) {
        __m128i pixels = _mm_loadu_si128((const __m128i *)&p[x]);
        __m128i shifted_pixels = _mm_srli_epi32(pixels, 8);
        __m128i final_argb = _mm_or_si128(shifted_pixels, v_alpha_mask);
        _mm_storeu_si128((__m128i *)&p[x], final_argb);
    }

    for (; x < total_pixels; x++) {
        p[x] = (p[x] >> 8) | 0xff000000;
    }
}

QPixmap QLaylaOSScreen::grabWindow(WId winId, int x, int y, int width, int height) const
{
    struct window_t *win = (struct window_t *)winId;
    winid_t screenshot_winid = 0;

    if (width == 0 || height == 0)
        return QPixmap();

    const QImage::Format format = QImage::Format_ARGB32;
    uint32_t *screenshot;
    int absoluteX, absoluteY;

    //qDebug() << "QLaylaOSScreen::grabWindow: 1 " << winId << x << "," << y << " " << width << "x" << height;

    if (win) {
        int hasframe = !(win->flags & WINDOW_NODECORATION);
        int winw = win->w;
        int winh = win->h;

        if (hasframe) {
            winw += (2 * WINDOW_BORDERWIDTH);
            winh += WINDOW_TITLEHEIGHT + WINDOW_BORDERWIDTH;
            x += WINDOW_BORDERWIDTH;
            y += WINDOW_TITLEHEIGHT;
        }

        screenshot_winid = win->winid;
        absoluteX = x;
        absoluteY = y;

        if (width < 0) width = winw - x;
        if (height < 0) height = winh - y;
    } else {
        absoluteX = x;
        absoluteY = y;

        if (width < 0) width = GLOB.screen.w - x;
        if (height < 0) height = GLOB.screen.h - y;
    }

    //qDebug() << "QLaylaOSScreen::grabWindow: 2 " << winId << absoluteX << "," << absoluteY << " " << width << "x" << height;

    screenshot = screenshot_get(screenshot_winid, absoluteX, absoluteY, width, height);

    if (!screenshot)
        return QPixmap();

    /* convert from native RGBA to ARGB pixels */
    rgba_to_argb_sse(screenshot, width, height);
    /*
    p = screenshot;
    for (x = 0; x < width * height; x++, p++) {
        *p = (*p >> 8) | 0xff000000;
    }
    */

    QImage image((uchar*)screenshot, width, height, width * 4, format, [](void* info)
        {
            screenshot_free((uint32_t *)info);
        }, 
        screenshot // Passed into the lambda as the 'info' pointer
    );

    return QPixmap::fromImage(image);
}

void QLaylaOSScreen::handleScreenResChange()
{
    if(!get_screen_info(&GLOB.screen))
    {
        return;
    }

    checkScreenRes();

    QRect m_geometry = geometry();
    QRect m_availableGeometry = availableGeometry();

    QWindowSystemInterface::handleScreenGeometryChange(this->screen(), m_geometry, m_availableGeometry);
}

QRect QLaylaOSScreen::geometry() const
{
    return QRect(0, 0, GLOB.screen.w, GLOB.screen.h);
}

QRect QLaylaOSScreen::availableGeometry() const
{
    return m_usableScreen;
}

int QLaylaOSScreen::depth() const
{
    switch (format()) {
    case QImage::Format_Invalid:
        return 0;
        break;
    case QImage::Format_MonoLSB:
        return 1;
        break;
    case QImage::Format_Indexed8:
        return 8;
        break;
    case QImage::Format_RGB16:
    case QImage::Format_RGB555:
        return 16;
        break;
    case QImage::Format_RGB888:
    case QImage::Format_BGR888:
        return 24;
        break;
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_RGBA8888:
    default:
        return 32;
        break;
    }
}

QImage::Format QLaylaOSScreen::format() const
{
    switch (get_screen_color_format(&GLOB.screen)) {
        //case SCREEN_COLOR_FORMAT_ARGB: return QImage::Format_ARGB32;
        //case SCREEN_COLOR_FORMAT_ABGR: return QImage::Format_RGBA8888;
        //case SCREEN_COLOR_FORMAT_RGB: return QImage::Format_BGR888;
        //case SCREEN_COLOR_FORMAT_BGR: return QImage::Format_RGB888;
        default: return QImage::Format_RGB32;       // XXX:
    }
}

QPlatformCursor *QLaylaOSScreen::cursor() const
{
    return m_cursor;
}

QSizeF QLaylaOSScreen::physicalSize() const
{
    return QSizeF(476, 268); // Approximate sizing for a standard 21.5-inch 16:9 monitor
}

QDpi QLaylaOSScreen::logicalBaseDpi() const
{
    return QDpi(96, 96);
}

QDpi QLaylaOSScreen::logicalDpi() const
{
    return QDpi(96, 96); 
}

Qt::ScreenOrientation QLaylaOSScreen::orientation() const
{
    return Qt::PrimaryOrientation;
}

qreal QLaylaOSScreen::devicePixelRatio() const
{
    return qreal(1.0);
}

qreal QLaylaOSScreen::refreshRate() const
{
    return 60.0;        // assume 60 Hz
}

QT_END_NAMESPACE
