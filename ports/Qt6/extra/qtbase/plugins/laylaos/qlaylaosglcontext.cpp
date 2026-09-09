// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2011 - 2013 BlackBerry Limited. All rights reserved.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qlaylaosglcontext.h"
#include "qlaylaosintegration.h"
#include "qlaylaosscreen.h"
#include "qlaylaoseglwindow.h"
#include "qlaylaoswindow.h"

#include "private/qeglconvenience_p.h"

#include <QtCore/QDebug>
#include <QtGui/QOpenGLContext>
#include <QtGui/QScreen>

#include <dlfcn.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(lcQpaGLContext, "qt.qpa.glcontext");

static QEGLPlatformContext::Flags makeFlags()
{
    QEGLPlatformContext::Flags result = {};

    result |= QEGLPlatformContext::NoSurfaceless;

    return result;
}

QLaylaOSGLContext::QLaylaOSGLContext(const QSurfaceFormat &format, QPlatformOpenGLContext *share)
    : QEGLPlatformContext(format, share, QLaylaOSIntegration::instance()->eglDisplay(), nullptr,
                          makeFlags())
{
}

QLaylaOSGLContext::~QLaylaOSGLContext()
{
}

EGLSurface QLaylaOSGLContext::eglSurfaceForPlatformSurface(QPlatformSurface *surface)
{
    QLaylaOSEglWindow *window = static_cast<QLaylaOSEglWindow *>(surface);
    window->ensureInitialized(this);
    return window->surface();
}

bool QLaylaOSGLContext::makeCurrent(QPlatformSurface *surface)
{
    qCDebug(lcQpaGLContext) << Q_FUNC_INFO;
    return QEGLPlatformContext::makeCurrent(surface);
}

void QLaylaOSGLContext::swapBuffers(QPlatformSurface *surface)
{
    qCDebug(lcQpaGLContext) << Q_FUNC_INFO;
    QEGLPlatformContext::swapBuffers(surface);
}

void QLaylaOSGLContext::doneCurrent()
{
    QEGLPlatformContext::doneCurrent();
}

QT_END_NAMESPACE
