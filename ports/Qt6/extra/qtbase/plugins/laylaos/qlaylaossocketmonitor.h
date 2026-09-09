// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSSOCKETMONITOR_H
#define QLAYLAOSSOCKETMONITOR_H

#include <qpa/qplatformintegration.h>
#include <QObject>
#include <QHash>
#include <QtCore/QMutex>
#include <QtCore/QMimeData>
#include <QSocketNotifier>

#include <gui/client/window.h>
#include <gui/event.h>

QT_BEGIN_NAMESPACE

class ButtonState
{
public:
    Qt::MouseButtons state;
    Qt::MouseButton pressed, released;
};

class QLaylaOSSocketMonitor : public QObject
{
    Q_OBJECT

public:
    QLaylaOSSocketMonitor();
    ~QLaylaOSSocketMonitor();
    void startMonitoring();
    bool isMonitoring();

    void addWindow(winid_t winid, QWindow *platformWindow);
    void removeWindow(winid_t winid);
    bool hasWindow(winid_t winid);
    QWindow *getWindow(winid_t winid);
    void updateWindowState(struct event_t *ev);
    void handleKeyEvent(struct event_t *ev, QEvent::Type type);

    void handleIncomingDndEvent(struct event_t *ev);
    void processFileBasedDropEvent(struct event_t *ev, QWindow *win, QPoint globalPos);

    QSocketNotifier *getSocketNotifier() { return m_read_notifier; }

Q_SIGNALS:
    void gonow();
    void mouseEventReceived(QWindow *window,
                            const QPointF &local, const QPointF &global,
                            Qt::MouseButtons state, Qt::MouseButton button,
                            QEvent::Type type, Qt::KeyboardModifiers mods);
    void wheelEventReceived(QWindow *window, ulong timestamp, 
                            const QPointF &local, const QPointF &global, 
                            QPoint pixelDelta, QPoint angleDelta, Qt::KeyboardModifiers mods);
    void keyEventReceived(QWindow *window, QEvent::Type t, int k, 
                            Qt::KeyboardModifiers mods, const QString & text);
    void dragEventReceived(int evtype, winid_t winid, QMimeData *dropData,
                           const QPoint &p, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    void dropEventReceived(winid_t winid, QMimeData *dropData,
                           const QPoint &p, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    void dragResponseReceived(int response);

public Q_SLOTS:
    void readyRead();
    void cleanup();

private:
    QSocketNotifier *m_read_notifier;
    QHash<winid_t, QWindow *>m_winmap;
    QMutex m_mapMutex;
    //QTimer *m_timer;
    bool m_monitoring;
};

QT_END_NAMESPACE

#endif  // QLAYLAOSSOCKETMONITOR_H
