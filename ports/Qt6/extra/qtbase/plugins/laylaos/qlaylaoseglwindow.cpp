// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2013 - 2014 BlackBerry Limited. All rights reserved.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default


#include "qlaylaoseglwindow.h"
#include "qlaylaosscreen.h"
#include "qlaylaosglcontext.h"
#include "qlaylaossocketmonitor.h"

#include <QDebug>

#include <errno.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQpaWindowEgl, "qt.qpa.window.egl");

QLaylaOSEglWindow::QLaylaOSEglWindow(QWindow *window, QLaylaOSSocketMonitor *socketmonitor) :
    QLaylaOSWindow(window, socketmonitor),
    m_eglDisplay(EGL_NO_DISPLAY),
    m_eglSurface(EGL_NO_SURFACE)
{
    init();
}

QLaylaOSEglWindow::~QLaylaOSEglWindow()
{
    // Cleanup EGL surface if it exists
    destroyEGLSurface();
}

bool QLaylaOSEglWindow::isInitialized() const
{
    return m_eglSurface != EGL_NO_SURFACE;
}

void QLaylaOSEglWindow::ensureInitialized(QLaylaOSGLContext* context)
{
    if (m_eglSurface == EGL_NO_SURFACE) {
        createEGLSurface(context);
    }
}

void QLaylaOSEglWindow::createEGLSurface(QLaylaOSGLContext *context)
{
    m_eglDisplay = context->eglDisplay();
    m_eglConfig = context->eglConfig();
    m_format = context->format();

    const EGLint eglSurfaceAttrs[] =
    {
        EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
        EGL_NONE
    };

    // Create EGL surface
    EGLSurface eglSurface = eglCreateWindowSurface(
        m_eglDisplay,
        m_eglConfig,
        (EGLNativeWindowType) nativeHandle(),
        eglSurfaceAttrs);

    if (eglSurface == EGL_NO_SURFACE)
        qWarning("QLaylaOS: failed to create EGL surface, err=%d", eglGetError());

    m_eglSurface = eglSurface;
}

void QLaylaOSEglWindow::destroyEGLSurface()
{
    // Destroy EGL surface if it exists
    if (m_eglSurface != EGL_NO_SURFACE) {
        EGLBoolean eglResult = eglDestroySurface(m_eglDisplay, m_eglSurface);
        if (Q_UNLIKELY(eglResult != EGL_TRUE))
            qFatal("QQNX: failed to destroy EGL surface, err=%d", eglGetError());
    }

    m_eglSurface = EGL_NO_SURFACE;
}

EGLSurface QLaylaOSEglWindow::surface() const
{
    return m_eglSurface;
}

void QLaylaOSEglWindow::setGeometry(const QRect &rect)
{
    QLaylaOSWindow::setGeometry(rect);
}

QT_END_NAMESPACE
