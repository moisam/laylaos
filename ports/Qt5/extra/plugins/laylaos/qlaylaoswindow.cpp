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

#include "qlaylaoswindow.h"
#include "qlaylaoskeymapper.h"
#include "qlaylaoseventlooper.h"

#include "private/qguiapplication_p.h"

#include <QCoreApplication>
#include <QThread>
#include <QWindow>
#include <QDeadlineTimer>
#include <qpa/qwindowsysteminterface.h>

#include <QDebug>

#include <gui/keys.h>
#include <gui/kbd.h>
#include <gui/mouse.h>

#define NODECORATION_FLAGS  \
    (WINDOW_NODECORATION | WINDOW_SKIPTASKBAR | WINDOW_ALWAYSONTOP)

QT_BEGIN_NAMESPACE

enum {
    DefaultWindowWidth = 160,
    DefaultWindowHeight = 160
};

static inline Qt::WindowType getWindowType(Qt::WindowFlags flags)
{
    return static_cast<Qt::WindowType>(static_cast<int>(flags & Qt::WindowType_Mask));
}

static uint32_t toLaylaOSFlags(Qt::WindowFlags flags)
{
    const Qt::WindowType type = getWindowType(flags);
    const bool isPopup = (type == Qt::Popup);
    const bool isSplashScreen = (type == Qt::SplashScreen);
    const bool isDialog = ((type == Qt::Dialog) || (type == Qt::Sheet) || (type == Qt::MSWindowsFixedSizeDialogHint));
    const bool isToolTip = (type == Qt::ToolTip);

    uint32_t wflag = 0;

    if (isSplashScreen) {
        wflag = NODECORATION_FLAGS;
    }

    if (isPopup) {
        wflag = NODECORATION_FLAGS;
        flags |= Qt::WindowStaysOnTopHint;
    }

    if (isDialog) {
        // TODO: create a modal form
        wflag = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;
    }

    if (isToolTip) {
        wflag = NODECORATION_FLAGS;
        flags |= Qt::WindowStaysOnTopHint;
    }

    if (flags & Qt::FramelessWindowHint)
        wflag |= WINDOW_NODECORATION;

    if (flags & Qt::MSWindowsFixedSizeDialogHint)
        wflag |= WINDOW_NORESIZE;

    if (flags & Qt::CustomizeWindowHint) {
        if (!(flags & Qt::WindowMinimizeButtonHint))
            wflag |= WINDOW_NOMINIMIZE;
        if (!(flags & Qt::WindowMaximizeButtonHint))
            wflag |= WINDOW_NOCONTROLBOX;   // TODO: flip only the max button
        if (!(flags & Qt::WindowCloseButtonHint))
            wflag |= WINDOW_NOCONTROLBOX;   // TODO: flip only the close button
    }

    if (flags & Qt::WindowStaysOnTopHint)
        wflag |= WINDOW_ALWAYSONTOP;

    return wflag;
}

QLaylaOSWindow::QLaylaOSWindow(QWindow *window, QLaylaOSEventLooper *eventlooper)
    : QPlatformWindow(window)
    , m_window(nullptr)
    , m_windowState(Qt::WindowNoState)
{
    const QRect rect = initialGeometry(window, window->geometry(), DefaultWindowWidth, DefaultWindowHeight);
    struct window_attribs_t attribs;
    uint32_t flags = toLaylaOSFlags(window->flags());
    const Qt::WindowType type = getWindowType(window->flags());
    const bool isDialog = ((type == Qt::Dialog) || (type == Qt::Sheet) || (type == Qt::MSWindowsFixedSizeDialogHint));

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = rect.x();
    attribs.y = rect.y();
    attribs.w = rect.width();
    attribs.h = rect.height();
    attribs.flags = flags;

    qDebug("QLaylaOSWindow::QLaylaOSWindow: x %d, y %d, w %u, h %u, fl 0x%x", attribs.x, attribs.y, attribs.w, attribs.h, attribs.flags);
    qDebug() << "QLaylaOSWindow::QLaylaOSWindow: isDialog " << isDialog << ", p " << window->parent();

    if (isDialog && window->parent()) {
        QLaylaOSWindow *parent = static_cast<QLaylaOSWindow*>(window->parent()->handle());

        if (parent) {
            struct window_t *pwin = parent->nativeHandle();

            qDebug("QLaylaOSWindow::QLaylaOSWindow: parent type %d, ownerid %ld", pwin->type, pwin->winid);
            m_window = __window_create(&attribs, WINDOW_TYPE_DIALOG, pwin->winid);
        } else {
            m_window = window_create(&attribs);
        }
    } else {
        m_window = window_create(&attribs);
    }

    if (Q_UNLIKELY(!m_window))
        qFatal("QLaylaOS: failed to create window: %s", strerror(errno));

    if (!window->title().isEmpty())
        window_set_title(m_window, (char *)window->title().toUtf8().constData());

    QPlatformWindow::setGeometry(rect);

    m_eventlooper = eventlooper;
    m_eventlooper->addWindow(m_window->winid, window);
}

QLaylaOSWindow::~QLaylaOSWindow()
{
    if (m_eventlooper) {
        m_eventlooper->removeWindow(m_window->winid);
        m_eventlooper = nullptr;
    }

    window_destroy(m_window);
    m_window = nullptr;
}

void QLaylaOSWindow::detachFromLooper()
{
    m_eventlooper = nullptr;
}

void QLaylaOSWindow::setGeometry(const QRect &rect)
{
    qDebug() << "QLaylaOSWindow::setGeometry:" << rect;
    QPlatformWindow::setGeometry(rect);

    window_set_size(m_window, rect.x(), rect.y(), rect.width(), rect.height());
}

QRect QLaylaOSWindow::geometry() const
{
    if(m_window->flags & WINDOW_NODECORATION)
        return QRect(QPoint(m_window->x, m_window->y), QSize(m_window->w, m_window->h));

    return QRect(QPoint(m_window->x + WINDOW_BORDERWIDTH, m_window->y + WINDOW_TITLEHEIGHT),
                 QSize(m_window->w, m_window->h));
}

//
// See: https://doc.qt.io/qt-5/application-windows.html
//
QMargins QLaylaOSWindow::frameMargins() const
{
    if(m_window->flags & WINDOW_NODECORATION)
        return QMargins(0, 0, 0, 0);

    return QMargins(WINDOW_BORDERWIDTH,     //left
                    WINDOW_TITLEHEIGHT,     // top
                    WINDOW_BORDERWIDTH,     // right
                    WINDOW_BORDERWIDTH);    // bottom
}

void QLaylaOSWindow::setVisible(bool visible)
{
    if (visible) {
        window_show(m_window);

        window()->requestActivate();

        QWindowSystemInterface::handleExposeEvent(window(), 
                    QRect(QPoint(0, 0), window()->geometry().size()));
    } else {
        window_hide(m_window);
    }
}

bool QLaylaOSWindow::isExposed() const
{
    //qDebug() << "isExposed: " << !(m_windowState == Qt::WindowMinimized);
    return !(m_windowState == Qt::WindowMinimized);
}

bool QLaylaOSWindow::isActive() const
{
    //qDebug() << "isActive: " << !(get_input_focus() == m_window->winid);
    return (get_input_focus() == m_window->winid);
}

WId QLaylaOSWindow::winId() const
{
    return (WId)(m_window);
}

struct window_t *QLaylaOSWindow::nativeHandle() const
{
    return m_window;
}

void QLaylaOSWindow::requestActivateWindow()
{
    if(m_window) window_raise(m_window);
}

void QLaylaOSWindow::setWindowState(Qt::WindowStates state)
{
    if (m_windowState == state)
        return;

    const Qt::WindowStates oldState = m_windowState;

    m_windowState = state;

    if (m_windowState & Qt::WindowMinimized)
        window_minimize(m_window);
    else if (m_windowState & Qt::WindowMaximized)
        window_maximize(m_window);
    else if (oldState & Qt::WindowMinimized)
        window_restore(m_window); // undo minimize
    else if (oldState & Qt::WindowMaximized)
        window_restore(m_window); // undo zoom
}

void QLaylaOSWindow::setWindowFlags(Qt::WindowFlags flags)
{
    uint32_t wflag = toLaylaOSFlags(flags);

    if ((wflag & WINDOW_NODECORATION) != (m_window->flags & WINDOW_NODECORATION))
        window_set_bordered(m_window, !!(wflag & WINDOW_NODECORATION));

    if ((wflag & WINDOW_NORESIZE) != (m_window->flags & WINDOW_NORESIZE))
        window_set_resizable(m_window, !!(wflag & WINDOW_NORESIZE));

    if ((wflag & WINDOW_ALWAYSONTOP) != (m_window->flags & WINDOW_ALWAYSONTOP))
        window_set_ontop(m_window, !!(wflag & WINDOW_ALWAYSONTOP));

    m_window->flags = wflag;
}

void QLaylaOSWindow::setWindowTitle(const QString &title)
{
    window_set_title(m_window, (char *)title.toLocal8Bit().constData());
}

void QLaylaOSWindow::propagateSizeHints()
{
    // TODO: process max size and zoom size as well
    window_set_min_size(m_window, window()->minimumSize().width(),
                                  window()->minimumSize().height());
}

void QLaylaOSWindow::raise()
{
    if (m_window) window_raise(m_window);
}

void QLaylaOSWindow::lower()
{
    // TODO: this should lower, not minimize, the window
    if (m_window) window_minimize(m_window);
}

bool QLaylaOSWindow::setKeyboardGrabEnabled(bool grab)
{
    if (grab) {
        return keyboard_grab(m_window);
    } else {
        keyboard_ungrab();
        return true;
    }
}

bool QLaylaOSWindow::setMouseGrabEnabled(bool grab)
{
    if (grab) {
        return mouse_grab(m_window, 0);
    } else {
        mouse_ungrab();
        return true;
    }
}

QT_END_NAMESPACE
