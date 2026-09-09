// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2011 - 2012 Research In Motion
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSGLCONTEXT_H
#define QLAYLAOSGLCONTEXT_H

#include <qpa/qplatformopenglcontext.h>
#include <QtGui/QSurfaceFormat>
#include <QtCore/QAtomicInt>
#include <QtCore/QSize>

#include <EGL/egl.h>
#include <QtGui/private/qeglplatformcontext_p.h>

QT_BEGIN_NAMESPACE

class QLaylaOSWindow;

class QLaylaOSGLContext : public QEGLPlatformContext
{
public:
    QLaylaOSGLContext(const QSurfaceFormat &format, QPlatformOpenGLContext *share);
    virtual ~QLaylaOSGLContext();

    bool makeCurrent(QPlatformSurface *surface) override;
    void swapBuffers(QPlatformSurface *surface) override;
    void doneCurrent() override;

protected:
    EGLSurface eglSurfaceForPlatformSurface(QPlatformSurface *surface) override;
};

QT_END_NAMESPACE

#endif // QQNXGLCONTEXT_H
