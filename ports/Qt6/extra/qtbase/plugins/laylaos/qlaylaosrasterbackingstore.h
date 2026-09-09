// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSRASTERWINDOWSURFACE_H
#define QLAYLAOSRASTERWINDOWSURFACE_H

#include <qpa/qplatformbackingstore.h>
//#include <QtCore/QMutex>

#include "qlaylaosbuffer.h"

#include <gui/gc.h>
#include <gui/screen.h>

QT_BEGIN_NAMESPACE

class QLaylaOSRasterWindow;

class QLaylaOSRasterBackingStore : public QPlatformBackingStore
{
public:
    explicit QLaylaOSRasterBackingStore(QWindow *window);
    ~QLaylaOSRasterBackingStore();

    QImage::Format nativeToQtFormat(int f);

    QPaintDevice *paintDevice() override;
    void flush(QWindow *window, const QRegion &region, const QPoint &offset) override;
    void resize(const QSize &size, const QRegion &staticContents) override;

    QImage toImage() const override;

private:
    struct gc_t m_gc;
    struct screen_t m_screen;
    QLaylaOSBuffer m_buffer;
    QSize m_bufferSize;
    bool usesNativeFormat;
};

QT_END_NAMESPACE

#endif
