#include "selectionoverlay.h"

SelectionOverlay::SelectionOverlay(std::function<void(const QPixmap&)> callback, QWidget *parent)
    : QWidget(parent), m_onCaptureCallback(callback) 
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowState(Qt::WindowFullScreen);
    setCursor(Qt::CrossCursor);
}

void SelectionOverlay::captureBackground()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if(screen)
    {
        m_fullScreenPixmap = screen->grabWindow(0);
    }
}

void SelectionOverlay::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.drawPixmap(0, 0, m_fullScreenPixmap);
    //painter.fillRect(rect(), QColor(0, 0, 0, 80));
}

void SelectionOverlay::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_startPoint = event->pos();

        if(!m_rubberBand)
        {
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, this);
        }

        m_rubberBand->setGeometry(QRect(m_startPoint, QSize()));
        m_rubberBand->show();
    }
}

void SelectionOverlay::mouseMoveEvent(QMouseEvent *event)
{
    if(m_rubberBand)
    {
        m_rubberBand->setGeometry(QRect(m_startPoint, event->pos()).normalized());
    }
}

void SelectionOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && m_rubberBand)
    {
        QRect selectedRect = m_rubberBand->geometry();
        m_rubberBand->hide();
        this->close();

        if(selectedRect.width() > 5 && selectedRect.height() > 5)
        {
            QPixmap cropped = m_fullScreenPixmap.copy(selectedRect);

            if(m_onCaptureCallback)
            {
                m_onCaptureCallback(cropped);
            }
        }
    }
}

void SelectionOverlay::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
    {
        if(m_rubberBand) m_rubberBand->hide();
        this->close();
    }
}

