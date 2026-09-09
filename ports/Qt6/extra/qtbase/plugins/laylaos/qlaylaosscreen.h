// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSSCREEN_H
#define QLAYLAOSSCREEN_H

#include <qpa/qplatformscreen.h>

class QLaylaOSCursor;

QT_BEGIN_NAMESPACE

class QLaylaOSScreen : public QPlatformScreen
{
public:
    QLaylaOSScreen();
    ~QLaylaOSScreen();

    void handleScreenResChange();
    void checkScreenRes();
    QLaylaOSCursor *getCursor() { return m_cursor; }

    QPixmap grabWindow(WId window, int x, int y, int width, int height) const override;

    QRect geometry() const override;
    QRect availableGeometry() const override;
    int depth() const override;
    QSizeF physicalSize() const override;
    QDpi logicalDpi() const override;
    QDpi logicalBaseDpi() const override;
    QImage::Format format() const override;
    QPlatformCursor *cursor() const override;

    Qt::ScreenOrientation orientation() const override;
    qreal devicePixelRatio() const override;
    qreal refreshRate() const override;

private:
    QLaylaOSCursor *m_cursor;
    QRect m_usableScreen;
};

QT_END_NAMESPACE

#endif
