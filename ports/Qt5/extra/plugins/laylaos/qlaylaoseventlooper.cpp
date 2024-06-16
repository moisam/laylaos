/***************************************************************************
**
** Copyright (C) 2024 Mohammed Isam <mohammed_isam1984@yahoo.com>
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

#include <QThread>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <QDebug>

#include "qlaylaoseventlooper.h"
#include "qlaylaoskeymapper.h"
#include "qlaylaosintegration.h"
#include "qlaylaoswindow.h"

#include "private/qguiapplication_p.h"

#include <gui/keys.h>
#include <gui/kbd.h>

QT_BEGIN_NAMESPACE

QLaylaOSEventLooper::QLaylaOSEventLooper()
    : m_shouldStop(false)
{
}

void QLaylaOSEventLooper::terminateThread()
{
    stopInputEventLoop();

    // block until thread terminates
    wait();
}


#define BUTTON_PRESSED(which)   \
    (!(obuttons & MOUSE_ ## which ## _DOWN) &&  \
      (nbuttons & MOUSE_ ## which ## _DOWN))

#define BUTTON_RELEASED(which)  \
    ((obuttons & MOUSE_ ## which ## _DOWN) &&  \
     !(nbuttons & MOUSE_ ## which ## _DOWN))

ButtonState QLaylaOSEventLooper::getMouseButtons(struct event_t *ev)
{
    struct window_t *lwin = win_for_winid(ev->dest);

    Qt::MouseButton pressed = Qt::NoButton;
    Qt::MouseButton released = Qt::NoButton;
    Qt::MouseButtons state = 0;

    if(lwin == nullptr) {
        return { state, pressed, released };
    }

    mouse_buttons_t obuttons = lwin->last_button_state;
    mouse_buttons_t nbuttons = ev->mouse.buttons;

    lwin->last_button_state = nbuttons;

    if(BUTTON_PRESSED(LBUTTON)) pressed = Qt::LeftButton;
    if(BUTTON_PRESSED(RBUTTON)) pressed = Qt::RightButton;
    if(BUTTON_PRESSED(MBUTTON)) pressed = Qt::MiddleButton;

    if(BUTTON_RELEASED(LBUTTON)) released = Qt::LeftButton;
    if(BUTTON_RELEASED(RBUTTON)) released = Qt::RightButton;
    if(BUTTON_RELEASED(MBUTTON)) released = Qt::MiddleButton;

    state.setFlag(Qt::LeftButton, (nbuttons & MOUSE_LBUTTON_DOWN) ? true : false);
    state.setFlag(Qt::RightButton, (nbuttons & MOUSE_RBUTTON_DOWN) ? true : false);
    state.setFlag(Qt::MiddleButton, (nbuttons & MOUSE_MBUTTON_DOWN) ? true : false);

    return { state, pressed, released };
}

#undef BUTTON_PRESSED

Qt::KeyboardModifiers QLaylaOSEventLooper::getModifiers(char modkeys) const
{
    Qt::KeyboardModifiers modifiers(Qt::NoModifier);

    if (modkeys & MODIFIER_MASK_SHIFT)
        modifiers |= Qt::ShiftModifier;
    if (modkeys & MODIFIER_MASK_ALT)
        modifiers |= Qt::AltModifier;
    if (modkeys & MODIFIER_MASK_CTRL)
        modifiers |= Qt::ControlModifier;

    return modifiers;
}

void QLaylaOSEventLooper::updateWindowState(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);
    Qt::WindowStates oldState, newState;

    if(win == nullptr) {
        return;
    }

    oldState = win->windowState();

    switch (ev->winst.state) {
        case WINDOW_STATE_MAXIMIZED: newState = Qt::WindowMaximized; break;
        case WINDOW_STATE_MINIMIZED: newState = Qt::WindowMinimized; break;
        case WINDOW_STATE_FULLSCREEN: newState = Qt::WindowFullScreen; break;

        default:
            /*
            if (oldState == Qt::WindowMaximized ||
                oldState == Qt::WindowMinimized ||
                oldState == Qt::WindowFullscreen) {
                if (get_input_focus() == ev->dest) {
                    newState = Qt::WindowActive;
                } else {
                    newState = Qt::WindowNoState;
                }
            }
            */
            newState = Qt::WindowNoState;
            break;
    }

    //qDebug() << "QLaylaOSEventLooper::updateWindowState: winid " << ev->dest << " - oldState " << oldState << " - newState " << newState;

    if (oldState != newState) {
        QWindowSystemInterface::handleWindowStateChanged(win, newState);

        if (newState != Qt::WindowMinimized) {
            handleDrawRequest(win, ev->dest);
        }
    } else if (ev->type == EVENT_WINDOW_SHOWN) {
        handleDrawRequest(win, ev->dest);
    }
}

void QLaylaOSEventLooper::run()
{
    struct event_t *ev = NULL;

    while (!m_shouldStop) {
        if (!pending_events_timeout(1)) {
            continue;
        }

        while ((ev = next_event_for_seqid(NULL, 0, 0))) {
            //qDebug() << "QLaylaOSEventLooper::run: eventtype " << ev->type << " - from " << QThread::currentThreadId();

            switch (ev->type) {
                case EVENT_WINDOW_POS_CHANGED:
                    handleWindowMovedEvent(ev);
                    break;

                case EVENT_WINDOW_RESIZE_OFFER:
                    handleWindowResizedEvent(ev);
                    break;

                case EVENT_WINDOW_STATE:
                    updateWindowState(ev);
                    break;

                case EVENT_WINDOW_GAINED_FOCUS:
                    handleWindowActivatedEvent(ev, true);
                    break;

                case EVENT_WINDOW_LOST_FOCUS:
                    handleWindowActivatedEvent(ev, false);
                    break;

                case EVENT_WINDOW_LOWERED:
                case EVENT_WINDOW_RAISED:
                case EVENT_WINDOW_SHOWN:
                case EVENT_WINDOW_HIDDEN:
                    updateWindowState(ev);
                    break;

                case EVENT_WINDOW_CLOSING:
                    handleQuitRequestEvent(ev);
                    break;

                case EVENT_MOUSE:
                    handleMouseEvent(ev);
                    break;

                case EVENT_MOUSE_ENTER:
                    handleEnteredViewEvent(ev);
                    break;

                case EVENT_MOUSE_EXIT:
                    handleExitedViewEvent(ev);
                    break;

                case EVENT_KEY_PRESS:
                    handleKeyEvent(ev, QEvent::KeyPress);
                    break;

                case EVENT_KEY_RELEASE:
                    handleKeyEvent(ev, QEvent::KeyRelease);
                    break;
            }

            free(ev);
            //qApp->processEvents();
        }
    }

    // let the windows know we are done
    QLaylaOSWindow *targetWindow;

    m_mapMutex.lock();

    for (auto i = m_winmap.cbegin(), end = m_winmap.cend(); i != end; ++i) {
        targetWindow = static_cast<QLaylaOSWindow*>(i.value()->handle());
        targetWindow->detachFromLooper();
    }

    m_mapMutex.unlock();

    //qDebug() << "QLaylaOSEventLooper::run: done - from " << QThread::currentThreadId();
    Q_EMIT done();
}

void QLaylaOSEventLooper::stopInputEventLoop()
{
    m_shouldStop = true;
}

void QLaylaOSEventLooper::addWindow(winid_t winid, QWindow *platformWindow)
{
    m_mapMutex.lock();
    m_winmap.insert(winid, platformWindow);
    m_mapMutex.unlock();
}

void QLaylaOSEventLooper::removeWindow(winid_t winid)
{
    m_mapMutex.lock();
    m_winmap.remove(winid);
    m_mapMutex.unlock();
}

void QLaylaOSEventLooper::handleWindowMovedEvent(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);

    if(win == nullptr) {
        return;
    }

    QRect rect(ev->win.x, ev->win.y, win->width(), win->height());

    QWindowSystemInterface::handleGeometryChange(win, rect);
    //QWindowSystemInterface::handleExposeEvent(win, rect);
}

void QLaylaOSEventLooper::handleWindowResizedEvent(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);
    struct window_t *lwin = win_for_winid(ev->dest);

    if(win == nullptr || lwin == nullptr) {
        return;
    }

    int16_t x = ev->win.x, y = ev->win.y;
    uint16_t w = ev->win.w, h = ev->win.h;

    window_resize(lwin, x, y, w, h);

    QRect rect(lwin->x, lwin->y, lwin->w, lwin->h);
    QWindowSystemInterface::handleGeometryChange(win, rect);
    QWindowSystemInterface::handleExposeEvent(win, rect);
    window_invalidate(lwin);
}

void QLaylaOSEventLooper::handleDrawRequest(QWindow *win, winid_t winid)
{
    QRect rect = QRect(0, 0, win->width(), win->height());
    QWindowSystemInterface::handleExposeEvent(win, QRegion(rect));
    window_invalidate(win_for_winid(winid));
}

static inline Qt::WindowType getWindowType(QWindow *win)
{
    return static_cast<Qt::WindowType>(static_cast<int>(win->flags() & Qt::WindowType_Mask));
}

void QLaylaOSEventLooper::handleWindowActivatedEvent(struct event_t *ev, bool activated)
{
    QWindow *win = m_winmap.value(ev->dest);

    if(win != nullptr) {
        if (activated) {
            QWindowSystemInterface::handleWindowActivated(win);
        } else {
            /*
             * When a window loses focus, the server sends an unfocus event to
             * the window that lost focus, followed immediately by a focus
             * event to the newly activated window.
             *
             * The unfocus event causes Qt to unfocus the window, which has
             * some side effects. If the newly focused window is a popup,
             * like the list of a combobox, this causes the popup to be
             * showed and then immediately hidden, before the user can 
             * interact with it.
             *
             * Our workaround here is to only send an unfocus event if:
             *   - the newly activated window is a normal window (e.g.
             *     it is not a popup), or
             *   - the newly activated window is not part of this application.
             *
             * The newly activated window gets its focus event in all cases.
             */
            winid_t inputFocus = get_input_focus();

            if (win == QGuiApplication::focusWindow() && 
                                        inputFocus != ev->dest) {
                QWindow *fwin = inputFocus ? m_winmap.value(inputFocus) : 
                                             nullptr;

                if(fwin == nullptr || getWindowType(fwin) == Qt::Window) {
                    QWindowSystemInterface::handleWindowActivated(nullptr);
                }
            }
        }
    }
}

void QLaylaOSEventLooper::handleQuitRequestEvent(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);

    if(win != nullptr) {
        QWindowSystemInterface::handleCloseEvent(win);
    }
}

void QLaylaOSEventLooper::handleMouseEvent(struct event_t *ev)
{
    //qDebug() << "QLaylaOSEventLooper::handleMouseEvent: buttons " << ev->mouse.buttons << " - win " << ev->dest;
    QWindow *win = m_winmap.value(ev->dest);

    if(win != nullptr) {
        const ButtonState buttonState = getMouseButtons(ev);
        const Qt::KeyboardModifiers keyboardModifiers = 
                                getModifiers(get_modifier_keys());
        const Qt::MouseEventSource source = Qt::MouseEventNotSynthesized;

        const QPoint globalPosition = QPoint(ev->mouse.x + win->x(),
                                                     ev->mouse.y + win->y());
        const QPoint localPosition = QPoint(ev->mouse.x, ev->mouse.y);
        unsigned long time = QWindowSystemInterfacePrivate::eventTime.elapsed();

        if (buttonState.pressed != Qt::NoButton) {
            QWindowSystemInterface::handleMouseEvent(win,
                                localPosition, globalPosition,
                                buttonState.state, buttonState.pressed,
                                QEvent::MouseButtonPress,
                                keyboardModifiers, source);
        } else if (buttonState.released != Qt::NoButton) {
            QWindowSystemInterface::handleMouseEvent(win,
                                localPosition, globalPosition,
                                buttonState.state, buttonState.released,
                                QEvent::MouseButtonRelease,
                                keyboardModifiers, source);
        } else {
            QWindowSystemInterface::handleMouseEvent(win,
                                localPosition, globalPosition,
                                buttonState.state, Qt::NoButton,
                                QEvent::MouseMove,
                                keyboardModifiers, source);
        }

        // https://doc.qt.io/qt-6/qwheelevent.html
        if(ev->mouse.buttons & MOUSE_VSCROLL_DOWN) {
            /*
            QWindowSystemInterface::handleWheelEvent(win, time,
                            localPosition, globalPosition, 
                            (1 * -120), Qt::Vertical, keyboardModifiers);
            */
            //QPoint point = (o == Qt::Vertical) ? QPoint(0, d) : QPoint(d, 0);
            QWindowSystemInterface::handleWheelEvent(win, time, 
                            localPosition, globalPosition,
                            QPoint(), QPoint(0, (1 * -120)), keyboardModifiers);
        }

        if(ev->mouse.buttons & MOUSE_VSCROLL_UP) {
            /*
            QWindowSystemInterface::handleWheelEvent(win, time,
                            localPosition, globalPosition, 
                            (1 * 120), Qt::Vertical, keyboardModifiers);
            */
            //QPoint point = (o == Qt::Vertical) ? QPoint(0, d) : QPoint(d, 0);
            QWindowSystemInterface::handleWheelEvent(win, time,
                            localPosition, globalPosition,
                            QPoint(), QPoint(0, (1 * 120)), keyboardModifiers);
        }
    }
}

void QLaylaOSEventLooper::handleKeyEvent(struct event_t *ev, QEvent::Type type)
{
    QWindow *win = m_winmap.value(ev->dest);

    if(win != nullptr) {
        int code;
        char modkeys = get_modifier_keys();
        const Qt::KeyboardModifiers keyboardModifiers = getModifiers(modkeys);

        /*
         * NOTE: The following conversion only works for Latin-1 chars.
         * TODO: Add support for UTF-8.
         */
        char printable;
        QString text;

        if((printable = get_printable_char(ev->key.code, ev->key.modifiers))) {
            char bytes[2] = { printable, 0 };
            text = QString::fromUtf8(bytes, 1);
        }

        code = QLaylaOSKeyMapper::translateKeyCode(ev->key.code, 
                                            (modkeys & MODIFIER_MASK_NUM));

        QWindowSystemInterface::handleKeyEvent(win, type, code, keyboardModifiers, text);
    }
}

void QLaylaOSEventLooper::handleEnteredViewEvent(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);
    struct window_t *lwin = win_for_winid(ev->dest);

    if(win != nullptr) {
        QWindowSystemInterface::handleEnterEvent(win);
    }

    if(lwin != nullptr) {
        lwin->last_button_state = ev->mouse.buttons;
    }
}

void QLaylaOSEventLooper::handleExitedViewEvent(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);

    if(win != nullptr) {
        QWindowSystemInterface::handleLeaveEvent(win);
    }
}

QT_END_NAMESPACE
