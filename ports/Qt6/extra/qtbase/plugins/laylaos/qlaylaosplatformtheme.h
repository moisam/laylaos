// Copyright (C) 2026 Mohammed Isam
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#ifndef QLAYLAOSPLATFORMTHEME_H
#define QLAYLAOSPLATFORMTHEME_H

#include <qpa/qplatformtheme.h>
#include <QtGui/qpalette.h>
#include <QtGui/QFont>

QT_BEGIN_NAMESPACE

class QLaylaOSPlatformTheme : public QPlatformTheme
{
public:
    QLaylaOSPlatformTheme();
    ~QLaylaOSPlatformTheme();

    void handleThemeChange();

    static QLaylaOSPlatformTheme *instance() { return m_instance; }

    const QPalette *palette(Palette type = SystemPalette) const override;
    QVariant themeHint(ThemeHint hint) const override;
    const QFont *font(Font type = SystemFont) const override;

#if QT_CONFIG(systemtrayicon)
    QPlatformSystemTrayIcon *createPlatformSystemTrayIcon() const override;
#endif

private:
    mutable QPalette m_customPalette;
    mutable QPalette m_customDarkPalette;
    mutable bool m_paletteInitialized;
    static QLaylaOSPlatformTheme *m_instance;

    mutable QFont m_defaultFont;
    mutable bool m_fontInitialized;
};

QT_END_NAMESPACE

#endif // QLAYLAOSPLATFORMTHEME_H
