// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2018 QNX Software Systems. All rights reserved.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSFOREIGNWINDOW_H
#define QLAYLAOSFOREIGNWINDOW_H

#include "qlaylaoswindow.h"

QT_BEGIN_NAMESPACE

class QLaylaOSForeignWindow : public QLaylaOSWindow
{
public:
    QLaylaOSForeignWindow(QWindow *window, QLaylaOSSocketMonitor *socketmonitor, winid_t screenWindow);
    ~QLaylaOSForeignWindow();

    bool isForeignWindow() const override;
};

QT_END_NAMESPACE

#endif // QLAYLAOSFOREIGNWINDOW_H
