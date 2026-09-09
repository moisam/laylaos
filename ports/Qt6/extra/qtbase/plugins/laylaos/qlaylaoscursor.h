// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSCURSOR_H
#define QLAYLAOSCURSOR_H

#include <qpa/qplatformcursor.h>
#include <QHash>
#include <QImage>

#include <gui/window-defs.h>
#include <gui/cursor.h>

QT_BEGIN_NAMESPACE

struct QLaylaOSCursorCacheNode
{
    uint64_t pixelHash;       // Unique signature of the bitmap data
    curid_t  cursorId;        // Cursor ID from the server
    uint64_t lastUsedTick;    // Timestamp for LRU eviction
};

class QLaylaOSCursor : public QPlatformCursor
{
public:
    QLaylaOSCursor();
    ~QLaylaOSCursor() override;

    void updateMousePos(int x, int y);

#ifndef QT_NO_CURSOR
    void changeCursor(QCursor *windowCursor, QWindow *window) override;
#endif
    QPoint pos() const override;
    void setPos(const QPoint &pos) override;

private:
#ifndef QT_NO_CURSOR
    uint64_t hashCursorBitmap(const QImage &image, const QPoint &hotspot);
    curid_t uploadCursorToServer(const QImage &image, const QPoint &hotspot);
    void evictOldestCacheNode();

    static const int MAX_CACHE_SIZE = 50;

    QHash<uint64_t, QLaylaOSCursorCacheNode> m_customCursorCache;
    uint64_t m_globalAccessCounter;
#endif

    QHash<Qt::CursorShape, curid_t> m_systemCurIds;
    static int cachedMouseX, cachedMouseY;
};

QT_END_NAMESPACE

#endif
