/***************************************************************************
**
** Copyright (C) 2024, 2025 Mohammed Isam <mohammed_isam1984@yahoo.com>
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

#ifndef QLAYLAOSSOCKETMONITOR_H
#define QLAYLAOSSOCKETMONITOR_H

#include <qpa/qplatformintegration.h>
#include <QObject>
#include <QHash>
#include <QtCore/QMutex>
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
    void startMonitoring();
    bool isMonitoring();

    void addWindow(winid_t winid, QWindow *platformWindow);
    void removeWindow(winid_t winid);
    void updateWindowState(struct event_t *ev);
    void handleKeyEvent(struct event_t *ev, QEvent::Type type);

Q_SIGNALS:
    void gonow();

private Q_SLOTS:
    void readyRead();

private:
    QSocketNotifier *m_read_notifier;
    QHash<winid_t, QWindow *>m_winmap;
    QMutex m_mapMutex;
    //QTimer *m_timer;
    bool m_monitoring;
};

QT_END_NAMESPACE

#endif  // QLAYLAOSSOCKETMONITOR_H
