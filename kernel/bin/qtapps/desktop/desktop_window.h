#ifndef DESKTOP_WINDOW_H
#define DESKTOP_WINDOW_H

#include <QWidget>
#include <QPixmap>
#include <QPushButton>
#include <vector>
#include "desktop_icon.h"
#include "../../desktop/desktop/desktop.h"


class DesktopWindow : public QWidget
{
public:
    explicit DesktopWindow(QWidget *parent = nullptr);

    void setActiveSelection(DesktopIcon *selectedIcon);
    void setWallpaper(const QString& path, int aspect = DESKTOP_BACKGROUND_STRETCHED);
    void setWallpaperAspect(int aspect);

    void loadEntries();
    void navigateSpatially(int dx, int dy);

protected:
    void customEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QPixmap m_wallpaper;
    QString m_wallpaperPath;
    int m_wallpaperAspect = DESKTOP_BACKGROUND_STRETCHED;
    bool m_backgroundIsImage = false;
    QColor m_backgroundColor = QColor(0x16, 0xA0, 0x85);
    QColor m_blackColor = QColor(0x00, 0x00, 0x00, 0xFF);

    std::vector<DesktopIcon *> m_icons;
    DesktopIcon *m_currentSelected = nullptr;
};

#endif      /* DESKTOP_WINDOW_H */
