// Copyright (C) 2026 Mohammed Isam
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

#include "qlaylaosplatformtheme.h"

#include <QtCore/qvariant.h>
#include <QtGui/QFont>
#include <qpa/qwindowsysteminterface.h>
#if QT_CONFIG(systemtrayicon)
#  include "qlaylaossystemtrayicon.h"
#endif

#include <gui/gui.h>
#include <gui/theme.h>

#define GLOB            __global_gui_data

QT_BEGIN_NAMESPACE

QLaylaOSPlatformTheme *QLaylaOSPlatformTheme::m_instance = nullptr;

QLaylaOSPlatformTheme::QLaylaOSPlatformTheme()
    : m_paletteInitialized(false)
{
    m_instance = this;
}

QLaylaOSPlatformTheme::~QLaylaOSPlatformTheme()
{
    m_instance = nullptr;
}

#if QT_CONFIG(systemtrayicon)
QPlatformSystemTrayIcon *QLaylaOSPlatformTheme::createPlatformSystemTrayIcon() const
{
    return new QLaylaOSSystemTrayIcon;
}
#endif

#define R(c)        (((c) >> 24) & 0xFF)
#define G(c)        (((c) >> 16) & 0xFF)
#define B(c)        (((c) >> 8 ) & 0xFF)

static QColor nativeToQColor(int index)
{
    uint32_t native = GLOB.themecolor[index];

    return QColor(R(native), G(native), B(native));
}

#undef R
#undef G
#undef B

static void updateTheme(QPalette &palette)
{
    palette.setColor(QPalette::Window, nativeToQColor(THEME_COLOR_WINDOW_BGCOLOR));
    palette.setColor(QPalette::WindowText, nativeToQColor(THEME_COLOR_BUTTON_TEXTCOLOR));
    palette.setColor(QPalette::Base, nativeToQColor(THEME_COLOR_INPUTBOX_BGCOLOR));
    palette.setColor(QPalette::Text, nativeToQColor(THEME_COLOR_BUTTON_TEXTCOLOR));
    palette.setColor(QPalette::Button, nativeToQColor(THEME_COLOR_BUTTON_BGCOLOR));
    palette.setColor(QPalette::ButtonText, nativeToQColor(THEME_COLOR_BUTTON_TEXTCOLOR));
    palette.setColor(QPalette::Highlight, nativeToQColor(THEME_COLOR_INPUTBOX_SELECT_BGCOLOR));
    palette.setColor(QPalette::HighlightedText, nativeToQColor(THEME_COLOR_INPUTBOX_SELECT_TEXTCOLOR));
}

static void updateDarkTheme(QPalette &darkPalette)
{
    QColor darkBg(0x1F, 0x1F, 0x1F);       
    QColor panelBg(0x16, 0x16, 0x16);      
    QColor textWhite(0xEC, 0xF0, 0xF1);   
    QColor textMuted(0x7F, 0x8C, 0x8D);   
    QColor accentTeal(0x16, 0xA0, 0x85);  

    darkPalette.setColor(QPalette::Window, darkBg);
    darkPalette.setColor(QPalette::WindowText, textWhite);
    darkPalette.setColor(QPalette::Base, panelBg);
    darkPalette.setColor(QPalette::AlternateBase, darkBg);
    darkPalette.setColor(QPalette::Text, textWhite);
    darkPalette.setColor(QPalette::Button, panelBg);
    darkPalette.setColor(QPalette::ButtonText, textWhite);
    darkPalette.setColor(QPalette::Highlight, accentTeal);
    darkPalette.setColor(QPalette::HighlightedText, Qt::white);
    darkPalette.setColor(QPalette::PlaceholderText, textMuted);
    darkPalette.setColor(QPalette::ToolTipBase, darkBg);
    darkPalette.setColor(QPalette::ToolTipText, textWhite);
}

const QPalette *QLaylaOSPlatformTheme::palette(Palette type) const
{
    if (type == SystemPalette) {
        char* envTheme = std::getenv("QT_LAYLAOS_THEME");
        bool isDark = (envTheme != nullptr && std::strcmp(envTheme, "dark") == 0);

        if (!m_paletteInitialized) {
            const QPalette *basePalette = QPlatformTheme::palette(SystemPalette);
            const QPalette *baseDarkPalette = QPlatformTheme::palette(SystemPalette);

            if (basePalette) m_customPalette = *basePalette;
            if (baseDarkPalette) m_customDarkPalette = *baseDarkPalette;

            m_paletteInitialized = true;
        }

        updateTheme(m_customPalette);
        updateDarkTheme(m_customDarkPalette);

        return isDark ? &m_customDarkPalette : &m_customPalette;
    }

    return QPlatformTheme::palette(type);
}

QVariant QLaylaOSPlatformTheme::themeHint(ThemeHint hint) const
{
    switch (hint) {
        case UiEffects:
            return 0;    // Disable all fade/blend animations to save CPU cycles

        case MouseDoubleClickInterval:
            return 800;

        default:
            return QPlatformTheme::themeHint(hint);
    }
}

void QLaylaOSPlatformTheme::handleThemeChange()
{
    updateTheme(m_customPalette);
    QWindowSystemInterface::handleThemeChange<QWindowSystemInterface::SynchronousDelivery>();
}

const QFont *QLaylaOSPlatformTheme::font(Font type) const
{
    if (type == SystemFont || type == FixedFont || type == MenuFont) {
        if (!m_fontInitialized) {
            m_defaultFont = QFont(QStringLiteral("Noto Sans"), 10);
            m_defaultFont.setWeight(QFont::Normal);
            m_defaultFont.setStyleHint(QFont::SansSerif); // Prioritizes Sans typography mapping
            
            m_fontInitialized = true;
        }

        return &m_defaultFont;
    }
    
    return QPlatformTheme::font(type);
}

QT_END_NAMESPACE
