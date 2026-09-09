#include <QApplication>
#include <QScreen>
#include <QTime>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QFont>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>

class ClockApp : public QWidget
{
public:
    ClockApp(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void initUi();
    void updateTimeText();

    bool m_isAnalog = true;
    QLabel *m_digitalLabel = nullptr;
    QVBoxLayout *m_layout = nullptr;

    bool m_dragging = false;
    QPoint m_dragPosition;
};

ClockApp::ClockApp(QWidget *parent) : QWidget(parent)
{
    initUi();
}

void ClockApp::initUi()
{
    setWindowFlags(Qt::WindowType::FramelessWindowHint | 
                   Qt::WindowType::WindowStaysOnTopHint |
                   Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WidgetAttribute::WA_TranslucentBackground);
    resize(200, 200);

    // Position at top-right corner of primary screen
    QScreen *screen = QApplication::primaryScreen();
    if(screen)
    {
        QRect screenGeometry = screen->availableGeometry();
        int margin = 10;
        int x = screenGeometry.width() - width() - margin;
        int y = screenGeometry.top() + margin;
        move(x, y);
    }

    // Digital layout setup (hidden by default)
    m_layout = new QVBoxLayout(this);
    m_digitalLabel = new QLabel(this);
    m_digitalLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);
    m_digitalLabel->setFont(QFont("Helvetica", 36, QFont::Weight::Bold));
    m_digitalLabel->setStyleSheet("color: #00FFCC; background-color: rgba(30, 30, 30, 200); border-radius: 15px;");
    
    m_layout->addWidget(m_digitalLabel);
    m_digitalLabel->hide();

    // Global refresh timer (updates every second)
    startTimer(1000);

    updateTimeText();
}

void ClockApp::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
    {
        // Start dragging using global screen position
        m_dragging = true;
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void ClockApp::mouseMoveEvent(QMouseEvent *event)
{
    if(m_dragging && (event->buttons() & Qt::MouseButton::LeftButton))
    {
        // Move the window frame based on mouse movement delta
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void ClockApp::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::MouseButton::LeftButton && m_dragging)
    {
        m_dragging = false;
        event->accept();

        // If the mouse didn't actually move much, treat it as a click to toggle clock modes
        static const int clickThreshold = 3; 
        QPoint delta = event->globalPosition().toPoint() - (frameGeometry().topLeft() + m_dragPosition);

        if(delta.manhattanLength() < clickThreshold)
        {
            m_isAnalog = !m_isAnalog;
            if(m_isAnalog)
            {
                m_digitalLabel->hide();
            }
            else
            {
                m_digitalLabel->show();
                updateTimeText();
            }
            update();
        }
    }
}

void ClockApp::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    
    menu.setStyleSheet("QMenu { background-color: #222; color: #FFF; border: 1px solid #555; padding: 5px; }"
                       "QMenu::item:selected { background-color: #00FFCC; color: #000; }");

    QAction *closeAction = menu.addAction("Close Clock");
    
    if(menu.exec(event->globalPos()) == closeAction)
    {
        close();
    }
}

void ClockApp::timerEvent(QTimerEvent *event)
{
    Q_UNUSED(event);
    updateTimeText();
    update();
}

void ClockApp::updateTimeText()
{
    if(!m_isAnalog && m_digitalLabel)
    {
        m_digitalLabel->setText(QTime::currentTime().toString("hh:mm:ss"));
    }
}

void ClockApp::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if(!m_isAnalog)
    {
        return; // Let the layout/QLabel handle digital drawing
    }

    // Define custom hand geometries
    QPolygon hourHand;
    hourHand << QPoint(6, 7) << QPoint(-6, 7) << QPoint(0, -50);

    QPolygon minuteHand;
    minuteHand << QPoint(4, 7) << QPoint(-4, 7) << QPoint(0, -80);

    QPolygon secondHand;
    secondHand << QPoint(2, 10) << QPoint(-2, 10) << QPoint(0, -90);

    // Define color scheme
    QColor bgColor(30, 30, 45, 220);
    QColor hourColor(255, 99, 71);    // Red
    QColor minuteColor(30, 144, 255); // Blue
    QColor secondColor(50, 205, 50);  // Green
    QColor tickColor(255, 255, 255, 180); // White

    painter.setRenderHint(QPainter::RenderHint::Antialiasing);

    // Center coordinates and scale canvas
    painter.translate(width() / 2.0, height() / 2.0);
    int side = qMin(width(), height());
    painter.scale(side / 200.0, side / 200.0);

    // Draw translucent background plate
    painter.setPen(Qt::PenStyle::NoPen);
    painter.setBrush(bgColor);
    painter.drawEllipse(-95, -95, 190, 190);

    // Draw 12 hour ticks around the rim
    painter.setBrush(tickColor);
    for(int i = 0; i < 12; ++i)
    {
        // Draw a small 4x10 pixel rectangle near the top rim edge (-90y)
        painter.drawRect(-2, -90, 4, 10);
        painter.rotate(30.0); // Rotate canvas 30 degrees for the next tick
    }

    // Fetch active system time
    QTime time = QTime::currentTime();

    // Draw Hour Hand
    painter.setBrush(hourColor);
    painter.save();
    painter.rotate(30.0 * ((time.hour() % 12) + time.minute() / 60.0));
    painter.drawConvexPolygon(hourHand);
    painter.restore();

    // Draw Minute Hand
    painter.setBrush(minuteColor);
    painter.save();
    painter.rotate(6.0 * (time.minute() + time.second() / 60.0));
    painter.drawConvexPolygon(minuteHand);
    painter.restore();

    // Draw Second Hand
    painter.setBrush(secondColor);
    painter.save();
    painter.rotate(6.0 * time.second());
    painter.drawConvexPolygon(secondHand);
    painter.restore();

    // Draw Center Pivot Pin
    painter.setBrush(QColor(255, 255, 255));
    painter.drawEllipse(-4, -4, 8, 8);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    ClockApp clock;
    clock.show();
    
    return app.exec();
}

