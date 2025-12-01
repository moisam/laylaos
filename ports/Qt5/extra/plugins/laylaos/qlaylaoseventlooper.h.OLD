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

#ifndef QLAYLAOSEVENTLOOPER_H
#define QLAYLAOSEVENTLOOPER_H

#include <qpa/qplatformintegration.h>
#include <QThread>
#include <QHash>
#include <QtCore/QMutex>

#include <gui/client/window.h>
#include <gui/event.h>

QT_BEGIN_NAMESPACE

class ButtonState
{
public:
    Qt::MouseButtons state;
    Qt::MouseButton pressed, released;
};


//class QLaylaOSIntegration;

class QLaylaOSEventLooper : public QThread
{
    Q_OBJECT

public:
    QLaylaOSEventLooper();
    //virtual ~QLaylaOSEventLooper();
    void addWindow(winid_t winid, QWindow *platformWindow);
    void removeWindow(winid_t winid);

    //void setPlatformIntegration(QLaylaOSIntegration *platformIntegration);
    void stopInputEventLoop();

    ButtonState getMouseButtons(struct event_t *ev);
    //Qt::MouseButtons getMouseButtons(struct event_t *ev) const;
    Qt::KeyboardModifiers getModifiers(char modkeys) const;
    //void checkWindowState(struct event_t *ev);
    void updateWindowState(struct event_t *ev);

    void handleWindowMovedEvent(struct event_t *ev);
    void handleWindowResizedEvent(struct event_t *ev);
    void handleDrawRequest(QWindow *win, winid_t winid);
    void handleWindowActivatedEvent(struct event_t *ev, bool activated);
    void handleWindowMinimizedEvent(struct event_t *ev, bool minimize);
    void handleWindowZoomedEvent(struct event_t *ev);
    void handleQuitRequestEvent(struct event_t *ev);
    void handleMouseEvent(struct event_t *ev);
    void handleKeyEvent(struct event_t *ev, QEvent::Type type);
    void handleEnteredViewEvent(struct event_t *ev);
    void handleExitedViewEvent(struct event_t *ev);

Q_SIGNALS:
    void done();

private Q_SLOTS:
    void terminateThread();

protected:
    void run();

private:
    bool m_shouldStop;
    QHash<winid_t, QWindow *>m_winmap;
    //QLaylaOSIntegration *m_platformIntegration;
    QMutex m_mapMutex;
};

QT_END_NAMESPACE

#endif  // QLAYLAOSEVENTLOOPER_H
