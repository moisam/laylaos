// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QThread>
#include <QWindow>
#include <QtCore/QMimeData>
#include <QtCore/QFile>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformdrag.h>

#include <QDebug>

#include "qlaylaossocketmonitor.h"
#include "qlaylaoskeymapper.h"
#include "qlaylaosintegration.h"
#include "qlaylaoswindow.h"
#include "qlaylaosscreen.h"
#include "qlaylaoscursor.h"
#include "qlaylaosplatformtheme.h"
#include "qlaylaosclientevents.h"
#include "qlaylaossystemtrayicon.h"

#include "private/qguiapplication_p.h"

#include <gui/keys.h>
#include <gui/kbd.h>
#include <gui/client/dragndrop.h>
#include <gui/client/systray.h>

QT_BEGIN_NAMESPACE

QLaylaOSSocketMonitor::QLaylaOSSocketMonitor()
    : m_monitoring(false)
{
}

QLaylaOSSocketMonitor::~QLaylaOSSocketMonitor()
{
}

void QLaylaOSSocketMonitor::cleanup()
{
    if (m_read_notifier != nullptr) {
        m_read_notifier->setEnabled(false);
        delete m_read_notifier;
        m_read_notifier = nullptr;
    }

    m_monitoring = false;
}

bool QLaylaOSSocketMonitor::isMonitoring()
{
    //qDebug() << "QLaylaOSSocketMonitor::isMonitoring: " << m_monitoring;
    return m_monitoring;
}

void QLaylaOSSocketMonitor::startMonitoring()
{
    if (m_monitoring) {
        return;
    }

    //qDebug() << "QLaylaOSSocketMonitor::startMonitoring: 1";
    m_read_notifier = new QSocketNotifier(__global_gui_data.serverfd, QSocketNotifier::Read);
    m_read_notifier->setEnabled(true);
    //qDebug() << "QLaylaOSSocketMonitor::startMonitoring: 2";

    QObject::connect(m_read_notifier, SIGNAL(activated(int)), this, SLOT(readyRead()));
    QObject::connect(this, SIGNAL(gonow()), this, SLOT(readyRead()));
    m_monitoring = true;
    //qDebug() << "QLaylaOSSocketMonitor::startMonitoring: monitoring fd " << __global_gui_data.serverfd;
}


#define BUTTON_PRESSED(which)   \
    (!(obuttons & MOUSE_ ## which ## _DOWN) &&  \
      (nbuttons & MOUSE_ ## which ## _DOWN))

#define BUTTON_RELEASED(which)  \
    ((obuttons & MOUSE_ ## which ## _DOWN) &&  \
     !(nbuttons & MOUSE_ ## which ## _DOWN))

static ButtonState getMouseButtons(struct event_t *ev)
{
    struct window_t *lwin = win_for_winid(ev->dest);

    Qt::MouseButton pressed = Qt::NoButton;
    Qt::MouseButton released = Qt::NoButton;
    Qt::MouseButtons state = {};

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

static Qt::KeyboardModifiers getModifiers(char modkeys)
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

static inline Qt::WindowType getWindowType(QWindow *win)
{
    return static_cast<Qt::WindowType>(static_cast<int>(win->flags() & Qt::WindowType_Mask));
}

static inline void handleDrawRequest(QWindow *win, winid_t winid)
{
    QRect rect = QRect(0, 0, win->width(), win->height());
    QWindowSystemInterface::handleExposeEvent(win, QRegion(rect));
    window_invalidate(win_for_winid(winid));
}

void QLaylaOSSocketMonitor::updateWindowState(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);
    struct window_t *lwin = win_for_winid(ev->dest);
    Qt::WindowStates oldState, newState;
    int visible = 1;

    if(win == nullptr) {
        return;
    }

    oldState = win->windowState();

    switch (ev->winst.state) {
        case WINDOW_STATE_MAXIMIZED: newState = Qt::WindowMaximized; break;
        case WINDOW_STATE_MINIMIZED: newState = Qt::WindowMinimized; visible = 0; break;
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

    //qDebug() << "QLaylaOSSocketMonitor::updateWindowState: winid " << ev->dest << " - oldState " << oldState << " - newState " << newState;

    if(lwin != nullptr) {
        lwin->visible = visible;
    }

    if (oldState != newState) {
        QWindowSystemInterface::handleWindowStateChanged(win, newState);

        if (newState != Qt::WindowMinimized && win->isVisible()) {
            handleDrawRequest(win, ev->dest);
        }
    } else if (ev->type == EVENT_WINDOW_SHOWN) {
        handleDrawRequest(win, ev->dest);
    }
}

void QLaylaOSSocketMonitor::readyRead()
{
    QWindow *win;
    struct window_t *lwin;
    struct event_t *ev = NULL;
    bool dofree;
    //bool hasdata = false;

    //qDebug() << "QLaylaOSSocketMonitor::readyRead: checking";
    m_read_notifier->setEnabled(false);

    while ((ev = next_event_for_seqid(NULL, 0, 0))) {
        //qDebug() << "QLaylaOSSocketMonitor::readyRead: eventtype " << ev->type;
        //hasdata = true;
        dofree = true;

        switch (ev->type) {
            case EVENT_WINDOW_POS_CHANGED:
            {
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_WINDOW_POS_CHANGED" << ev->dest;
                win = m_winmap.value(ev->dest);
                lwin = win_for_winid(ev->dest);

                if(win == nullptr || lwin == nullptr) {
                    break;
                }

                QRect rect(ev->win.x, ev->win.y, win->width(), win->height());

                lwin->x = ev->win.x;
                lwin->y = ev->win.y;

                QWindowSystemInterface::handleGeometryChange(win, rect);
                //QWindowSystemInterface::handleExposeEvent(win, rect);
                break;
            }

            case EVENT_WINDOW_RESIZE_OFFER:
            {
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_WINDOW_RESIZE_OFFER" << ev->dest;
                win = m_winmap.value(ev->dest);
                lwin = win_for_winid(ev->dest);

                if(win == nullptr || lwin == nullptr) {
                    break;
                }

                int16_t x = ev->win.x, y = ev->win.y;
                uint16_t w = ev->win.w, h = ev->win.h;

                //qDebug() << "QLaylaOSSocketMonitor::readyRead:" << x << ", " << y << ", " << w << ", " << h;

                window_resize(lwin, x, y, w, h);

                QRect rect(lwin->x, lwin->y, lwin->w, lwin->h);
                QWindowSystemInterface::handleGeometryChange(win, rect);
                QRect rect2(0, 0, lwin->w, lwin->h);
                QWindowSystemInterface::handleExposeEvent(win, rect2);
                ////window_invalidate(lwin);
                break;
            }

            case EVENT_WINDOW_GAINED_FOCUS:
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_WINDOW_GAINED_FOCUS" << ev->dest;
                win = m_winmap.value(ev->dest);
                lwin = win_for_winid(ev->dest);

                if(lwin != nullptr) {
                    lwin->flags |= WINDOW_IS_FOCUSED;
                }

                if(win != nullptr) {
                    Qt::WindowType type = win->type();

                    if (type != Qt::Popup && type != Qt::ToolTip) {
                        //qDebug() << "QLaylaOSSocketMonitor::readyRead: activated type " << getWindowType(win);
                        QWindowSystemInterface::handleFocusWindowChanged(win);
                    }
                }

                break;

            case EVENT_WINDOW_LOST_FOCUS:
            {
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_WINDOW_LOST_FOCUS dest " << ev->dest << ", src " << ev->src;
                QWindow *winsrc = m_winmap.value(ev->src);
                win = m_winmap.value(ev->dest);
                lwin = win_for_winid(ev->dest);

                if (lwin != nullptr) {
                    lwin->flags &= ~WINDOW_IS_FOCUSED;

                    // we want to close popup menus from systray widgets but not
                    // the applications menu spawned by the bottom panel itself
                    if (lwin->owner_winid && lwin->owner_winid == systray_manager_winid && win && !winsrc) {
                        QWindowSystemInterface::handleCloseEvent(win);
                        //QWindowSystemInterface::handleWindowActivated(nullptr);
                    }
                }

                // focus went to a non-Qt window
                if (winsrc == nullptr) {
                    //qDebug() << "QLaylaOSSocketMonitor::readyRead: de-activated";
                    QWindowSystemInterface::handleFocusWindowChanged(nullptr);
                }

                break;
            }

            case EVENT_WINDOW_LOWERED:
            case EVENT_WINDOW_RAISED:
            case EVENT_WINDOW_SHOWN:
            case EVENT_WINDOW_HIDDEN:
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_WINDOW_* (" << ev->type << "), " << ev->dest;
                updateWindowState(ev);
                break;

            case EVENT_WINDOW_STATE:
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_WINDOW_STATE" << ev->dest;
                updateWindowState(ev);
                break;

            case EVENT_WINDOW_CLOSING:
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_WINDOW_CLOSING" << ev->dest;
                win = m_winmap.value(ev->dest);

                if(win != nullptr) {
                    QWindowSystemInterface::handleCloseEvent(win);
                }

                break;

            case EVENT_MOUSE_ENTER:
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_MOUSE_ENTER" << ev->dest;
                win = m_winmap.value(ev->dest);
                lwin = win_for_winid(ev->dest);

                if(win != nullptr) {
                    QWindowSystemInterface::handleEnterEvent(win);
                }

                if(lwin != nullptr) {
                    lwin->last_button_state = ev->mouse.buttons;
                }

                QLaylaOSIntegration::instance()->getScreen()->getCursor()->updateMousePos(ev->mouse.x + win->x(), ev->mouse.y + win->y());

                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_MOUSE_ENTER" << ev->dest << "end";
                break;

            case EVENT_MOUSE_EXIT:
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_MOUSE_EXIT" << ev->dest;
                win = m_winmap.value(ev->dest);

                if(win != nullptr) {
                    QWindowSystemInterface::handleLeaveEvent(win);
                }

                QLaylaOSIntegration::instance()->getScreen()->getCursor()->updateMousePos(-1, -1);

                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_MOUSE_EXIT" << ev->dest << "end";
                break;

            case EVENT_MOUSE:
            {
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_MOUSE" << ev->dest;
                win = m_winmap.value(ev->dest);

                if(win == nullptr) {
                    break;
                }

                const ButtonState buttonState = getMouseButtons(ev);
                const Qt::KeyboardModifiers keyboardModifiers = 
                                        getModifiers(ev->mouse.modifiers);
                //const Qt::MouseEventSource source = Qt::MouseEventNotSynthesized;

                const QPoint globalPosition = QPoint(ev->mouse.x + win->x(),
                                                     ev->mouse.y + win->y());
                const QPoint localPosition = QPoint(ev->mouse.x, ev->mouse.y);

                QLaylaOSIntegration::instance()->getScreen()->getCursor()->updateMousePos(globalPosition.x(), globalPosition.y());

                if (buttonState.pressed != Qt::NoButton) {
                    //qDebug() << "QLaylaOSSocketMonitor::readyRead: MOUSE_PRESS " << ev->dest;
                    Q_EMIT mouseEventReceived(win, localPosition, globalPosition, 
                                              buttonState.state, buttonState.pressed,
                                              QEvent::MouseButtonPress,
                                              keyboardModifiers);
                } else if (buttonState.released != Qt::NoButton) {
                    //qDebug() << "QLaylaOSSocketMonitor::readyRead: MOUSE_RELEASE " << ev->dest;
                    Q_EMIT mouseEventReceived(win, localPosition, globalPosition, 
                                              buttonState.state, buttonState.released,
                                              QEvent::MouseButtonRelease,
                                              keyboardModifiers);
                } else {
                    Q_EMIT mouseEventReceived(win, localPosition, globalPosition, 
                                              buttonState.state, Qt::NoButton,
                                              QEvent::MouseMove,
                                              keyboardModifiers);
                }

                // https://doc.qt.io/qt-6/qwheelevent.html
                if(ev->mouse.buttons & MOUSE_VSCROLL_DOWN) {
                    unsigned long time = QWindowSystemInterfacePrivate::eventTime.elapsed();
                    Q_EMIT wheelEventReceived(win, time, localPosition, globalPosition,
                                              QPoint(), QPoint(0, (1 * -120)), keyboardModifiers);
                }

                if(ev->mouse.buttons & MOUSE_VSCROLL_UP) {
                    unsigned long time = QWindowSystemInterfacePrivate::eventTime.elapsed();
                    Q_EMIT wheelEventReceived(win, time, localPosition, globalPosition,
                                              QPoint(), QPoint(0, (1 * 120)), keyboardModifiers);
                }
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_MOUSE" << ev->dest << "end";
                break;
            }

            case EVENT_KEY_PRESS:
                handleKeyEvent(ev, QEvent::KeyPress);
                break;

            case EVENT_KEY_RELEASE:
                handleKeyEvent(ev, QEvent::KeyRelease);
                break;

            case EVENT_SCREEN_RES_CHANGED:
            {
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_SCREEN_RES_CHANGED";
                QLaylaOSScreen *screen = QLaylaOSIntegration::instance()->getScreen();
                screen->handleScreenResChange();
                break;
            }

            case EVENT_COLOR_THEME_DATA:
                set_color_theme(ev);
                QLaylaOSPlatformTheme::instance()->handleThemeChange();
                break;

            case EVENT_DRAG_ENTER:
            case EVENT_DRAG_MOVE:
            case EVENT_DRAG_LEAVE:
            case EVENT_DRAG_DROP:
                handleIncomingDndEvent(ev);
                break;

            case EVENT_DRAG_RESPONSE:
                Q_EMIT dragResponseReceived(ev->winst.state);
                break;

            case EVENT_CHILD_WINDOW_CREATED:
            case EVENT_CHILD_WINDOW_SHOWN:
            case EVENT_CHILD_WINDOW_HIDDEN:
            case EVENT_CHILD_WINDOW_RAISED:
            case EVENT_CHILD_WINDOW_DESTROYED:
                win = m_winmap.value(ev->dest);
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_CHILD_*" << ev->dest << "win" << win;

                if (win != nullptr) {
                    QCoreApplication::postEvent(win, new LaylaOSChildEvent(ev->src, ev->type));
                }
                break;

            case EVENT_CHILD_WINDOW_ICON_SET:
                win = m_winmap.value(ev->dest);

                if (win != nullptr) {
                    struct event_res_t *evbuf = (struct event_res_t *)ev;
                    QCoreApplication::postEvent(win, 
                            new LaylaOSChildIconEvent(ev->src, evbuf->restype, evbuf->resid));
                }
                break;

            case EVENT_CHILD_WINDOW_TITLE_SET:
                win = m_winmap.value(ev->dest);

                if (win != nullptr) {
                    QCoreApplication::postEvent(win, 
                            new LaylaOSChildTitleEvent(ev->src, 
                                QString((char *)((struct event_buf_t *)ev)->buf)));
                }
                break;

            case REQUEST_SYSTRAY_ADD:
            case REQUEST_SYSTRAY_REMOVE:
            case REQUEST_SYSTRAY_SHOW:
            case REQUEST_SYSTRAY_HIDE:
                win = m_winmap.value(ev->dest);
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: EVENT_SYSTRAY_*" << ev->type << ev->dest << "win" << win;

                if (win != nullptr) {
                    QCoreApplication::postEvent(win, new LaylaOSSystrayRequest(ev->src, ev->type));
                }
                break;

            case REQUEST_SYSTRAY_SET_ICON:
                win = m_winmap.value(ev->dest);
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: REQUEST_SYSTRAY_SET_ICON" << ev->dest << "win" << win;

                if (win != nullptr) {
                    struct event_buf_t *evbuf = (struct event_buf_t *)ev;
                    QPixmap pixmap;
                    QByteArray rawdata = 
                            QByteArray(reinterpret_cast<const char*>(evbuf->buf), evbuf->bufsz);

                    pixmap.loadFromData(rawdata, "PNG");

                    QCoreApplication::postEvent(win, 
                            new LaylaOSSystrayIconRequest(ev->src, QIcon(pixmap)));
                }
                break;

            case REQUEST_SYSTRAY_SET_TOOLTIP:
                win = m_winmap.value(ev->dest);
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: REQUEST_SYSTRAY_SET_TOOLTIP" << ev->dest << "win" << win;

                if (win != nullptr) {
                    QCoreApplication::postEvent(win, 
                            new LaylaOSSystrayTooltipRequest(ev->src, 
                                QString((char *)((struct event_buf_t *)ev)->buf)));
                }
                break;

            case REQUEST_SYSTRAY_GET_BOUNDS:
                win = m_winmap.value(ev->dest);
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: REQUEST_SYSTRAY_GET_BOUNDS" << ev->dest << "win" << win;

                if (win != nullptr) {
                    QCoreApplication::postEvent(win, new LaylaOSSystrayBoundsRequest(ev->src, ev->seqid));
                }
                break;

            case REQUEST_SYSTRAY_SHOW_MESSAGE:
            {
                win = m_winmap.value(ev->dest);
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: REQUEST_SYSTRAY_SHOW_MESSAGE" << ev->dest << "win" << win;
                if (win == nullptr) break;

                struct event_traymsg_t *evbuf = (struct event_traymsg_t *)ev;
                QString title = QString(evbuf->data);
                QString msg = QString(evbuf->data + evbuf->titlelen);
                QPixmap pixmap;

                if(evbuf->titlelen + evbuf->msglen < evbuf->datasz)
                {
                    size_t strlen = evbuf->titlelen + evbuf->msglen;
                    QByteArray rawdata = 
                            QByteArray(reinterpret_cast<const char*>(evbuf->data + strlen), 
                                                                        evbuf->datasz - strlen);

                    pixmap.loadFromData(rawdata, "PNG");
                }

                QCoreApplication::postEvent(win, new LaylaOSSystrayMsgRequest(ev->src, title, msg, evbuf->type, evbuf->msgduration, pixmap));
                break;
            }

            case EVENT_SYSTRAY_CLICK:
            case EVENT_SYSTRAY_DOUBLE_CLICK:
                //qDebug() << "QLaylaOSSocketMonitor::readyRead: REQUEST_SYSTRAY_CLICK_*" << ev->dest;
                QLaylaOSSystemTrayIcon::dispatchClientEvent(ev);
                break;

            // Handle application-defined requests and events
            default:
                //if(ev->type >= (uint32_t)REQUEST_APPLICATION_PRIVATE)
                {
                    win = m_winmap.value(ev->dest);

                    if (win != nullptr) {
                        dofree = false;
                        QCoreApplication::postEvent(win, new LaylaOSCustomEvent(ev));
                    }
                }
                break;
        }

        if (dofree) free(ev);
        //qApp->processEvents();
    }

    /*
    if (hasdata) {
        QWindowSystemInterface::flushWindowSystemEvents();
    }
    */

    m_read_notifier->setEnabled(true);
}

void QLaylaOSSocketMonitor::addWindow(winid_t winid, QWindow *platformWindow)
{
    m_mapMutex.lock();
    m_winmap.insert(winid, platformWindow);
    m_mapMutex.unlock();
}

void QLaylaOSSocketMonitor::removeWindow(winid_t winid)
{
    m_mapMutex.lock();
    m_winmap.remove(winid);
    m_mapMutex.unlock();
}

bool QLaylaOSSocketMonitor::hasWindow(winid_t winid)
{
    m_mapMutex.lock();
    bool res = m_winmap.value(winid) != nullptr;
    m_mapMutex.unlock();

    return res;
}

QWindow *QLaylaOSSocketMonitor::getWindow(winid_t winid)
{
    m_mapMutex.lock();
    QWindow *win = m_winmap.value(winid);
    m_mapMutex.unlock();

    return win;
}

void QLaylaOSSocketMonitor::handleKeyEvent(struct event_t *ev, QEvent::Type type)
{
    QWindow *win = m_winmap.value(ev->dest);
    //qDebug() << "QLaylaOSSocketMonitor::handleKeyEvent: win " << win << ", ev->dest " << ev->dest;

    if(win != nullptr) {
        int code;
        char modkeys = ev->key.modifiers; // get_modifier_keys();
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

        //qDebug() << "QLaylaOSSocketMonitor::handleKeyEvent: code " << code << ", mods " << keyboardModifiers;

        Q_EMIT keyEventReceived(win, type, code, keyboardModifiers, text);
    }
}

static Qt::MouseButtons mouseButtonsFromDragEvent(mouse_buttons_t b)
{
    Qt::MouseButtons state = {};

    state.setFlag(Qt::LeftButton, (b & MOUSE_LBUTTON_DOWN) ? true : false);
    state.setFlag(Qt::RightButton, (b & MOUSE_RBUTTON_DOWN) ? true : false);
    state.setFlag(Qt::MiddleButton, (b & MOUSE_MBUTTON_DOWN) ? true : false);

    return state;
}

void QLaylaOSSocketMonitor::handleIncomingDndEvent(struct event_t *ev)
{
    QWindow *win = m_winmap.value(ev->dest);

    if(win == nullptr) {
        return;
    }

    struct event_res_t *evres = (struct event_res_t *)ev;
    QPoint globalPos = QPoint(evres->dnd.mousex + win->x(), evres->dnd.mousey + win->y());

    if (ev->type == EVENT_DRAG_ENTER || ev->type == EVENT_DRAG_MOVE) {
        //qDebug() << "QLaylaOSSocketMonitor::handleIncomingDndEvent: ENTER win " << ev->dest;
        char *buf = evres->data;
        char *lbuf = buf + evres->datasz;

        QMimeData *frameMimeData = new QMimeData();

        // the server sends mime types as a series of NULL-terminated strings,
        // the last one of which is just a '\0'
        while (buf < lbuf) {
            if (*buf == '\0') break;

            frameMimeData->setData(QString::fromUtf8(buf), QByteArray());

            while (*buf++) ;
        }

        Q_EMIT dragEventReceived(evres->type, ev->dest, frameMimeData, globalPos,
                                 mouseButtonsFromDragEvent(evres->dnd.buttons), 
                                 getModifiers(evres->dnd.modifiers));
    } else if (ev->type == EVENT_DRAG_DROP) {
        //qDebug() << "QLaylaOSSocketMonitor::handleIncomingDndEvent: DROP win " << ev->dest;
        processFileBasedDropEvent(ev, win, globalPos);
    } else if (ev->type == EVENT_DRAG_LEAVE) {
        //qDebug() << "QLaylaOSSocketMonitor::handleIncomingDndEvent: LEAVE win " << ev->dest;
        Q_EMIT dragEventReceived(evres->type, ev->dest, nullptr, QPoint(), Qt::NoButton, Qt::NoModifier);
    }
}

void QLaylaOSSocketMonitor::processFileBasedDropEvent(struct event_t *ev, QWindow *win,
                                                      QPoint globalPos)
{
    struct event_res_t *evres = (struct event_res_t *)ev;

    QString tempPath = QString::fromUtf8(evres->data);
    //qDebug() << "QLaylaOSSocketMonitor::processFileBasedDropEvent: tempPath " << tempPath;

    QFile file(tempPath);

    if (!file.open(QIODevice::ReadOnly)) {
        return; // File vanished or access denied
    }

    QMimeData *mimeData = new QMimeData();
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 formatCount = 0;
    in >> formatCount;
    //qDebug() << "QLaylaOSSocketMonitor::processFileBasedDropEvent: formatCount " << formatCount;

    // Unpack every format block out of the disk/RAM stream
    for (quint32 i = 0; i < formatCount; ++i) {
        quint32 nameLen = 0;
        in >> nameLen;
        QByteArray formatNameBytes(nameLen, 0);
        in.readRawData(formatNameBytes.data(), nameLen);
        QString formatStr = QString::fromUtf8(formatNameBytes);

        quint32 dataLen = 0;
        in >> dataLen;
        QByteArray payloadBytes(dataLen, 0);
        in.readRawData(payloadBytes.data(), dataLen);
        //qDebug() << "QLaylaOSSocketMonitor::processFileBasedDropEvent: formatStr " << formatStr << ", dataLen " << dataLen;

        mimeData->setData(formatStr, payloadBytes);
    }

    file.close();

    // delete the file from /tmp
    file.remove(); 

    if (win) {
        Q_EMIT dropEventReceived(ev->dest, mimeData, globalPos,
                                 mouseButtonsFromDragEvent(evres->dnd.buttons), 
                                 getModifiers(evres->dnd.modifiers));
    } else {
        delete mimeData; // Avoid leaks if the window was destroyed mid-drop
    }
}

QT_END_NAMESPACE
