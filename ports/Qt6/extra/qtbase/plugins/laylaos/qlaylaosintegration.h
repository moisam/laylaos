// Copyright (C) 2024 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSINTEGRATION_H
#define QLAYLAOSINTEGRATION_H

#include <qpa/qplatformintegration.h>
#include <qpa/qplatformnativeinterface.h>
#include <QtCore/QMimeData>

#include <QObject>

#if QT_CONFIG(opengl)
#include <EGL/egl.h>
#endif

#include <gui/window-defs.h>

QT_BEGIN_NAMESPACE

#if !defined(QT_NO_CLIPBOARD)
class QLaylaOSClipboard;
#endif
class QLaylaOSScreen;
class QLaylaOSSocketMonitor;
class QLaylaOSDrag;

class QLaylaOSIntegration : public QObject, public QPlatformIntegration
{
    Q_OBJECT
public:
    explicit QLaylaOSIntegration(const QStringList &paramList);
    ~QLaylaOSIntegration();

    static QLaylaOSIntegration *instance() { return ms_instance; }

    bool hasCapability(QPlatformIntegration::Capability cap) const override;

    QPlatformWindow *createPlatformWindow(QWindow *window) const override;
    QPlatformWindow *createForeignWindow(QWindow *window, WId nativeHandle) const override;
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const override;
    QAbstractEventDispatcher *createEventDispatcher() const override;

#if QT_CONFIG(opengl)
    EGLDisplay eglDisplay() const { return m_eglDisplay; }
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const override;
#endif

    QPlatformFontDatabase *fontDatabase() const override;
    QPlatformServices *services() const override;
    QVariant styleHint(StyleHint hint) const override;
    QPlatformTheme *createPlatformTheme(const QString &name) const override;
    QStringList themeNames() const override;
    QPlatformDrag *drag() const override;

    QPlatformNativeInterface *nativeInterface() const override;

#ifndef QT_NO_CLIPBOARD
    QPlatformClipboard *clipboard() const override;
#endif

    QLaylaOSSocketMonitor *getSocketMonitor() { return m_socketmonitor; }
    QLaylaOSDrag *getDragEngine() const { return m_dragEngine; }
    QLaylaOSScreen *getScreen() { return m_screen; }
    QPlatformInputContext *inputContext() const override { return m_inputContext; }

public Q_SLOTS:
    void dispatchMouseFromMainThread(QWindow *window,
                                     const QPointF &local, const QPointF &global,
                                     Qt::MouseButtons state, Qt::MouseButton button,
                                     QEvent::Type type, Qt::KeyboardModifiers mods);
    void dispatchWheelFromMainThread(QWindow *window, ulong timestamp, 
                                     const QPointF &local, const QPointF &global, 
                                     QPoint pixelDelta, QPoint angleDelta, Qt::KeyboardModifiers mods);
    void dispatchKeyFromMainThread(QWindow *window, QEvent::Type t, int k, 
                                     Qt::KeyboardModifiers mods, const QString & text);
    void dispatchDragFromMainThread(int evtype, winid_t winid, QMimeData *dropData,
                                    const QPoint &p,
                                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    void dispatchDropFromMainThread(winid_t winid, QMimeData *dropData,
                                    const QPoint &p,
                                    Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    void dispatchDragResponseFromMainThread(int response);

private:
#ifndef QT_NO_CLIPBOARD
    QLaylaOSClipboard *m_clipboard;
#endif
    QLaylaOSScreen *m_screen;
    QLaylaOSSocketMonitor *m_socketmonitor;
    QLaylaOSDrag *m_dragEngine;
    QThread *m_inputThread;
    QScopedPointer<QPlatformFontDatabase> m_fontDb;
    QScopedPointer<QPlatformServices> m_services;
    QScopedPointer<QPlatformNativeInterface> m_nativeInterface;
    QPlatformInputContext *m_inputContext;

#if QT_CONFIG(opengl)
    EGLDisplay m_eglDisplay;
    void createEglDisplay();
    void destroyEglDisplay();
#endif

    static QLaylaOSIntegration *ms_instance;
};

QT_END_NAMESPACE

#endif
