/***************************************************************************
**
** Copyright (C) 2024 Mohammed Isam <mohammed_isam1984@yahoo.com>
** Copyright (C) 2015 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Tobias Koenig <tobias.koenig@kdab.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#if !defined(QT_NO_CLIPBOARD)

#include "qlaylaosclipboard.h"

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
            clipboard_set_data(CLIPBOARD_FORMAT_TEXT, (void *)data.data(), data.count());
        }
    } else {
        clipboard_set_data(CLIPBOARD_FORMAT_TEXT, (void *)"", 1);
    }

    m_userMimeData = mimeData;

    emitChanged(QClipboard::Clipboard);
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
