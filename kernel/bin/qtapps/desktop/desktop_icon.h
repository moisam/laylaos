#ifndef DESKTOP_ICON_H
#define DESKTOP_ICON_H

#include <QWidget>
#include <QPixmap>
#include <QString>
#include <QPoint>
#include <functional>

class DesktopIcon : public QWidget
{
public:
    DesktopIcon(const QString& title, const QString& cmd, const QString& iconPath, QWidget* parent = nullptr);

    void launch();
    void setSelected(bool selected);
    bool isSelected() const { return m_isSelected; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    QPixmap m_iconPixmap;
    QString m_title;
    QString m_cmd;
    bool m_isSelected = false;
    bool m_isDragging = false;
    QPoint m_dragOffset;
    QColor selectedBackgroundAlpha = QColor(0xFF, 0xFB, 0xCC, 0x88);
    QColor selectedBackgroundSolid = QColor(0xFF, 0xFB, 0xCC, 0xFF);
    QColor normalBackground = QColor(0x16, 0xA0, 0x85, 0xFF);
};

#endif      /* DESKTOP_ICON_H */
