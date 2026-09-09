// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QLAYLAOSCLIPBOARD_H
#define QLAYLAOSCLIPBOARD_H

#if !defined(QT_NO_CLIPBOARD)

#include <qpa/qplatformclipboard.h>

QT_BEGIN_NAMESPACE

class QLaylaOSClipboard : public QPlatformClipboard
{
public:
    QLaylaOSClipboard();
    ~QLaylaOSClipboard();

    QMimeData *mimeData(QClipboard::Mode mode = QClipboard::Clipboard) override;
    void setMimeData(QMimeData *data, QClipboard::Mode mode = QClipboard::Clipboard) override;
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;

private:
    QMimeData *m_systemMimeData;
    QMimeData *m_userMimeData;
};

QT_END_NAMESPACE

#endif

#endif
