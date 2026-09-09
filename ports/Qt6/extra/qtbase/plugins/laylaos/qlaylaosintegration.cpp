// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qlaylaosintegration.h"
#include "qlaylaosclipboard.h"
#include "qlaylaosrasterbackingstore.h"
#include "qlaylaoswindow.h"
#include "qlaylaosforeignwindow.h"
#include "qlaylaosscreen.h"
#include "qlaylaossocketmonitor.h"
#include "qlaylaosplatformtheme.h"
#include "qlaylaosdrag.h"

#if !defined(QT_NO_OPENGL)
#include "qlaylaoseglwindow.h"
#include "qlaylaosglcontext.h"
#include <QtGui/QOpenGLContext>
#endif

#include <QtGui/private/qgenericunixfontdatabase_p.h>
#include <QtGui/private/qdesktopunixservices_p.h>
#include <QtGui/private/qgenericunixeventdispatcher_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qrhibackingstore_p.h>
#include <qpa/qplatforminputcontextfactory_p.h>
#include <private/qinputdevicemanager_p_p.h>
#include <qpa/qwindowsysteminterface.h>

#include <fcntl.h>
#include <gui/gui.h>
#include <gui/client/dragndrop.h>

QT_BEGIN_NAMESPACE

QLaylaOSIntegration *QLaylaOSIntegration::ms_instance;

QLaylaOSIntegration::QLaylaOSIntegration(const QStringList &parameters)
    : QPlatformIntegration()
#if !defined(QT_NO_CLIPBOARD)
    , m_clipboard(new QLaylaOSClipboard)
#endif
    , m_dragEngine(new QLaylaOSDrag)
    , m_fontDb(new QGenericUnixFontDatabase)
    , m_services(new QDesktopUnixServices)
#if QT_CONFIG(opengl)
    , m_eglDisplay(EGL_NO_DISPLAY)
#endif
{
    Q_UNUSED(parameters);

    ms_instance = this;

    char dummy_name[] = "Qt6App";
    char *dummy_argv[] = { dummy_name, NULL };

    gui_init_no_fonts(1, dummy_argv);

    // libgui sets the close-on-exec flag on the server file descriptor,
    // but we need it open in order for dialogs that spawn their own 
    // event loop to work correctly
    int oldflags = fcntl(__global_gui_data.serverfd, F_GETFD, 0);
    oldflags &= ~FD_CLOEXEC;
    fcntl(__global_gui_data.serverfd, F_SETFD, oldflags);

    m_inputContext = QPlatformInputContextFactory::create();
    m_nativeInterface.reset(new QPlatformNativeInterface);

    m_screen = new QLaylaOSScreen;
    m_socketmonitor = new QLaylaOSSocketMonitor;

    m_inputThread = new QThread(nullptr);
    m_socketmonitor->moveToThread(m_inputThread);

    QObject::connect(m_socketmonitor, &QLaylaOSSocketMonitor::mouseEventReceived,
                     this, &QLaylaOSIntegration::dispatchMouseFromMainThread,
                     Qt::QueuedConnection);
    QObject::connect(m_socketmonitor, &QLaylaOSSocketMonitor::wheelEventReceived,
                     this, &QLaylaOSIntegration::dispatchWheelFromMainThread,
                     Qt::QueuedConnection);
    QObject::connect(m_socketmonitor, &QLaylaOSSocketMonitor::keyEventReceived,
                     this, &QLaylaOSIntegration::dispatchKeyFromMainThread,
                     Qt::QueuedConnection);
    QObject::connect(m_socketmonitor, &QLaylaOSSocketMonitor::dragEventReceived,
                     this, &QLaylaOSIntegration::dispatchDragFromMainThread,
                     Qt::QueuedConnection);
    QObject::connect(m_socketmonitor, &QLaylaOSSocketMonitor::dropEventReceived,
                     this, &QLaylaOSIntegration::dispatchDropFromMainThread,
                     Qt::QueuedConnection);
    QObject::connect(m_socketmonitor, &QLaylaOSSocketMonitor::dragResponseReceived,
                     this, &QLaylaOSIntegration::dispatchDragResponseFromMainThread,
                     Qt::QueuedConnection);

    QObject::connect(m_inputThread, &QThread::started, 
                     m_socketmonitor, &QLaylaOSSocketMonitor::startMonitoring);
    QObject::connect(m_inputThread, &QThread::finished, 
                     m_socketmonitor, &QLaylaOSSocketMonitor::cleanup);
    QObject::connect(m_inputThread, &QThread::finished, m_socketmonitor, &QObject::deleteLater);
    m_inputThread->start();

    // notify system about available screen
    QWindowSystemInterface::handleScreenAdded(m_screen);

#if QT_CONFIG(opengl)
    createEglDisplay();
#endif

    // ensure we use the fusion theme by default
    qputenv("QT_STYLE_OVERRIDE", "fusion");

    if (qEnvironmentVariableIsEmpty("FONTCONFIG_PATH") ||
        qEnvironmentVariableIsEmpty("FONTCONFIG_FILE")) {
        qputenv("FONTCONFIG_PATH", "/usr/Qt-6.11/lib/fonts");
        qputenv("FONTCONFIG_FILE", "/usr/Qt-6.11/lib/fonts/fonts.conf");
    }

    QFont defaultUIFont(QStringLiteral("Noto Sans"), 10);
    defaultUIFont.setWeight(QFont::Normal);
    defaultUIFont.setStyleHint(QFont::SansSerif);

    QGuiApplication::setFont(defaultUIFont);

    QFont::insertSubstitution(QStringLiteral("Arial"), QStringLiteral("Noto Sans"));
    QFont::insertSubstitution(QStringLiteral("sans-serif"), QStringLiteral("Noto Sans"));
    QFont::insertSubstitution(QStringLiteral("Courier New"), QStringLiteral("Noto Sans Mono"));
    QFont::insertSubstitution(QStringLiteral("Monospace"), QStringLiteral("Noto Sans Mono"));
}

QLaylaOSIntegration::~QLaylaOSIntegration()
{
    /*
    if (m_socketmonitor) {
        delete m_socketmonitor;
        m_socketmonitor = nullptr;
    }
    */

    if (m_inputThread) {
        m_inputThread->quit();

        if (!m_inputThread->wait(1000)) {
            m_inputThread->terminate();
        }

        delete m_inputThread;
        m_inputThread = nullptr;
    }

    QWindowSystemInterface::handleScreenRemoved(m_screen);
    m_screen = nullptr;

#if !defined(QT_NO_CLIPBOARD)
    if (m_clipboard) {
        delete m_clipboard;
        m_clipboard = nullptr;
    }
#endif

#if QT_CONFIG(opengl)
    destroyEglDisplay();
#endif

    ms_instance = nullptr;
}

QVariant QLaylaOSIntegration::styleHint(QPlatformIntegration::StyleHint hint) const
{
    if (hint == QPlatformIntegration::StyleHint::MouseDoubleClickInterval) {
        return QVariant(800);
    }

    return QPlatformIntegration::styleHint(hint);
}

QPlatformTheme *QLaylaOSIntegration::createPlatformTheme(const QString &name) const
{
    if (name == QStringLiteral("laylaos") || name == QStringLiteral("fusion")) {
        return new QLaylaOSPlatformTheme();
    }

    return QPlatformIntegration::createPlatformTheme(name);
}

QStringList QLaylaOSIntegration::themeNames() const
{
    return QStringList() << QStringLiteral("fusion");
}

void QLaylaOSIntegration::dispatchMouseFromMainThread(QWindow *window,
                                                const QPointF &local, const QPointF &global,
                                                Qt::MouseButtons state, Qt::MouseButton button,
                                                QEvent::Type type, Qt::KeyboardModifiers mods)
{
    const Qt::MouseEventSource source = Qt::MouseEventNotSynthesized;

    QWindowSystemInterface::handleMouseEvent(window, local, global, state, button, type, mods, source);
}

void QLaylaOSIntegration::dispatchWheelFromMainThread(QWindow *window, ulong timestamp, 
                                                      const QPointF &local, const QPointF &global, 
                                                      QPoint pixelDelta, QPoint angleDelta, 
                                                      Qt::KeyboardModifiers mods)
{
    QWindowSystemInterface::handleWheelEvent(window, timestamp, local, global, pixelDelta, angleDelta, mods);
}

void QLaylaOSIntegration::dispatchKeyFromMainThread(QWindow *window, QEvent::Type t, int k, 
                                                    Qt::KeyboardModifiers mods, const QString & text)
{
    //qDebug() << "QLaylaOSIntegration::dispatchKeyFromMainThread: t " << t << " k " << k;
    QWindowSystemInterface::handleKeyEvent(window, t, k, mods, text);
}

static void sendDragResponse(QPlatformDragQtResponse &response, QWindow *win)
{
    bool accepted = response.isAccepted();
    Qt::DropAction acceptedAction = response.acceptedAction();

    int responseToken = DRAG_RESPONSE_REJECT; // Default: 0 = Rejected

    if (accepted) {
        if (acceptedAction == Qt::CopyAction) responseToken = DRAG_RESPONSE_ACCEPT_COPY;
        else if (acceptedAction == Qt::MoveAction) responseToken = DRAG_RESPONSE_ACCEPT_MOVE;
    }

    QLaylaOSWindow *targetWindow = static_cast<QLaylaOSWindow*>(win->handle());
    struct window_t *lwin = targetWindow->nativeHandle();

    if (lwin) drag_response(lwin->winid, responseToken);
}

void QLaylaOSIntegration::dispatchDragFromMainThread(int evtype, winid_t winid, 
                                                     QMimeData *dropData,
                                                     const QPoint &p,
                                                     Qt::MouseButtons buttons, 
                                                     Qt::KeyboardModifiers modifiers)
{
    QWindow *win = m_socketmonitor->getWindow(winid);

    if(win == nullptr) {
        if (dropData) {
            dropData->deleteLater();
        }
        return;
    }

    Qt::DropActions allowedActions = (evtype == EVENT_DRAG_LEAVE) ?
                                        Qt::IgnoreAction :
                                            (Qt::CopyAction | Qt::MoveAction);

    QPlatformDragQtResponse response = 
            QWindowSystemInterface::handleDrag(win, dropData, p, allowedActions, buttons, modifiers);

    if (evtype == EVENT_DRAG_ENTER || evtype == EVENT_DRAG_MOVE) {
        sendDragResponse(response, win);
    }

    if (dropData) {
        dropData->deleteLater();
    }
}

void QLaylaOSIntegration::dispatchDropFromMainThread(winid_t winid, 
                                                     QMimeData *dropData,
                                                     const QPoint &p,
                                                     Qt::MouseButtons buttons, 
                                                     Qt::KeyboardModifiers modifiers)
{
    QWindow *win = m_socketmonitor->getWindow(winid);

    //qDebug() << "QLaylaOSIntegration::dispatchDropFromMainThread: winid " << winid << " win " << win << " dropData " << dropData;

    if(win != nullptr) {
        QWindowSystemInterface::handleDrop(win, dropData, p, Qt::CopyAction, buttons, modifiers);
    }

    if (dropData) {
        dropData->deleteLater();
    }
}

void QLaylaOSIntegration::dispatchDragResponseFromMainThread(int response)
{
    if (m_dragEngine) m_dragEngine->handleDragResponse(response);
}

bool QLaylaOSIntegration::hasCapability(QPlatformIntegration::Capability capability) const
{
    switch (capability) {
    case MultipleWindows:
    case ForeignWindows:
    case ThreadedPixmaps:
    case WindowManagement:
    case OffscreenSurface:
        return true;
#if !defined(QT_NO_OPENGL)
    case OpenGL:
    case ThreadedOpenGL:
    case BufferQueueingOpenGL:
    case OpenGLOnRasterSurface:
        return true;
#endif
    default:
        return QPlatformIntegration::hasCapability(capability);
    }
}

QPlatformDrag *QLaylaOSIntegration::drag() const
{
    return m_dragEngine;
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

QPlatformNativeInterface *QLaylaOSIntegration::nativeInterface() const
{
    return m_nativeInterface.data();
}

#if !defined(QT_NO_CLIPBOARD)
QPlatformClipboard *QLaylaOSIntegration::clipboard() const
{
    return m_clipboard;
}
#endif

QPlatformWindow *QLaylaOSIntegration::createForeignWindow(QWindow *window, WId nativeHandle) const
{
    winid_t screenWindow = static_cast<winid_t>(nativeHandle);
    if (m_socketmonitor->hasWindow(screenWindow)) {
        qWarning() << "QWindow already created for foreign window"
                   << screenWindow;
        return nullptr;
    }

    return new QLaylaOSForeignWindow(window, m_socketmonitor, screenWindow);
}

QPlatformWindow *QLaylaOSIntegration::createPlatformWindow(QWindow *window) const
{
    //if (m_socketmonitor->isMonitoring() == false)
    //    m_socketmonitor->startMonitoring();

    QSurface::SurfaceType surfaceType = window->surfaceType();
    switch (surfaceType) {
    case QSurface::RasterSurface:
    {
        QLaylaOSWindow *win = new QLaylaOSWindow(window, m_socketmonitor);
        win->init();
        return win;
    }
#if !defined(QT_NO_OPENGL)
    case QSurface::OpenGLSurface:
        return new QLaylaOSEglWindow(window, m_socketmonitor);
#endif
    default:
        qFatal("QQnxWindow: unsupported window API");
    }
    return 0;
}

QPlatformBackingStore *QLaylaOSIntegration::createPlatformBackingStore(QWindow *window) const
{
    QSurface::SurfaceType surfaceType = window->surfaceType();
    switch (surfaceType) {
    case QSurface::RasterSurface:
        return new QLaylaOSRasterBackingStore(window);
#if !defined(QT_NO_OPENGL)
    // Return a QRhiBackingStore for non-raster surface windows
    case QSurface::OpenGLSurface:
        return new QRhiBackingStore(window);
#endif
    default:
        return nullptr;
    }
}

#if !defined(QT_NO_OPENGL)
QPlatformOpenGLContext *QLaylaOSIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
    // Get color channel sizes from window format
    QSurfaceFormat format = context->format();
    int alphaSize = format.alphaBufferSize();
    int redSize = format.redBufferSize();
    int greenSize = format.greenBufferSize();
    int blueSize = format.blueBufferSize();

    // Check if all channels are don't care
    if (alphaSize == -1 && redSize == -1 && greenSize == -1 && blueSize == -1) {
        // Set color channels based on depth of window's screen
        QLaylaOSScreen *screen = static_cast<QLaylaOSScreen*>(context->screen()->handle());
        int depth = screen->depth();
        if (depth == 32) {
            // SCREEN_FORMAT_RGBA8888
            alphaSize = 8;
            redSize = 8;
            greenSize = 8;
            blueSize = 8;
        } else {
            // SCREEN_FORMAT_RGB565
            alphaSize = 0;
            redSize = 5;
            greenSize = 6;
            blueSize = 5;
        }
    } else {
        // Choose best match based on supported pixel formats
        if (alphaSize <= 0 && redSize <= 5 && greenSize <= 6 && blueSize <= 5) {
            // SCREEN_FORMAT_RGB565
            alphaSize = 0;
            redSize = 5;
            greenSize = 6;
            blueSize = 5;
        } else {
            // SCREEN_FORMAT_RGBA8888
            alphaSize = 8;
            redSize = 8;
            greenSize = 8;
            blueSize = 8;
        }
    }

    // Update color channel sizes in window format
    format.setAlphaBufferSize(alphaSize);
    format.setRedBufferSize(redSize);
    format.setGreenBufferSize(greenSize);
    format.setBlueBufferSize(blueSize);
    context->setFormat(format);

    QLaylaOSGLContext *ctx = new QLaylaOSGLContext(context->format(), context->shareHandle());
    return ctx;
}
#endif

#if QT_CONFIG(opengl)
void QLaylaOSIntegration::createEglDisplay()
{
    // Initialize connection to EGL
    m_eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (Q_UNLIKELY(m_eglDisplay == EGL_NO_DISPLAY))
        qFatal("QLaylaOSIntegration: failed to obtain EGL display: %x", eglGetError());

    EGLBoolean eglResult = eglInitialize(m_eglDisplay, 0, 0);
    if (Q_UNLIKELY(eglResult != EGL_TRUE))
        qFatal("QLaylaOSIntegration: failed to initialize EGL display, err=%d", eglGetError());
}

void QLaylaOSIntegration::destroyEglDisplay()
{
    // Close connection to EGL
    eglTerminate(m_eglDisplay);
}
#endif

QAbstractEventDispatcher *QLaylaOSIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QT_END_NAMESPACE
