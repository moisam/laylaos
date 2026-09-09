// Copyright (C) 2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSCLIENTEVENTS_H
#define QLAYLAOSCLIENTEVENTS_H

#include <kernel/mouse.h>
#include <gui/event.h>

QT_BEGIN_NAMESPACE

enum LaylaOSCustomEventTypes
{
    // Child window events sent to parent window
    LaylaOSChildEventType = QEvent::MaxUser - 1,
    LaylaOSChildTitleEventType = QEvent::MaxUser - 2,
    LaylaOSChildIconEventType = QEvent::MaxUser - 3,

    // Systray manager events sent to client apps
    LaylaOSSystrayClickEventType = QEvent::MaxUser - 4,
    LaylaOSSystrayDoubleClickEventType = QEvent::MaxUser - 5,

    // Client systray requests sent to systray manager
    LaylaOSSystrayRequestType = QEvent::MaxUser - 6,
    LaylaOSSystrayIconRequestType = QEvent::MaxUser - 7,
    LaylaOSSystrayTooltipRequestType = QEvent::MaxUser - 8,
    LaylaOSSystrayBoundsRequestType = QEvent::MaxUser - 9,
    LaylaOSSystrayMsgRequestType = QEvent::MaxUser - 10,

    // Application-defined requests and events
    LaylaOSCustomEventType = QEvent::MaxUser - 11,
};

class LaylaOSChildEvent : public QEvent
{
public:
    LaylaOSChildEvent(winid_t child_winid, uint32_t evtype) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSChildEventType))
        , m_childWinid(child_winid), m_evType(evtype) {}

    winid_t m_childWinid;
    uint32_t m_evType;
};

class LaylaOSChildTitleEvent : public QEvent
{
public:
    LaylaOSChildTitleEvent(winid_t child_winid, QString title) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSChildTitleEventType))
        , m_childWinid(child_winid), m_title(title) {}

    winid_t m_childWinid;
    QString m_title;
};

class LaylaOSChildIconEvent : public QEvent
{
public:
    LaylaOSChildIconEvent(winid_t child_winid, uint32_t restype, resid_t resid) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSChildIconEventType))
        , m_childWinid(child_winid), m_restype(restype), m_resid(resid) {}

    winid_t m_childWinid;
    uint32_t m_restype;
    resid_t m_resid;
};

class LaylaOSSystrayClickEvent : public QEvent
{
public:
    LaylaOSSystrayClickEvent(winid_t winid, mouse_buttons_t b, int x, int y) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSSystrayClickEventType))
        , m_winid(winid), m_buttons(b), m_x(x), m_y(y) {}

    winid_t m_winid;
    mouse_buttons_t m_buttons;
    int m_x, m_y;
};

class LaylaOSSystrayDoubleClickEvent : public QEvent
{
public:
    LaylaOSSystrayDoubleClickEvent(winid_t winid, int x, int y) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSSystrayDoubleClickEventType))
        , m_winid(winid), m_x(x), m_y(y) {}

    winid_t m_winid;
    int m_x, m_y;
};

class LaylaOSSystrayRequest : public QEvent
{
public:
    LaylaOSSystrayRequest(winid_t winid, uint32_t evtype) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSSystrayRequestType))
        , m_winid(winid), m_evType(evtype) {}

    winid_t m_winid;
    uint32_t m_evType;
};

class LaylaOSSystrayIconRequest : public QEvent
{
public:
    LaylaOSSystrayIconRequest(winid_t winid, QIcon icon) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSSystrayIconRequestType))
        , m_winid(winid), m_icon(icon) {}

    winid_t m_winid;
    QIcon m_icon;
};

class LaylaOSSystrayTooltipRequest : public QEvent
{
public:
    LaylaOSSystrayTooltipRequest(winid_t winid, QString tooltip) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSSystrayTooltipRequestType))
        , m_winid(winid), m_tooltip(tooltip) {}

    winid_t m_winid;
    QString m_tooltip;
};

class LaylaOSSystrayBoundsRequest : public QEvent
{
public:
    LaylaOSSystrayBoundsRequest(winid_t winid, uint32_t seqid) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSSystrayBoundsRequestType))
        , m_winid(winid), m_seqid(seqid) {}

    winid_t m_winid;
    uint32_t m_seqid;
};

class LaylaOSSystrayMsgRequest : public QEvent
{
public:
    LaylaOSSystrayMsgRequest(winid_t winid, QString title, QString msg, int type, int duration, QPixmap pixmap) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSSystrayMsgRequestType))
        , m_winid(winid), m_title(title), m_msg(msg), m_type(type)
        , m_duration(duration), m_pixmap(pixmap) {}

    winid_t m_winid;
    QString m_title, m_msg;
    int m_type, m_duration;
    QPixmap m_pixmap;
};

/*
 * XXX: The receiving application is responsible for freeing the passed event struct!
 */

class LaylaOSCustomEvent : public QEvent
{
public:
    LaylaOSCustomEvent(struct event_t *ev) 
        : QEvent(static_cast<QEvent::Type>(LaylaOSCustomEventType))
        , m_ev(ev) {}

    struct event_t *m_ev;
};

QT_END_NAMESPACE

#endif  // QLAYLAOSCLIENTEVENTS_H
