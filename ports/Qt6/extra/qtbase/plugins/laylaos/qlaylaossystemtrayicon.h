// Copyright (C) 2026 Mohammed Isam <mohammed_isam1984@yahoo.com>
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSSYSTEMTRAYICON_H
#define QLAYLAOSSYSTEMTRAYICON_H

#include <QtGui/qicon.h>
#include <QtGui/qpa/qplatformsystemtrayicon.h>

QT_BEGIN_NAMESPACE

class QLaylaOSSystemTrayIcon : public QPlatformSystemTrayIcon {
public:
    QLaylaOSSystemTrayIcon();
    ~QLaylaOSSystemTrayIcon() override;
    void init() override;
    void cleanup() override;
    void updateIcon(const QIcon &icon) override;
    void updateToolTip(const QString &tooltip) override;
    QRect geometry() const override;
    void showMessage(const QString &title, const QString &msg,
                     const QIcon &icon, MessageIcon iconType, int msecs) override;

    bool isSystemTrayAvailable() const override { return m_systrayAvailable; }
    bool supportsMessages() const override;

    QPlatformMenu *createMenu() const override;
    void updateMenu(QPlatformMenu *) override;

    static void dispatchClientEvent(struct event_t *ev);
    void clientEvent(struct event_t *ev);

private:
    bool isInstalled() const { return m_win != nullptr; }
    bool ensureInstalled();
    void ensureCleanup();
    QByteArray serializeIconAsPng(const QIcon& icon);

    struct window_t *m_win = nullptr;
    QIcon m_icon;
    QString m_toolTip;
    bool m_visible = false;
    bool m_systrayAvailable = true;
};

QT_END_NAMESPACE

#endif // QLAYLAOSSYSTEMTRAYICON_H
