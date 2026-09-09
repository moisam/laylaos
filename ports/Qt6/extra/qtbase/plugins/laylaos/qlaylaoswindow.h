// Copyright (C) 2024-2026 Mohammed Isam
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSWINDOW_H
#define QLAYLAOSWINDOW_H

#include <qpa/qplatformwindow.h>
#include <QtCore/QThread>
#include <QtCore/QTimer>

#include <gui/window-defs.h>
#include <gui/client/window.h>

QT_BEGIN_NAMESPACE

class QLaylaOSSocketMonitor;

class QLaylaOSWindow : public QObject, public QPlatformWindow
{
    Q_OBJECT

public:
    explicit QLaylaOSWindow(QWindow *window, QLaylaOSSocketMonitor *socketmonitor);
    virtual ~QLaylaOSWindow();

    /*
    QRect initialGeometry(const QWindow *w, const QRect &initialGeometry, 
                                      int defaultWidth, int defaultHeight);
    */
    void init();
    bool hasTransparentBackground() const;

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
    void setWindowIcon(const QIcon &icon) override;

    bool setKeyboardGrabEnabled(bool grab) override;
    bool setMouseGrabEnabled(bool grab) override;

    void raise() override;
    void lower() override;
    void propagateSizeHints() override;

    qreal devicePixelRatio() const override;

protected:
    struct window_t *m_window;
    QLaylaOSSocketMonitor *m_socketmonitor;
    Qt::WindowStates m_windowState;
    bool m_mouse_grabbed;
    bool m_kbd_grabbed;
};

QT_END_NAMESPACE

#endif
