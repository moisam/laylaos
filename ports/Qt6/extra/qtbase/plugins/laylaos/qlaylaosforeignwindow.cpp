// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2018 QNX Software Systems. All rights reserved.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qlaylaosforeignwindow.h"
#include "qlaylaosintegration.h"
#include "qlaylaossocketmonitor.h"

#include <gui/kbd.h>

QT_BEGIN_NAMESPACE

QLaylaOSForeignWindow::QLaylaOSForeignWindow(QWindow *window, 
                                             QLaylaOSSocketMonitor *socketmonitor, 
                                             winid_t screenWindow)
    : QLaylaOSWindow(window, socketmonitor)
{
    struct window_attribs_t attribs;
    int st;

    m_window = (struct window_t *)malloc(sizeof(struct window_t));

    if (Q_UNLIKELY(!m_window))
        qFatal("QLaylaOS: failed to create foreign window: %s", strerror(errno));

    memset(m_window, 0, sizeof(struct window_t));

    m_window->winid = screenWindow;

    if (!window_add_to_global_list(m_window, 0))
        qFatal("QLaylaOS: failed to add foreign window: %s", strerror(errno));

    m_socketmonitor->addWindow(m_window->winid, window);

    // get the window's attributes
    if (get_win_attribs(screenWindow, &attribs)) {
        m_window->x = attribs.x;
        m_window->y = attribs.y;
        m_window->w = attribs.w;
        m_window->h = attribs.h;
        m_window->flags = attribs.flags;

        // reflect the foreign window's geometry as our own
        QPlatformWindow::setGeometry(QRect(m_window->x, m_window->y, m_window->w, m_window->h));
    }

    st = get_win_state(screenWindow);
    m_windowState = Qt::WindowNoState;

    if (st == WINDOW_STATE_MAXIMIZED) {
        m_windowState = Qt::WindowMaximized;
        QPlatformWindow::setWindowState(m_windowState);
    } else if (st == WINDOW_STATE_MINIMIZED) {
        m_windowState = Qt::WindowMinimized;
        QPlatformWindow::setWindowState(m_windowState);
    }

    winid_t focus = get_input_focus();

    if (focus == m_window->winid)
        m_window->flags |= WINDOW_IS_FOCUSED;

    // see the comment at the end of QLaylaOSWindow::QLaylaOSWindow() for
    // why this call is important here
    emit m_socketmonitor->gonow();

    //qDebug() << "QLaylaOSWindow::QLaylaOSWindow: done";
}

QLaylaOSForeignWindow::~QLaylaOSForeignWindow()
{
    if (m_socketmonitor) {
        m_socketmonitor->removeWindow(m_window->winid);
        m_socketmonitor = nullptr;
    }

    m_window = nullptr;
}

bool QLaylaOSForeignWindow::isForeignWindow() const
{
    return true;
}

QT_END_NAMESPACE
