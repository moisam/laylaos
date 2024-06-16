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

#ifndef QLAYLAOSWINDOW_H
#define QLAYLAOSWINDOW_H

#include <qpa/qplatformwindow.h>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <gui/window-defs.h>
#include <gui/client/window.h>

QT_BEGIN_NAMESPACE

class QLaylaOSEventLooper;

class QLaylaOSWindow : public QObject, public QPlatformWindow
{
    Q_OBJECT

public:
    explicit QLaylaOSWindow(QWindow *window, QLaylaOSEventLooper *eventlooper);
    virtual ~QLaylaOSWindow();

    QRect geometry() const override;
    void setGeometry(const QRect &rect) override;
    QMargins frameMargins() const override;
    void setVisible(bool visible) override;

    bool isExposed() const override;
    bool isActive() const override;

    WId winId() const override;
    struct window_t *nativeHandle() const;

    void requestActivateWindow() override;
    void setWindowState(Qt::WindowStates state) override;
    void setWindowFlags(Qt::WindowFlags flags) override;
    void setWindowTitle(const QString &title) override;

    bool setKeyboardGrabEnabled(bool grab) override;
    bool setMouseGrabEnabled(bool grab) override;

    void raise() override;
    void lower() override;
    void propagateSizeHints() override;

    void detachFromLooper();

protected:
    struct window_t *m_window;
    QLaylaOSEventLooper *m_eventlooper;

private:
    Qt::WindowStates m_windowState;
};

QT_END_NAMESPACE

#endif
