// Copyright (C) 2026 Mohammed Isam
// Copyright (C) 2013 BlackBerry Limited. All rights reserved.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSEGLWINDOW_H
#define QLAYLAOSEGLWINDOW_H

#include "qlaylaoswindow.h"
#include <QtCore/QMutex>
#include <QtCore/QLoggingCategory>

#if !defined(QT_NO_OPENGL)
#include <EGL/egl.h>
#endif

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(lcQpaWindowEgl);

class QLaylaOSGLContext;
class QLaylaOSSocketMonitor;

class QLaylaOSEglWindow : public QLaylaOSWindow
{
public:
    QLaylaOSEglWindow(QWindow *window, QLaylaOSSocketMonitor *socketmonitor);
    ~QLaylaOSEglWindow();

    EGLSurface surface() const;

    bool isInitialized() const;
    void ensureInitialized(QLaylaOSGLContext *context);

    void setGeometry(const QRect &rect) override;

    QSurfaceFormat format() const override { return m_format; }

protected:
    int pixelFormat();
    void resetBuffers();

private:
    void createEGLSurface(QLaylaOSGLContext *context);
    void destroyEGLSurface();

    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    EGLSurface m_eglSurface;
    QSurfaceFormat m_format;
};

QT_END_NAMESPACE

#endif // QQNXEGLWINDOW_H
