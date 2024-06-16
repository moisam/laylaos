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

#include "qlaylaosscreen.h"
#include "qlaylaoscursor.h"

#include <qpa/qwindowsysteminterface.h>

#include <gui/gui-global.h>
#include <gui/screen.h>

QLaylaOSScreen::QLaylaOSScreen()
    : m_cursor(new QLaylaOSCursor)
{
}

QLaylaOSScreen::~QLaylaOSScreen()
{
    delete m_cursor;
    m_cursor = nullptr;
}

QPixmap QLaylaOSScreen::grabWindow(WId winId, int x, int y, int width, int height) const
{
    if (width == 0 || height == 0)
        return QPixmap();

    // TODO: implement this - see Haiku plugin source files

    return QPixmap();
}

QRect QLaylaOSScreen::geometry() const
{
    return QRect(0, 0, __global_gui_data.screen.w, __global_gui_data.screen.h);
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
        return 24;
        break;
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    default:
        return 32;
        break;
    }
}

QImage::Format QLaylaOSScreen::format() const
{
    return QImage::Format_RGB32;
}

QPlatformCursor *QLaylaOSScreen::cursor() const
{
    return m_cursor;
}

QT_END_NAMESPACE
