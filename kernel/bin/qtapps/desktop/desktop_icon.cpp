#include "desktop_icon.h"
#include "desktop_window.h"
#include <QPainter>
#include <QMouseEvent>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <gui/gui.h>
#include "../../desktop/desktop/run_command.c"


DesktopIcon::DesktopIcon(const QString& title, const QString& cmd, const QString& iconPath, QWidget *parent)
    : QWidget(parent), m_title(title), m_cmd(cmd)
{
    m_iconPixmap.load(iconPath);
    
    // Fixed size
    setFixedSize(90, 105);
}

void DesktopIcon::setSelected(bool selected)
{
    m_isSelected = selected;
    update(); // Redraw with highlight change
}

void DesktopIcon::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::transparent);

    int iconSize = 48;
    int iconX = (width() - iconSize) / 2;
    int iconY = 4;
    QRect iconRect(iconX, iconY, iconSize, iconSize);

    // Draw the base app icon image first
    if(!m_iconPixmap.isNull())
    {
        painter.drawPixmap(iconRect, m_iconPixmap);
    }

    // Draw translucent selection tint directly on top of the icon
    if(m_isSelected)
    {
        // Save the current clean state of the painter before altering blending modes
        painter.save();

        // This locks drawing operations strictly to existing pixel alpha masks
        painter.setCompositionMode(QPainter::CompositionMode_SourceAtop);

        painter.setPen(Qt::NoPen);
        painter.setBrush(selectedBackgroundAlpha);
        painter.drawRect(iconRect);

        // Restore painter back to standard CompositionMode_SourceOver for text drawing
        painter.restore();
    }

    // Compute bounding rect for text
    QRect maxTextConstraint(4, 58, width() - 8, 36); 
    QFontMetrics metrics(painter.font());

    QRect calculatedTextRect = metrics.boundingRect(
        maxTextConstraint,
        Qt::AlignCenter | Qt::TextWordWrap,
        m_title
    );

    int finalBoxWidth = calculatedTextRect.width() + 12;
    int finalBoxHeight = calculatedTextRect.height() + 6;

    // Ensure it doesn't spill over the widget edges
    finalBoxWidth = qMin(finalBoxWidth, width() - 4);
    
    // Center the background box horizontally relative to the icon widget
    int boxX = (width() - finalBoxWidth) / 2;
    int boxY = 58 + ((maxTextConstraint.height() - finalBoxHeight) / 2); // Center single lines vertically

    QRect finalTextBox(boxX, boxY, finalBoxWidth, finalBoxHeight);

    // Draw text background box
    painter.setPen(Qt::NoPen);

    if(m_isSelected)
    {
        painter.setBrush(selectedBackgroundSolid);
    }
    else
    {
        painter.setBrush(normalBackground);
    }
    painter.drawRoundedRect(finalTextBox, 2, 2);

    // Draw text
    if(m_isSelected)
    {
        painter.setPen(Qt::black);
    }
    else
    {
        painter.setPen(Qt::white);
    }
    
    painter.drawText(finalTextBox, Qt::AlignCenter | Qt::TextWordWrap, m_title);
}

void DesktopIcon::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_isDragging = true;
        m_dragOffset = event->pos();
        raise();

        // Notify parent to re-route active focus selection to us
        if(auto *desktop = static_cast<DesktopWindow *>(parentWidget()))
        {
            desktop->setActiveSelection(this);
        }
    }
}

void DesktopIcon::mouseMoveEvent(QMouseEvent *event)
{
    if(m_isDragging && (event->buttons() & Qt::LeftButton))
    {
        // Calculate the absolute destination relative to DesktopWindow
        QPoint globalPos = mapToParent(event->pos());
        QPoint newPos = globalPos - m_dragOffset;

        // Boundary check: Ensure icons do not drag entirely off screen edges
        if(parentWidget())
        {
            newPos.setX(qBound(0, newPos.x(), parentWidget()->width() - width()));
            newPos.setY(qBound(0, newPos.y(), parentWidget()->height() - height()));
        }

        move(newPos);
    }
}

void DesktopIcon::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_isDragging = false;
    }
}

void DesktopIcon::mouseDoubleClickEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        launch();
    }
}

void DesktopIcon::launch()
{
    QByteArray byteBuffer = m_cmd.toUtf8();
    const char *c_str = byteBuffer.constData();

    // unset this for the app we are about to run
    unsetenv("QT_LAYLAOS_THEME");

    run_command((char *)c_str);

    // we need to restore this in case system theme changed later on
    setenv("QT_LAYLAOS_THEME", "dark", 1);
}

