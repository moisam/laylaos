#ifndef DEKSTOP_TOAST_H
#define DEKSTOP_TOAST_H

class DesktopToast : public QWidget
{
public:
    DesktopToast(const QString& title, const QString& msg, int type, int duration, 
                 const QPixmap& customIcon, QWidget *parent = nullptr) : QWidget(parent)
    {
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFixedSize(300, 85);

        setStyleSheet(
            "QWidget { background-color: #1F1F1F; border: 1px solid #16A085; border-radius: 6px; }"
            "QLabel#Title { color: #1ABC9C; font-weight: bold; font-size: 13px; border: none; background: transparent; }"
            "QLabel#Message { color: #ECF0F1; font-size: 11px; border: none; background: transparent; }"
        );

        auto* mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(12, 10, 12, 10);
        mainLayout->setSpacing(10);

        auto* iconLabel = new QLabel(this);
        iconLabel->setFixedSize(24, 24);
        
        if(!customIcon.isNull())
        {
            iconLabel->setPixmap(customIcon);
        }
        else
        {
            // Text emoji fallback mapping if app doesn't supply a unique pixmap
            QString fallbackGlyph = "-";
            iconLabel->setText(fallbackGlyph);
            iconLabel->setStyleSheet("font-size: 18px; border: none; background: transparent;");
        }
        mainLayout->addWidget(iconLabel, 0, Qt::AlignTop);

        auto *textContainer = new QWidget(this);
        textContainer->setStyleSheet("border: none; background: transparent;");

        auto *textLayout = new QVBoxLayout(textContainer);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);

        auto *titleLabel = new QLabel(title, this);
        titleLabel->setObjectName("Title");

        auto *msgLabel = new QLabel(msg, this);
        msgLabel->setObjectName("Message");
        msgLabel->setWordWrap(true);

        textLayout->addWidget(titleLabel);
        textLayout->addWidget(msgLabel);
        mainLayout->addWidget(textContainer, 1);

        if(QScreen *screen = QGuiApplication::primaryScreen())
        {
            QRect screenGeom = screen->geometry();
            move(screenGeom.width() - width() - 20, screenGeom.height() - height() - 60);
        }

        auto *selfDestructTimer = new QTimer(this);
        QObject::connect(selfDestructTimer, &QTimer::timeout, [this]()
        {
            this->close();
            this->deleteLater();
        });
        selfDestructTimer->start(duration);
    }
};

#endif      /* DEKSTOP_TOAST_H */
