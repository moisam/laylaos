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

#include "qlaylaoscursor.h"
#include "qlaylaoswindow.h"

QLaylaOSCursor::QLaylaOSCursor()
{
    m_curids.insert(Qt::ArrowCursor, CURSOR_NORMAL);
    m_curids.insert(Qt::UpArrowCursor, CURSOR_NS);
    m_curids.insert(Qt::CrossCursor, CURSOR_CROSSHAIR);
    m_curids.insert(Qt::WaitCursor, CURSOR_WAITING);
    m_curids.insert(Qt::IBeamCursor, CURSOR_IBEAM);
    m_curids.insert(Qt::SizeVerCursor, CURSOR_NS);
    m_curids.insert(Qt::SizeHorCursor, CURSOR_WE);
    m_curids.insert(Qt::SizeBDiagCursor, CURSOR_NESW);
    m_curids.insert(Qt::SizeFDiagCursor, CURSOR_NWSE);
    m_curids.insert(Qt::SizeAllCursor, CURSOR_CROSS);
    m_curids.insert(Qt::BlankCursor, CURSOR_NONE);
    m_curids.insert(Qt::SplitVCursor, CURSOR_NS);
    m_curids.insert(Qt::SplitHCursor, CURSOR_WE);
    m_curids.insert(Qt::PointingHandCursor, CURSOR_HAND);
    m_curids.insert(Qt::ForbiddenCursor, CURSOR_X);
    m_curids.insert(Qt::OpenHandCursor, CURSOR_HAND);
    m_curids.insert(Qt::ClosedHandCursor, CURSOR_HAND);
    m_curids.insert(Qt::WhatsThisCursor, CURSOR_NORMAL);
    m_curids.insert(Qt::BusyCursor, CURSOR_WAITING);
}

#ifndef QT_NO_CURSOR
void QLaylaOSCursor::changeCursor(QCursor *windowCursor, QWindow *window)
{
    if (!window)
        return;

    QLaylaOSWindow *targetWindow = static_cast<QLaylaOSWindow*>(window->handle());
    struct window_t *lwin = targetWindow->nativeHandle();

    if (!windowCursor) {
        cursor_show(lwin, CURSOR_NORMAL);
    } else {
        const Qt::CursorShape shape = windowCursor->shape();
        cursor_show(lwin, m_curids.value(shape));
    }
}
#endif

QPoint QLaylaOSCursor::pos() const
{
    struct cursor_info_t curinfo = { 0, 0, 0, 0, 0, };
    
    cursor_get_info(&curinfo);

    return QPoint(curinfo.x, curinfo.y);
}

void QLaylaOSCursor::setPos(const QPoint &pos)
{
    cursor_set_pos(pos.x(), pos.y());
}

