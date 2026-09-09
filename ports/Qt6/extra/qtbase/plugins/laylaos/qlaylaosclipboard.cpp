// Copyright (C) 2024-2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#if !defined(QT_NO_CLIPBOARD)

#include "qlaylaosclipboard.h"
#include "qlaylaosintegration.h"
#include "qlaylaossocketmonitor.h"

#include <private/qguiapplication_p.h>
#include <QMimeData>
#include <QThread>

#include <gui/clipboard.h>

QLaylaOSClipboard::QLaylaOSClipboard()
    : m_systemMimeData(nullptr)
    , m_userMimeData(nullptr)
{
}

QLaylaOSClipboard::~QLaylaOSClipboard()
{
    delete m_userMimeData;
    delete m_systemMimeData;
}

QMimeData *QLaylaOSClipboard::mimeData(QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return 0;

    if (m_userMimeData)
        return m_userMimeData;

    if (!m_systemMimeData)
        m_systemMimeData = new QMimeData();
    else
        m_systemMimeData->clear();

    /*
     * NOTE: LaylaOS only supports text at the moment.
     * TODO: update this when we support more types.
     */

    int datasz;
    const void *data = nullptr;

    if((datasz = clipboard_has_data(CLIPBOARD_FORMAT_TEXT)) != 0) {
        if((data = clipboard_get_data(CLIPBOARD_FORMAT_TEXT)) != nullptr) {
            m_systemMimeData->setText(QString::fromLocal8Bit(reinterpret_cast<const char*>(data), datasz));
        }
    }

    // see the comment at the end of QLaylaOSWindow::QLaylaOSWindow() for
    // why this call is important here
    auto *laylaos_integration = static_cast<QLaylaOSIntegration *>(QGuiApplicationPrivate::platformIntegration());
    QLaylaOSSocketMonitor *monitor = laylaos_integration->getSocketMonitor();
    emit monitor->gonow();
    //m_socketmonitor->readyRead();

    return m_systemMimeData;
}

void QLaylaOSClipboard::setMimeData(QMimeData *mimeData, QClipboard::Mode mode)
{
    if (mode != QClipboard::Clipboard)
        return;

    if (mimeData) {
        if (m_systemMimeData == mimeData)
            return;

        if (m_userMimeData == mimeData)
            return;
    }

    if (mimeData) {
        /*
         * NOTE: LaylaOS only supports text at the moment.
         * TODO: update this when we support more types.
         */

        if(mimeData->hasText()) {
            const QByteArray data = mimeData->text().toLocal8Bit();
            clipboard_set_data(CLIPBOARD_FORMAT_TEXT, (void *)data.data(), data.size());
        }
    } else {
        clipboard_set_data(CLIPBOARD_FORMAT_TEXT, (void *)"", 1);
    }

    m_userMimeData = mimeData;

    emitChanged(QClipboard::Clipboard);

    // see the comment at the end of QLaylaOSWindow::QLaylaOSWindow() for
    // why this call is important here
    auto *laylaos_integration = static_cast<QLaylaOSIntegration *>(QGuiApplicationPrivate::platformIntegration());
    QLaylaOSSocketMonitor *monitor = laylaos_integration->getSocketMonitor();
    emit monitor->gonow();
    //m_socketmonitor->readyRead();
}

bool QLaylaOSClipboard::supportsMode(QClipboard::Mode mode) const
{
    return (mode == QClipboard::Clipboard);
}

bool QLaylaOSClipboard::ownsMode(QClipboard::Mode mode) const
{
    Q_UNUSED(mode);

    return false;
}

#endif
