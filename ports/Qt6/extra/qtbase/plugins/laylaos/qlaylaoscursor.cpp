// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qlaylaoscursor.h"
#include "qlaylaoswindow.h"
#include "qlaylaosintegration.h"
#include "qlaylaossocketmonitor.h"

#include <private/qguiapplication_p.h>

#include <QBitmap>
#include <QPixmap>

int QLaylaOSCursor::cachedMouseX = -1;
int QLaylaOSCursor::cachedMouseY = -1;

QLaylaOSCursor::QLaylaOSCursor()
#ifndef QT_NO_CURSOR
    : m_globalAccessCounter(0)
#endif
{
    m_systemCurIds.insert(Qt::ArrowCursor, CURSOR_NORMAL);
    m_systemCurIds.insert(Qt::UpArrowCursor, CURSOR_UP);
    m_systemCurIds.insert(Qt::CrossCursor, CURSOR_FLEUR);
    m_systemCurIds.insert(Qt::WaitCursor, CURSOR_WAITING);
    m_systemCurIds.insert(Qt::IBeamCursor, CURSOR_IBEAM);
    m_systemCurIds.insert(Qt::SizeVerCursor, CURSOR_NS);
    m_systemCurIds.insert(Qt::SizeHorCursor, CURSOR_WE);
    m_systemCurIds.insert(Qt::SizeBDiagCursor, CURSOR_NESW);
    m_systemCurIds.insert(Qt::SizeFDiagCursor, CURSOR_NWSE);
    m_systemCurIds.insert(Qt::SizeAllCursor, CURSOR_FLEUR);
    m_systemCurIds.insert(Qt::BlankCursor, CURSOR_NONE);
    m_systemCurIds.insert(Qt::SplitVCursor, CURSOR_ROWRESIZE);
    m_systemCurIds.insert(Qt::SplitHCursor, CURSOR_COLRESIZE);
    m_systemCurIds.insert(Qt::PointingHandCursor, CURSOR_POINTING_HAND);
    m_systemCurIds.insert(Qt::ForbiddenCursor, CURSOR_FORBIDDEN);
    m_systemCurIds.insert(Qt::OpenHandCursor, CURSOR_OPEN_HAND);
    m_systemCurIds.insert(Qt::ClosedHandCursor, CURSOR_CLOSED_HAND);
    m_systemCurIds.insert(Qt::WhatsThisCursor, CURSOR_HELP);
    m_systemCurIds.insert(Qt::BusyCursor, CURSOR_ARROW_WAITING);
    m_systemCurIds.insert(Qt::DragMoveCursor, CURSOR_DND_MOVE);
    m_systemCurIds.insert(Qt::DragLinkCursor, CURSOR_DND_LINK);
    m_systemCurIds.insert(Qt::DragCopyCursor, CURSOR_DND_COPY);

    struct cursor_info_t curinfo = { 0, 0, 0, 0, 0, };
    
    cursor_get_info(&curinfo);

    cachedMouseX = curinfo.x;
    cachedMouseY = curinfo.y;
}

QLaylaOSCursor::~QLaylaOSCursor()
{
    for (const auto &node : m_customCursorCache) {
        cursor_free(node.cursorId);
    }
}

#ifndef QT_NO_CURSOR

uint64_t QLaylaOSCursor::hashCursorBitmap(const QImage &image, const QPoint &hotspot)
{
    uint64_t hash = 14695981039346656037ULL; // FNV-1a 64-bit offset basis
    
    // Hash the image dimensions and hotspot
    hash = (hash ^ image.width()) * 1099511628211ULL;
    hash = (hash ^ image.height()) * 1099511628211ULL;
    hash = (hash ^ hotspot.x()) * 1099511628211ULL;
    hash = (hash ^ hotspot.y()) * 1099511628211ULL;

    // Hash the underlying raw pixel data
    const uchar *pixels = image.constBits();
    int byteCount = image.sizeInBytes();
    
    for (int i = 0; i < byteCount; ++i) {
        hash = (hash ^ pixels[i]) * 1099511628211ULL;
    }

    return hash;
}

void QLaylaOSCursor::evictOldestCacheNode()
{
    if (m_customCursorCache.isEmpty()) return;

    uint64_t oldestKey = 0;
    uint64_t oldestTick = std::numeric_limits<uint64_t>::max();
    uint32_t curIdToDestroy = 0;

    // Find the least recently accessed node
    QHash<uint64_t, QLaylaOSCursorCacheNode>::const_iterator it;
    for (it = m_customCursorCache.constBegin(); it != m_customCursorCache.constEnd(); ++it) {
        if (it.value().lastUsedTick < oldestTick) {
            oldestTick = it.value().lastUsedTick;
            oldestKey = it.key();
            curIdToDestroy = it.value().cursorId;
        }
    }

    if (curIdToDestroy > 0) {
        cursor_free(curIdToDestroy);
    }

    m_customCursorCache.remove(oldestKey);
}

curid_t QLaylaOSCursor::uploadCursorToServer(const QImage &image, const QPoint &hotspot)
{
    QImage srcImage = image.convertToFormat(QImage::Format_RGBA8888);

    int width = srcImage.width();
    int height = srcImage.height();

    QImage destImage(width, height, QImage::Format_RGBA8888);

    for (int y = 0; y < height; ++y) {
        const uchar *srcLine = srcImage.scanLine(y);
        uchar *destLine = destImage.scanLine(y);

        for (int x = 0; x < width; ++x) {
            uchar r = srcLine[x * 4 + 0];
            uchar g = srcLine[x * 4 + 1];
            uchar b = srcLine[x * 4 + 2];
            uchar a = srcLine[x * 4 + 3];

            // Target Layout: Alpha in byte 0, Blue in byte 1, Green in byte 2, Red in byte 3
            destLine[x * 4 + 0] = a;
            destLine[x * 4 + 1] = b;
            destLine[x * 4 + 2] = g;
            destLine[x * 4 + 3] = r;
        }
    }

    const uchar *rawPixelBuffer = destImage.bits();

    curid_t newId = cursor_load(width, height, hotspot.x(), hotspot.y(), (uint32_t *)rawPixelBuffer);
    
    return newId;
}

void QLaylaOSCursor::changeCursor(QCursor *windowCursor, QWindow *window)
{
    if (!window)
        return;

    QLaylaOSWindow *targetWindow = static_cast<QLaylaOSWindow*>(window->handle());
    struct window_t *lwin = targetWindow->nativeHandle();

    if (!windowCursor) {
        cursor_show(lwin, CURSOR_NORMAL);
        return;
    }

    Qt::CursorShape shape = windowCursor->shape();

    // Standard system cursors
    if (shape != Qt::BitmapCursor) {
        cursor_show(lwin, m_systemCurIds.value(shape));
    }

    // Custom bitmap cursors
    QImage customImage = windowCursor->pixmap().toImage();
    if (customImage.isNull()) {
        // Fallback check if this is QBitmap and not QPixmap
        customImage = windowCursor->bitmap().toImage();
    }
    
    QPoint hotspot = windowCursor->hotSpot();

    // Hash the cursor
    uint64_t pixelHash = hashCursorBitmap(customImage, hotspot);
    m_globalAccessCounter++;

    if (m_customCursorCache.contains(pixelHash)) {
        m_customCursorCache[pixelHash].lastUsedTick = m_globalAccessCounter; // Update LRU
        cursor_show(lwin, m_customCursorCache[pixelHash].cursorId);
        return;
    }

    if (m_customCursorCache.size() >= MAX_CACHE_SIZE) {
        evictOldestCacheNode();
    }

    curid_t curId = uploadCursorToServer(customImage, hotspot);

    QLaylaOSCursorCacheNode newNode;
    newNode.pixelHash = pixelHash;
    newNode.cursorId = curId;
    newNode.lastUsedTick = m_globalAccessCounter;

    m_customCursorCache.insert(pixelHash, newNode);

    cursor_show(lwin, curId);
}

#endif

QPoint QLaylaOSCursor::pos() const
{
    if (cachedMouseX == -1 || cachedMouseY == -1)
    {
        struct cursor_info_t curinfo = { 0, 0, 0, 0, 0, };
    
        cursor_get_info(&curinfo);

        // see the comment at the end of QLaylaOSWindow::QLaylaOSWindow() for
        // why this call is important here
        auto *laylaos_integration = static_cast<QLaylaOSIntegration *>(QGuiApplicationPrivate::platformIntegration());
        QLaylaOSSocketMonitor *monitor = laylaos_integration->getSocketMonitor();
        emit monitor->gonow();
        //m_socketmonitor->readyRead();

        cachedMouseX = curinfo.x;
        cachedMouseY = curinfo.y;
    }

    return QPoint(cachedMouseX, cachedMouseY);
}

void QLaylaOSCursor::setPos(const QPoint &pos)
{
    cursor_set_pos(pos.x(), pos.y());
    cachedMouseX = pos.x();
    cachedMouseY = pos.y();
}

void QLaylaOSCursor::updateMousePos(int x, int y)
{
    cachedMouseX = x;
    cachedMouseY = y;
}

