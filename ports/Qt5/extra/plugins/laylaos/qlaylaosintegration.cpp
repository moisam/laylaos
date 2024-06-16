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

#include "qlaylaosintegration.h"
#include "qlaylaosclipboard.h"
#include "qlaylaosrasterbackingstore.h"
#include "qlaylaoswindow.h"
#include "qlaylaosscreen.h"
#include "qlaylaoseventlooper.h"

#include <QCoreApplication>
#include <qpa/qplatformwindow.h>
#include <qpa/qwindowsysteminterface.h>
#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>
#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtServiceSupport/private/qgenericunixservices_p.h>

#include <gui/gui.h>

QT_BEGIN_NAMESPACE

QLaylaOSIntegration::QLaylaOSIntegration(const QStringList &parameters)
    : m_clipboard(new QLaylaOSClipboard)
    , m_fontDb(new QGenericUnixFontDatabase)
    , m_services(new QGenericUnixServices)
{
    Q_UNUSED(parameters);

    char dummy_name[] = "Qt5App";
    char *dummy_argv[] = { dummy_name, NULL };

    gui_init_no_fonts(1, dummy_argv);

    m_screen = new QLaylaOSScreen;
    m_eventlooper = new QLaylaOSEventLooper;

    // notify system about available screen
    QWindowSystemInterface::handleScreenAdded(m_screen);

    QObject::connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), m_eventlooper, SLOT(terminateThread()));
    QObject::connect(m_eventlooper, SIGNAL(done()), m_eventlooper, SLOT(deleteLater()));

    m_eventlooper->start();
}

QLaylaOSIntegration::~QLaylaOSIntegration()
{
    if (m_eventlooper) {
        m_eventlooper->stopInputEventLoop();
        m_eventlooper->wait();
        m_eventlooper = nullptr;
    }

    QWindowSystemInterface::handleScreenRemoved(m_screen);
    m_screen = nullptr;

    if (m_clipboard) {
        delete m_clipboard;
        m_clipboard = nullptr;
    }
}

bool QLaylaOSIntegration::hasCapability(QPlatformIntegration::Capability capability) const
{
    return QPlatformIntegration::hasCapability(capability);
}

QPlatformFontDatabase *QLaylaOSIntegration::fontDatabase() const
{
    //return QPlatformIntegration::fontDatabase();
    return m_fontDb.data();
}

QPlatformServices *QLaylaOSIntegration::services() const
{
    return m_services.data();
}

QPlatformClipboard *QLaylaOSIntegration::clipboard() const
{
    return m_clipboard;
}

QPlatformWindow *QLaylaOSIntegration::createPlatformWindow(QWindow *window) const
{
    QPlatformWindow *platformWindow = new QLaylaOSWindow(window, m_eventlooper);
    //platformWindow->requestActivateWindow();
    return platformWindow;
}

QPlatformBackingStore *QLaylaOSIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QLaylaOSRasterBackingStore(window);
}

QAbstractEventDispatcher *QLaylaOSIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QT_END_NAMESPACE
