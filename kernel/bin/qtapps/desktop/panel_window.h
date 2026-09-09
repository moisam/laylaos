#ifndef PANEL_WINDOW_H
#define PANEL_WINDOW_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include "panel_alt_tab.h"

#include <kernel/mouse.h>
#include <gui/window-defs.h>
#include <gui/resources.h>

class PanelWindow : public QWidget
{
public:
    explicit PanelWindow(QWidget* parent = nullptr);
    void childEvent(winid_t child_winid, uint32_t evtype);
    void childEventTitleSet(winid_t child_winid, QString title);
    void childEventIconSet(winid_t child_winid, uint32_t restype, resid_t resid);
    void systrayEvent(winid_t winid, uint32_t evtype);
    void systrayBoundsEvent(winid_t winid, uint32_t seqid);
    void systrayIconEvent(winid_t winid, QIcon icon);
    void systrayTooltipEvent(winid_t winid, QString tooltip);
    void systrayMsgEvent(QString title, QString msg, int type, int duration, QPixmap pixmap);

    void dispatchTrayClickEvent(winid_t winid, mouse_buttons_t mbutton, QObject *widget);

    void customEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    //void resizeEvent(QResizeEvent *event) override;

private:
    void createApplicationMenu();
    void createSystemTray();
    QIcon getCachedIcon(const QString& iconPath, const QString& iconName);
    QIcon getCachedIconForWinId(winid_t winid);
    void handleAltTabKeyPressed();
    void handleAltKeyReleased();
    void recalcButtonWidths();

    AltTabSwitcher* m_switcher = nullptr;
    std::vector<AltTabWindowInfo> m_altTabWindows;
    int m_altTabCurrentIndex = -1;
    bool m_isAltTabActive = false;

    int m_curButtonWidth = 120;
    QIcon m_defaultAppIcon;
    QLabel *m_clockLabel;
    QHBoxLayout *m_taskbarLayout = nullptr;    
    QHBoxLayout *m_mainLayout = nullptr;
    QHBoxLayout *m_trayLayout = nullptr;
    QWidget *m_taskbarWidget = nullptr;
    QWidget *m_trayWidget = nullptr;
    QPushButton *m_lastFocusedChild = nullptr;
    QPushButton *m_menuButton = nullptr;

    std::map<winid_t, QToolButton *> m_trayButtons;
    std::map<winid_t, QPushButton *> m_taskbarButtons;
    std::map<winid_t, QIcon> m_taskbarIconCache;
    std::map<QString, QIcon> m_appMenuIconCache;
};

#endif      /* PANEL_WINDOW_H */
