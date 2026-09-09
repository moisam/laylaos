// Copyright (C) 2024-2026 Mohammed Isam
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qlaylaoswindow.h"
#include "qlaylaoskeymapper.h"
#include "qlaylaossocketmonitor.h"
#include "qlaylaosintegration.h"
#include "qlaylaosscreen.h"

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
#include <gui/client/systray.h>

#define NODECORATION_FLAGS  \
    (WINDOW_NODECORATION | WINDOW_SKIPTASKBAR | WINDOW_ALWAYSONTOP)

QT_BEGIN_NAMESPACE

enum {
    DefaultWindowWidth = 20,
    DefaultWindowHeight = 20
};

static inline Qt::WindowType getWindowType(Qt::WindowFlags flags)
{
    return static_cast<Qt::WindowType>(static_cast<int>(flags & Qt::WindowType_Mask));
}

static uint32_t toLaylaOSFlags(Qt::WindowFlags flags)
{
    const Qt::WindowType type = getWindowType(flags);
    const bool isWidget = (type == Qt::Widget);
    const bool isPopup = (type == Qt::Popup);
    const bool isSplashScreen = (type == Qt::SplashScreen);
    const bool isDialog = ((type == Qt::Dialog) || (type == Qt::Sheet) || (type == Qt::MSWindowsFixedSizeDialogHint));
    const bool isToolTip = (type == Qt::ToolTip);
    const bool isTool = (type == Qt::Tool);
    const bool isSubwindow = (type & Qt::SubWindow) == Qt::SubWindow;

    uint32_t wflag = 0;

    if (isSplashScreen) {
        wflag = NODECORATION_FLAGS;
    }

    if (isPopup) {
        wflag = NODECORATION_FLAGS | WINDOW_ABSOLUTE_COORDS;
        flags |= Qt::WindowStaysOnTopHint;
    }

    if (isDialog) {
        // TODO: create a modal form
        wflag = WINDOW_NORESIZE | WINDOW_NOMINIMIZE | WINDOW_SKIPTASKBAR;
    }

    if (isToolTip) {
        wflag = NODECORATION_FLAGS | WINDOW_NOFOCUS | WINDOW_ABSOLUTE_COORDS;
        flags |= Qt::WindowStaysOnTopHint;
    }

    if (isSubwindow) {
        wflag = WINDOW_SKIPTASKBAR | WINDOW_SUBWINDOW;
    }

    if (isWidget || isTool) {
        wflag = WINDOW_SKIPTASKBAR;
    }

    if (flags & Qt::FramelessWindowHint)
        wflag |= WINDOW_NODECORATION;

    if (!(flags & Qt::WindowTitleHint) && (type != Qt::Window))
        wflag |= WINDOW_NODECORATION;

    if (flags & Qt::MSWindowsFixedSizeDialogHint)
        wflag |= WINDOW_NORESIZE;

    if (flags & Qt::WindowDoesNotAcceptFocus)
        wflag |= WINDOW_NOFOCUS | WINDOW_SKIPTASKBAR;

    if (flags & Qt::WindowTransparentForInput)
        wflag |= WINDOW_NOINPUT;

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

    if (flags & Qt::WindowStaysOnBottomHint)
        wflag |= WINDOW_NORAISE;

    return wflag;
}

static QWindow *findParent(QWindow *window)
{
    QWindow *topLevelParent = nullptr;

    topLevelParent = window->transientParent();

    if (!topLevelParent) {
        topLevelParent = window->parent();
    }

    if (topLevelParent && topLevelParent->type() == Qt::Window) return topLevelParent;

    QWindow *root = topLevelParent ? topLevelParent : window;

    while (root->parent()) {
        root = root->parent();
    }

    //qDebug() << "QLaylaOSWindow::findParent: root " << root;

    return root;
}

void QLaylaOSWindow::init()
{
    QWindow *p = findParent(window());
    const QRect rect = initialGeometry(window(), window()->geometry(), DefaultWindowWidth, DefaultWindowHeight);
    struct window_attribs_t attribs;
    uint32_t flags = toLaylaOSFlags(window()->flags());
    const Qt::WindowType type = getWindowType(window()->flags());
    const bool isSubwindow = (type & Qt::SubWindow) == Qt::SubWindow;
    const bool isPopup = (type == Qt::Popup);
    const bool isToolTip = (type == Qt::ToolTip);
    const bool isTool = (type == Qt::Tool);
    const bool isDialog = ((type == Qt::Dialog) || (type == Qt::Sheet) || (type == Qt::MSWindowsFixedSizeDialogHint));
    bool tried = false;

    attribs.gravity = WINDOW_ALIGN_ABSOLUTE;
    attribs.x = rect.x();
    attribs.y = rect.y();
    attribs.w = rect.width();
    attribs.h = rect.height();
    attribs.flags = flags;

    qDebug("QLaylaOSWindow::QLaylaOSWindow: x %d, y %d, w %u, h %u, fl 0x%x", attribs.x, attribs.y, attribs.w, attribs.h, attribs.flags);
    qDebug() << "QLaylaOSWindow::QLaylaOSWindow: type " << type << ", p " << p;

    if (!p)
        p = QGuiApplication::focusWindow();

    qDebug() << "QLaylaOSWindow::QLaylaOSWindow: isDialog " << isDialog << ", isPopup " << isPopup << ", isToolTip " << isToolTip << ", isSubwindow " << isSubwindow << ", qtflags " << window()->flags() << ", p " << p;

    if (p) {
        QLaylaOSWindow *parent = static_cast<QLaylaOSWindow*>(p->handle());

        if (parent) {
            struct window_t *pwin = parent->nativeHandle();

            qDebug("QLaylaOSWindow::QLaylaOSWindow: parent type %d, ownerid %ld", pwin->type, pwin->winid);

            if (isDialog) {
                m_window = __window_create(&attribs, WINDOW_TYPE_DIALOG, pwin->winid);
                tried = true;
            } else if (isPopup || isToolTip) {
                m_window = __window_create(&attribs, WINDOW_TYPE_MENU_FRAME, pwin->winid);
                tried = true;
            } else if (isSubwindow || isTool) {
                m_window = __window_create(&attribs, WINDOW_TYPE_WINDOW, pwin->winid);
                tried = true;
            }
        } else if (isPopup) {
            // we have no way of identifying systray menus, so assume a popup
            // with no parent is one of those and reparent to the systray manager
            winid_t systray_manager = systray_get_manager_winid();

            if (systray_manager != 0) {
                m_window = __window_create(&attribs, WINDOW_TYPE_MENU_FRAME, systray_manager);
                tried = true;
            }
        }
    }

    if (!tried)
        m_window = window_create(&attribs);

    if (Q_UNLIKELY(!m_window))
        qFatal("QLaylaOS: failed to create window: %s", strerror(errno));

    if (!window()->title().isEmpty())
        window_set_title(m_window, (char *)window()->title().toUtf8().constData());

    setWindowIcon(window()->icon());
    m_window->visible = 0;

    m_socketmonitor->addWindow(m_window->winid, window());
    qDebug() << "QLaylaOSWindow::QLaylaOSWindow: done -- winid " << m_window->winid;

    // libgui has functions that call get_server_reply() internally.
    // This call leads to event pooling in libgui, which can lead to missed
    // events as our socket monitor will not fire an activated() event until
    // a new event occurs, e.g. the mouse is moved.
    // To avoid this, we make the below call to process any pooled events.
    emit m_socketmonitor->gonow();
    //m_socketmonitor->readyRead();
}

QLaylaOSWindow::QLaylaOSWindow(QWindow *window, QLaylaOSSocketMonitor *socketmonitor)
    : QPlatformWindow(window)
    , m_window(nullptr)
    , m_socketmonitor(socketmonitor)
    , m_windowState(Qt::WindowNoState)
    , m_mouse_grabbed(false)
    , m_kbd_grabbed(false)
{
}

QLaylaOSWindow::~QLaylaOSWindow()
{
    if (m_socketmonitor) {
        m_socketmonitor->removeWindow(m_window->winid);
        m_socketmonitor = nullptr;
    }

    if (m_window) window_destroy(m_window);
    m_window = nullptr;
}

void QLaylaOSWindow::setWindowIcon(const QIcon &icon)
{
    if (icon.isNull()) {
        return;
    }

    QSize targetSize(64, 64);
    QPixmap pixmap = icon.pixmap(targetSize);
    
    if (pixmap.isNull()) {
        return;
    }

    QImage srcImage = pixmap.toImage().convertToFormat(QImage::Format_RGBA8888);

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

    window_load_icon(m_window, width, height, (uint32_t *)rawPixelBuffer);
}

/*
QRect QLaylaOSWindow::initialGeometry(const QWindow *w, const QRect &initialGeometry, 
                                      int defaultWidth, int defaultHeight)
{
    const Qt::WindowType type = getWindowType(w->flags());
    const bool isSubwindow = (type & Qt::SubWindow) == Qt::SubWindow;
    QRect rect = QPlatformWindow::initialGeometry(w, initialGeometry, defaultWidth, defaultHeight);
    //qDebug() << "QLaylaOSWindow::initialGeometry: 1 rect " << rect;

    if ((rect.y() == 0) && !isSubwindow) {
        // push below the desktop's top panel
        QRect screenres = QLaylaOSIntegration::instance()->getScreen()->availableGeometry();
        //qDebug() << "QLaylaOSWindow::initialGeometry: screenres " << screenres;
        rect.moveTop(screenres.top());
    }
    //qDebug() << "QLaylaOSWindow::initialGeometry: 2 rect " << rect;

    return rect;
}
*/

void QLaylaOSWindow::setGeometry(const QRect &rect)
{
    QRect adjustedRect = rect;
    /*
    const Qt::WindowType type = getWindowType(window()->flags());
    const bool isSubwindow = (type & Qt::SubWindow) == Qt::SubWindow;
    */

    if (m_window) {
        //qDebug() << "QLaylaOSWindow::setGeometry: winid " << m_window->winid << ", rect " << rect;

        /*
        if (adjustedRect.y() == 0 && !isSubwindow &&
            (type == Qt::Window || type == Qt::Widget)) {
            // push below the desktop's top panel
            QRect screenres = QLaylaOSIntegration::instance()->getScreen()->availableGeometry();
            adjustedRect.moveTop(screenres.top());
            QPlatformWindow::setGeometry(adjustedRect);
        }
        */

        window_set_size(m_window, adjustedRect.x(), adjustedRect.y(), 
                                  adjustedRect.width(), adjustedRect.height());

        QWindowSystemInterface::handleGeometryChange(window(), adjustedRect);
        QWindowSystemInterface::handleExposeEvent(window(), QRect(QPoint(0, 0), adjustedRect.size()));
    }
}

QRect QLaylaOSWindow::geometry() const
{
    if (!m_window)
        qFatal("QLaylaOS: requesting geometry() on a destroyed window");

    if (m_window->flags & WINDOW_NODECORATION)
        return QRect(QPoint(m_window->x, m_window->y), QSize(m_window->w, m_window->h));

    return QRect(QPoint(m_window->x + WINDOW_BORDERWIDTH, m_window->y + WINDOW_TITLEHEIGHT),
                 QSize(m_window->w, m_window->h));
}

//
// See: https://doc.qt.io/qt-5/application-windows.html
//
QMargins QLaylaOSWindow::frameMargins() const
{
    if (m_window && m_window->flags & WINDOW_NODECORATION)
        return QMargins(0, 0, 0, 0);

    return QMargins(WINDOW_BORDERWIDTH,     // left
                    WINDOW_TITLEHEIGHT,     // top
                    WINDOW_BORDERWIDTH,     // right
                    WINDOW_BORDERWIDTH);    // bottom
}

void QLaylaOSWindow::setVisible(bool visible)
{
    if (!m_window) return;

    //qDebug() << "QLaylaOSWindow::setVisible: winid " << m_window->winid << " visible" << visible;

    if (visible) {
        // If the app calls setMinimumSize() or setMaximumSize() before setVisible()
        // the size hints are not propagated. Catch this here before the window is
        // shown and pass the size hints to the server
        QSize minSize = window()->minimumSize();
        QSize maxSize = window()->maximumSize();
        bool minSizeValid = false;
        bool maxSizeValid = false;

        if (minSize.width() > 0 && minSize.height() > 0) {
            window_set_min_size(m_window, minSize.width(), minSize.height());
            minSizeValid = true;
        }

        if (maxSize.width() < QWINDOWSIZE_MAX && maxSize.height() < QWINDOWSIZE_MAX) { 
            window_set_max_size(m_window, maxSize.width(), maxSize.height());
            maxSizeValid = true;
        }

        if (minSizeValid && maxSizeValid &&
            minSize.width() == maxSize.width() &&
            minSize.height() == maxSize.height() &&
            !(m_window->flags & WINDOW_NORESIZE)) {
            window_set_resizable(m_window, 0);
        }

        // QWidget-attribute Qt::WA_ShowWithoutActivating.
        const auto showWithoutActivating = window()->property("_q_showWithoutActivating");
        if (showWithoutActivating.isValid() && showWithoutActivating.toBool()) {
            if (!(m_window->flags & WINDOW_NOFOCUS)) {
                window_set_focusable(m_window, 0);
            }
        }

        // We need to apply maximize and fullscreen states before showing the window.
        // Qt calls setGeometry() before calling setVisible(), so applying these
        // states in setWindowState() will not work as the maximized/fullscreen
        // geometry will be overwritten by setGeometry() before the window is shown.
        if (m_windowState & Qt::WindowMaximized)
            window_maximize(m_window);
        else if (m_windowState & Qt::WindowFullScreen)
            window_enter_fullscreen(m_window);

        window_show(m_window);

        if(!(window()->flags() & Qt::WindowDoesNotAcceptFocus))
            window()->requestActivate();
    } else {
        window_hide(m_window);

        //qDebug() << "QLaylaOSWindow::setVisible: winid " << m_window->winid << " m_mouse_grabbed" << m_mouse_grabbed;

        if (m_mouse_grabbed) {
            mouse_ungrab();
            m_mouse_grabbed = false;
        }

        //qDebug() << "QLaylaOSWindow::setVisible: winid " << m_window->winid << " m_kbd_grabbed" << m_kbd_grabbed;

        if (m_kbd_grabbed) {
            keyboard_ungrab();
            m_kbd_grabbed = false;
        }
    }
}

bool QLaylaOSWindow::isExposed() const
{
    //qDebug() << "isExposed: winid " << m_window->winid << !(m_windowState == Qt::WindowMinimized);
    //return !(m_windowState == Qt::WindowMinimized);
    //qDebug() << "isExposed: winid " << m_window->winid << m_window->visible;
    return m_window->visible;
}

bool QLaylaOSWindow::isActive() const
{
    if (!m_window) return false;

    return !!(m_window->flags & WINDOW_IS_FOCUSED);
    /*
    winid_t focus = get_input_focus();

    //qDebug() << "isActive: winid " << m_window->winid << ", focus " << focus << ", " << (focus == m_window->winid);

    // see the comment at the end of QLaylaOSWindow::QLaylaOSWindow() for
    // why this call is important here
    emit m_socketmonitor->gonow();
    //m_socketmonitor->readyRead();

    return (focus == m_window->winid);
    */
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
    //qDebug() << "QLaylaOSWindow::requestActivateWindow: " << m_window->winid;
    if(m_window) {
        window_raise(m_window);
    }
}

void QLaylaOSWindow::setWindowState(Qt::WindowStates state)
{
    //qDebug() << "QLaylaOSWindow::setWindowState: " << state;
    if (m_windowState == state)
        return;

    const Qt::WindowStates oldState = m_windowState;

    m_windowState = state;

    // if the window is shown, changes must be immediate
    if (isExposed()) {
        if (m_windowState & Qt::WindowMinimized)
            window_minimize(m_window);
        else if (m_windowState & Qt::WindowMaximized)
            window_maximize(m_window);
        else if (m_windowState & Qt::WindowFullScreen)
            window_enter_fullscreen(m_window);
        else if (oldState & Qt::WindowMinimized)
            window_restore(m_window); // undo minimize
        else if (oldState & Qt::WindowMaximized)
            window_restore(m_window); // undo zoom
        else if (oldState & Qt::WindowFullScreen)
            window_restore(m_window); // undo fullscreen
    }
}

void QLaylaOSWindow::setWindowFlags(Qt::WindowFlags flags)
{
    uint32_t wflag = toLaylaOSFlags(flags);
    //qDebug() << "QLaylaOSWindow::setWindowFlags: " << flags << wflag;

    if (!m_window) return;

    if ((wflag & WINDOW_NODECORATION) != (m_window->flags & WINDOW_NODECORATION))
        window_set_bordered(m_window, !(wflag & WINDOW_NODECORATION));

    if ((wflag & WINDOW_NORESIZE) != (m_window->flags & WINDOW_NORESIZE))
        window_set_resizable(m_window, !(wflag & WINDOW_NORESIZE));

    if ((wflag & WINDOW_ALWAYSONTOP) != (m_window->flags & WINDOW_ALWAYSONTOP))
        window_set_ontop(m_window, !!(wflag & WINDOW_ALWAYSONTOP));

    if ((wflag & WINDOW_NOFOCUS) != (m_window->flags & WINDOW_NOFOCUS))
        window_set_focusable(m_window, !(wflag & WINDOW_NOFOCUS));

    if ((wflag & WINDOW_NOINPUT) != (m_window->flags & WINDOW_NOINPUT))
        window_set_transparent_to_mouse(m_window, !!(wflag & WINDOW_NOINPUT));

    m_window->flags = wflag;
}

void QLaylaOSWindow::setWindowTitle(const QString &title)
{
    if (m_window)
        window_set_title(m_window, (char *)title.toLocal8Bit().constData());
}

void QLaylaOSWindow::propagateSizeHints()
{
    if (!m_window) return;

    //qDebug() << "QLaylaOSWindow::propagateSizeHints: " << m_window->winid << window()->minimumSize() << window()->maximumSize();

    // TODO: process zoom size as well
    window_set_min_size(m_window, window()->minimumSize().width(),
                                  window()->minimumSize().height());

    window_set_max_size(m_window, window()->maximumSize().width(),
                                  window()->maximumSize().height());

    if (window()->minimumSize().width() == window()->maximumSize().width() &&
        window()->minimumSize().height() == window()->maximumSize().height() &&
        !(m_window->flags & WINDOW_NORESIZE)) {
        window_set_resizable(m_window, 0);
    }
}

void QLaylaOSWindow::raise()
{
    //qDebug() << "QLaylaOSWindow::raise: " << m_window->winid;
    if (m_window) window_raise(m_window);
}

void QLaylaOSWindow::lower()
{
    //qDebug() << "QLaylaOSWindow::lower: " << m_window->winid;

    // TODO: this should lower, not minimize, the window
    if (m_window) window_minimize(m_window);
}

bool QLaylaOSWindow::setKeyboardGrabEnabled(bool grab)
{
    //qDebug() << "QLaylaOSWindow::setKeyboardGrabEnabled: " << m_window->winid << grab;

    if (grab) {
        int res = keyboard_grab(m_window);

        // see the comment at the end of QLaylaOSWindow::QLaylaOSWindow() for
        // why this call is important here
        emit m_socketmonitor->gonow();
        //m_socketmonitor->readyRead();

        m_kbd_grabbed = res;
        return res;
    } else {
        keyboard_ungrab();
        m_kbd_grabbed = false;
        return true;
    }
}

bool QLaylaOSWindow::setMouseGrabEnabled(bool grab)
{
    //qDebug() << "QLaylaOSWindow::setMouseGrabEnabled: " << m_window->winid << grab;

    if (grab) {
        int res = mouse_grab(m_window, 0);

        // see the comment at the end of QLaylaOSWindow::QLaylaOSWindow() for
        // why this call is important here
        emit m_socketmonitor->gonow();
        //m_socketmonitor->readyRead();

        m_mouse_grabbed = res;
        return res;
    } else {
        mouse_ungrab();
        m_mouse_grabbed = false;
        return true;
    }
}

bool QLaylaOSWindow::hasTransparentBackground() const
{
    QSurfaceFormat format = window()->format();

    if (format.alphaBufferSize() > 0) {
        return true;
    }

    if (window()->flags() & Qt::FramelessWindowHint) {
        return true;
    }

    return false;
}

qreal QLaylaOSWindow::devicePixelRatio() const
{
    return 1.0; 
}

QT_END_NAMESPACE
