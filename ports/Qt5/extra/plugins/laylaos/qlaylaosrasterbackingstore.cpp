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

#include "qlaylaosrasterbackingstore.h"
#include "qlaylaoswindow.h"

#include <QDebug>

#include <gui/gc.h>
#include <gui/rgb.h>
#include <gui/client/window.h>

#define GLOB            __global_gui_data

#ifdef __x86_64__
#include <emmintrin.h>
#endif

QT_BEGIN_NAMESPACE

QLaylaOSRasterBackingStore::QLaylaOSRasterBackingStore(QWindow *window)
    : QPlatformBackingStore(window)
    , m_bitmap(nullptr)
{
}

QLaylaOSRasterBackingStore::~QLaylaOSRasterBackingStore()
{
    if(m_bitmap->data) free(m_bitmap->data);
    m_bitmap->data = nullptr;
    free(m_bitmap);
    m_bitmap = nullptr;
}

QPaintDevice *QLaylaOSRasterBackingStore::paintDevice()
{
    if (!m_bufferSize.isEmpty() && m_bitmap)
        return m_buffer.image();

    return nullptr;
}

void QLaylaOSRasterBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    //Q_UNUSED(region);
    //Q_UNUSED(offset);

    if (!window || !m_bitmap)
        return;

    QLaylaOSWindow *targetWindow = static_cast<QLaylaOSWindow*>(window->handle());
    struct window_t *lwin = targetWindow->nativeHandle();

    char *src, *dest;
    int x, x1, x2, y1, y2;
    int w = m_bitmap->width;
    int h = m_bitmap->height;
    int rshift, bshift, gshift;
    int bpl = (((int)m_bitmap->width * 32) + 7) / 8;

    bpl = (bpl + 4) & ~3;   // align to 4 bytes

    rshift = GLOB.screen.red_pos;
    gshift = GLOB.screen.green_pos;
    bshift = GLOB.screen.blue_pos;

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

        src = ((char *)m_bitmap->data + (bpl * y1) + (4 * x1));
        dest = ((char *)lwin->gc->buffer + (lwin->gc->pitch * y1) + (4 * x1));

        if (lwin->gc->pixel_width == 1) {
            // destination is 8bit RGB

            uint32_t *src32;
            uint8_t *dest8;

            for ( ; y1 <= y2; y1++) {
                src32 = (uint32_t *)src;
                dest8 = (uint8_t *)dest;

                for (x = x1; x <= x2; x++) {
                    // source pixels are formatted as 0xffRRGGBB
                    *dest8 = gc_comp_to_rgb8(lwin->gc,
                                             (*src32 & 0xff),
                                             ((*src32 & 0xff00) >> 8),
                                             ((*src32 & 0xff00) >> 16));
                    src32++;
                    dest8++;
                }

                src += bpl;
                dest += lwin->gc->pitch;
            }
        } else if (lwin->gc->pixel_width == 2) {
            // destination is 16bit RGB

            uint32_t *src32;
            uint16_t *dest16;

            for ( ; y1 <= y2; y1++) {
                src32 = (uint32_t *)src;
                dest16 = (uint16_t *)dest;

                for (x = x1; x <= x2; x++) {
                    // source pixels are formatted as 0xffRRGGBB
                    *dest16 = gc_comp_to_rgb16(lwin->gc,
                                               (*src32 & 0xff),
                                               ((*src32 & 0xff00) >> 8),
                                               ((*src32 & 0xff00) >> 16));
                    src32++;
                    dest16++;
                }

                src += bpl;
                dest += lwin->gc->pitch;
            }
        } else if (lwin->gc->pixel_width == 3) {
            // destination is 24bit RGB

            uint32_t tmp, j;
            uint32_t *src32;

            for ( ; y1 <= y2; y1++) {
                src32 = (uint32_t *)src;

                for (j = 0, x = x1; x <= x2; x++, j += 3) {
                    // source pixels are formatted as 0xffRRGGBB
                    tmp = ((*src32 & 0xff) << rshift) |
                          (((*src32 & 0xff00) >> 8) << gshift) |
                          (((*src32 & 0xff0000) >> 16) << bshift);
                    dest[j + 0] = tmp & 0xff;
                    dest[j + 1] = (tmp >> 8) & 0xff;
                    dest[j + 2] = (tmp >> 16) & 0xff;
                    src32++;
                }

                src += bpl;
                dest += lwin->gc->pitch;
            }
        } else {
            // destination is 32bit RGBA
            uint32_t *src32, *dest32;

#ifdef __x86_64__
            __m128i rshift128 = _mm_set_epi32(0, 0, 0, rshift);
            __m128i gshift128 = _mm_set_epi32(0, 0, 0, gshift);
            __m128i bshift128 = _mm_set_epi32(0, 0, 0, bshift);
            __m128i ff_mask128 = _mm_set_epi32(0xff, 0xff, 0xff, 0xff);
            __m128i src128, dest128, r128, g128;
#endif

            for ( ; y1 <= y2; y1++) {
                src32 = (uint32_t *)src;
                dest32 = (uint32_t *)dest;
                x = x1;

#ifdef __x86_64__
                for ( ; x < x2 - 3; x += 4) {
                    src128 = _mm_loadu_si128((__m128i const *)src32);

                    // source pixels are formatted as 0xffRRGGBB
                    
                    // get red by masking the lower byte of each pixel
                    // and then shift to its destination position
                    r128 = _mm_and_si128(src128, ff_mask128);
                    r128 = _mm_sll_epi32(r128, rshift128);

                    // get green by shifting right, then mask the lower byte
                    // of each pixel and shift to its destination position
                    g128 = _mm_srli_si128(src128, 1);
                    g128 = _mm_and_si128(g128, ff_mask128);
                    g128 = _mm_sll_epi32(g128, gshift128);

                    // same for blue
                    dest128 = _mm_srli_si128(src128, 2);
                    dest128 = _mm_and_si128(dest128, ff_mask128);
                    dest128 = _mm_sll_epi32(dest128, bshift128);

                    // add in red and green
                    dest128 = _mm_or_si128(dest128, g128);
                    dest128 = _mm_or_si128(dest128, r128);

                    // store the result and move on
                    _mm_storeu_si128((__m128i *)dest32, dest128);

                    dest32 += 4;
                    src32 += 4;
                }
#endif

                for ( ; x <= x2; x++) {
                    *dest32 = ((*src32 & 0xff) << rshift) |
                              (((*src32 & 0xff00) >> 8) << gshift) |
                              (((*src32 & 0xff0000) >> 16) << bshift);
                    dest32++;
                    src32++;
                }

                src += bpl;
                dest += lwin->gc->pitch;
            }
        }
    }

    /*
    gc_blit_bitmap(lwin->gc, m_bitmap, 0, 0, 0, 0,
                             m_bitmap->width, m_bitmap->height);
    */

    window_invalidate(lwin);
}

void QLaylaOSRasterBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);

    if (m_bufferSize == size)
        return;

    int bpl = (((int)size.width() * 32) + 7) / 8;

    bpl = (bpl + 4) & ~3;   // align to 4 bytes

    if (m_bitmap) {
        if(m_bitmap->data) free(m_bitmap->data);
        free(m_bitmap);
        m_bitmap = nullptr;
    }

    if (!(m_bitmap = (struct bitmap32_t *)malloc(sizeof(struct bitmap32_t)))) {
        return;
    }

    if (!(m_bitmap->data = (uint32_t *)malloc(bpl * size.height()))) {
        free(m_bitmap);
        m_bitmap = nullptr;
        return;
    }

    m_bitmap->width = size.width();
    m_bitmap->height = size.height();
    m_buffer = QLaylaOSBuffer(m_bitmap);
    m_bufferSize = size;
}

QImage QLaylaOSRasterBackingStore::toImage() const
{
    if (!m_bufferSize.isEmpty() && m_bitmap)
        return *m_buffer.image();

    return QImage();
}

QT_END_NAMESPACE
