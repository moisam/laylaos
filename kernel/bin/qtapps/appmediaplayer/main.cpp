#include <QApplication>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QVideoWidget>
#include <QSlider>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QTime>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QFileInfo>
#include <functional>

#define APPICON_PATH            "/usr/share/gui/icons/video.png"

class PlayerVideoWidget : public QVideoWidget
{
public:
    explicit PlayerVideoWidget(QWidget *parent = nullptr) : QVideoWidget(parent)
    {
        m_overlayLabel = new QLabel(this);
        m_overlayLabel->setAlignment(Qt::AlignCenter);
        m_overlayLabel->setStyleSheet(
            "QLabel {"
            "  background-color: rgba(0, 0, 0, 180);"
            "  color: white;"
            "  font-weight: bold;"
            "  font-size: 18px;"
            "  border-radius: 8px;"
            "  padding: 10px 20px;"
            "}"
        );
        m_overlayLabel->hide();

        m_opacityEffect = new QGraphicsOpacityEffect(m_overlayLabel);
        m_overlayLabel->setGraphicsEffect(m_opacityEffect);

        m_fadeAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
        m_fadeAnimation->setDuration(800);
        m_fadeAnimation->setStartValue(1.0);
        m_fadeAnimation->setEndValue(0.0);

        QObject::connect(m_fadeAnimation, &QPropertyAnimation::finished, [this]()
        {
            m_overlayLabel->hide();
        });
    }

    void setOnDoubleClickedCallback(std::function<void()> callback)
    {
        m_doubleClickedCallback = callback;
    }

    void showOverlayMessage(const QString &text)
    {
        m_fadeAnimation->stop();
        m_overlayLabel->setText(text);
        m_overlayLabel->adjustSize();
        centerOverlay();
        m_opacityEffect->setOpacity(1.0);
        m_overlayLabel->show();
        m_fadeAnimation->start();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QVideoWidget::resizeEvent(event);
        centerOverlay();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if(event->button() == Qt::LeftButton)
        {
            if(m_doubleClickedCallback)
            {
                m_doubleClickedCallback();
            }
            event->accept();
        }
        else
        {
            QVideoWidget::mouseDoubleClickEvent(event);
        }
    }

private:
    void centerOverlay()
    {
        if(m_overlayLabel && m_overlayLabel->isVisible())
        {
            int x = (width() - m_overlayLabel->width()) / 2;
            int y = (height() - m_overlayLabel->height()) / 2;
            m_overlayLabel->move(x, y);
        }
    }

    QLabel                  *m_overlayLabel;
    QGraphicsOpacityEffect  *m_opacityEffect;
    QPropertyAnimation      *m_fadeAnimation;
    std::function<void()>    m_doubleClickedCallback;
};

class VideoPlayer : public QMainWindow
{
public:
    VideoPlayer(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        setWindowTitle("Media Player");
        resize(600, 400);
        setWindowIcon(QIcon(APPICON_PATH));

        m_mediaPlayer = new QMediaPlayer(this);
        m_audioOutput = new QAudioOutput(this);
        m_videoWidget = new PlayerVideoWidget(this);

        m_videoWidget->setParent(this);
        m_mediaPlayer->setAudioOutput(m_audioOutput);
        m_mediaPlayer->setVideoOutput(m_videoWidget);

        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);

        m_playButton = new QPushButton("Play", this);
        m_playButton->setEnabled(false);
        m_playButton->setFocusPolicy(Qt::NoFocus); 

        m_positionSlider = new QSlider(Qt::Horizontal, this);
        m_positionSlider->setRange(0, 0);
        m_positionSlider->setFocusPolicy(Qt::NoFocus);

        m_timeLabel = new QLabel("00:00 / 00:00", this);
        m_timeLabel->setFixedWidth(110);
        m_timeLabel->setAlignment(Qt::AlignCenter);

        m_muteButton = new QPushButton("Mute", this);
        m_muteButton->setFixedWidth(32);
        m_muteButton->setFocusPolicy(Qt::NoFocus);
        m_isMuted = false;
        m_savedVolume = 70;

        m_volumeSlider = new QSlider(Qt::Horizontal, this);
        m_volumeSlider->setRange(0, 100);
        m_volumeSlider->setValue(m_savedVolume);
        m_volumeSlider->setFixedWidth(100);
        m_volumeSlider->setFocusPolicy(Qt::NoFocus);
        m_audioOutput->setVolume(m_savedVolume / 100.0f);

        QHBoxLayout *controlsLayout = new QHBoxLayout();
        m_controlsWidget = new QWidget(this);
        m_controlsWidget->setLayout(controlsLayout);
        controlsLayout->setContentsMargins(0, 0, 0, 0);
        
        controlsLayout->addWidget(m_playButton);
        controlsLayout->addWidget(m_positionSlider);
        controlsLayout->addWidget(m_timeLabel);
        controlsLayout->addWidget(m_muteButton);
        controlsLayout->addWidget(m_volumeSlider);

        QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

        mainLayout->setContentsMargins(5, 5, 5, 5);
        mainLayout->setSpacing(5);

        mainLayout->addWidget(m_videoWidget);
        mainLayout->addWidget(m_controlsWidget);

        mainLayout->setStretch(0, 1);  // Force video surface to take 100% of free space
        mainLayout->setStretch(1, 0);  // Tell controls row to stick to its minimum required height

        createMenus();

        connect(m_playButton, &QPushButton::clicked, [this]()
        {
            togglePlayback();
        });

        connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged, [this](QMediaPlayer::PlaybackState state) 
        {
            onPlaybackStateChanged(state);
        });

        connect(m_mediaPlayer, &QMediaPlayer::positionChanged, [this](qint64 position)
        {
            onPositionChanged(position);
        });

        connect(m_mediaPlayer, &QMediaPlayer::durationChanged, [this](qint64 duration) 
        {
            onDurationChanged(duration);
        });

        connect(m_positionSlider, &QSlider::sliderMoved, [this](int position) 
        {
            setPosition(position);
        });

        connect(m_volumeSlider, &QSlider::valueChanged, [this](int volume) 
        {
            setVolume(volume);
        });

        connect(m_muteButton, &QPushButton::clicked, [this]() 
        {
            toggleMute();
        });

        m_videoWidget->setOnDoubleClickedCallback([this]() 
        {
            toggleFullscreen();
        });

        setFocusPolicy(Qt::StrongFocus);
    }

    void loadAndPlayFile(const QString &filePath) 
    {
        if(!filePath.isEmpty() && QFileInfo::exists(filePath)) 
        {
            m_mediaPlayer->setSource(QUrl::fromLocalFile(filePath));
            m_playButton->setEnabled(true);
            m_mediaPlayer->play();
            m_videoWidget->showOverlayMessage("Loaded: " + QFileInfo(filePath).fileName());
            //m_defaultSpeedAction->setChecked(true);
            //m_mediaPlayer->setPlaybackRate(1.0);
        }
    }

protected:
    void keyPressEvent(QKeyEvent *event) override 
    {
        const qint64 standardSkipMs = 5000;

        switch(event->key()) 
        {
            case Qt::Key_Space:
                if(m_playButton->isEnabled()) { togglePlayback(); }
                event->accept();
                break;
            case Qt::Key_Left:
                if(m_mediaPlayer->isSeekable()) 
                {
                    m_mediaPlayer->setPosition(qMax(0LL, m_mediaPlayer->position() - standardSkipMs));
                    m_videoWidget->showOverlayMessage("Seek Back -5s");
                }
                event->accept();
                break;
            case Qt::Key_Right:
                if(m_mediaPlayer->isSeekable()) 
                {
                    m_mediaPlayer->setPosition(qMin(m_mediaPlayer->duration(), m_mediaPlayer->position() + standardSkipMs));
                    m_videoWidget->showOverlayMessage("Seek Forward +5s");
                }
                event->accept();
                break;
            case Qt::Key_Escape:
                if (isFullScreen()) { toggleFullscreen(); }
                event->accept();
                break;
            case Qt::Key_M:
                toggleMute();
                event->accept();
                break;
            // Map 'A' to cycle aspect options
            case Qt::Key_A:
                cycleAspectRatio();
                event->accept();
                break;
            default:
                QMainWindow::keyPressEvent(event);
                break;
        }
    }

private slots:
    void changeAspectRatio(QAction *action) 
    {
        Qt::AspectRatioMode mode = static_cast<Qt::AspectRatioMode>(action->data().toInt());
        m_videoWidget->setAspectRatioMode(mode);
        m_videoWidget->showOverlayMessage("Aspect Ratio: " + action->text());
    }

    void toggleMute() 
    {
        if(!m_isMuted) 
        {
            m_savedVolume = m_volumeSlider->value();
            m_volumeSlider->setValue(0); 
            m_muteButton->setText("Unmute");
            m_isMuted = true;
            m_videoWidget->showOverlayMessage("Mute On");
        } 
        else
        {
            m_volumeSlider->setValue(m_savedVolume == 0 ? 30 : m_savedVolume); 
            m_muteButton->setText("Mute");
            m_isMuted = false;
            m_videoWidget->showOverlayMessage(QString("Mute Off (Volume: %1%)").arg(m_volumeSlider->value()));
        }
    }

    void changePlaybackSpeed(QAction *action) 
    {
        double speed = action->data().toDouble();
        m_mediaPlayer->setPlaybackRate(speed);
        m_videoWidget->showOverlayMessage(QString("Speed: %1x").arg(speed, 0, 'f', 1));
    }

    void toggleFullscreen() 
    {
        if(!isFullScreen()) 
        {
            m_controlsWidget->hide();
            menuBar()->hide();
            showFullScreen();
            m_videoWidget->showOverlayMessage("Fullscreen Mode");
        }
        else
        {
            m_controlsWidget->show();
            menuBar()->show();
            showNormal();
            m_videoWidget->showOverlayMessage("Windowed Mode");
        }
    }

    void openFile() 
    {
        QString fileName = QFileDialog::getOpenFileName(this, "Open Video", "", 
            "Video Files (*.mp4 *.avi *.mkv *.mov);;All Files (*)");
        
        if(!fileName.isEmpty()) 
        {
            loadAndPlayFile(fileName);
        }
    }

    void togglePlayback() {
        if(m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState) 
        {
            m_mediaPlayer->pause();
        }
        else
        {
            m_mediaPlayer->play();
        }
    }

    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state) 
    {
        if(state == QMediaPlayer::PlayingState) 
        {
            m_playButton->setText("Pause");
            m_videoWidget->showOverlayMessage("Play");
        }
        else
        {
            m_playButton->setText("Play");
            m_videoWidget->showOverlayMessage("Pause");
        }
    }

    void onPositionChanged(qint64 position) 
    {
        if(!m_positionSlider->isSliderDown()) 
        {
            m_positionSlider->setValue(static_cast<int>(position));
        }
        updateTimeLabel(position, m_mediaPlayer->duration());
    }

    void onDurationChanged(qint64 duration) 
    {
        m_positionSlider->setRange(0, static_cast<int>(duration));
        updateTimeLabel(m_mediaPlayer->position(), duration);
    }

    void setPosition(int position) 
    {
        m_mediaPlayer->setPosition(position);
    }

    void setVolume(int volume) 
    {
        m_audioOutput->setVolume(volume / 100.0f);
        if(volume > 0 && m_isMuted) 
        {
            m_muteButton->setText("Mute");
            m_isMuted = false;
        }
        else if(volume == 0 && !m_isMuted) 
        {
            m_muteButton->setText("Unmute");
            m_isMuted = true;
        }
        m_videoWidget->showOverlayMessage(QString("Volume: %1%").arg(volume));
    }

    void showAbout() 
    {
        QMessageBox::about(this, "About Player","Qt6 Video Player\n\nShortcuts:\n- Spacebar: Play/Pause\n- Left/Right: Seek 5s\n- M Key: Mute\n- A Key: Cycle Aspect Ratio\n- Escape: Normal Window");
    }

private:
    void cycleAspectRatio() 
    {
        QList<QAction*> actions = m_aspectGroup->actions();
        for(int i = 0; i < actions.count(); ++i) 
        {
            if(actions.at(i)->isChecked()) 
            {
                int nextIndex = (i + 1) % actions.count();
                actions.at(nextIndex)->setChecked(true);
                changeAspectRatio(actions.at(nextIndex));
                break;
            }
        }
    }

    void createMenus() 
    {
        // File Menu
        QMenu *fileMenu = menuBar()->addMenu("&File");
        QAction *openAction = fileMenu->addAction("&Open Video File...");
        openAction->setShortcut(QKeySequence::Open);
        connect(openAction, &QAction::triggered, [this]() { openFile(); });
        fileMenu->addSeparator();
        QAction *exitAction = fileMenu->addAction("E&xit");
        exitAction->setShortcut(QKeySequence::Quit);
        connect(exitAction, &QAction::triggered, [this]() { close(); });

        // Playback Menu
        QMenu *playbackMenu = menuBar()->addMenu("&Playback");
        QMenu *speedSubMenu = playbackMenu->addMenu("&Playback Speed");
        QActionGroup *speedGroup = new QActionGroup(this);
        speedGroup->setExclusive(true);

        QList speeds = {0.5, 1.0, 1.5, 2.0};
        for(double speed : speeds) 
        {
            QAction *action = speedSubMenu->addAction(QString("%1x").arg(speed, 0, 'f', 1));
            action->setCheckable(true);
            action->setData(speed);
            speedGroup->addAction(action);
            if(speed == 1.0) 
            {
                action->setChecked(true);
                m_defaultSpeedAction = action;
            }
        }

        connect(speedGroup, &QActionGroup::triggered, [this](QAction *action) 
        {
            changePlaybackSpeed(action);
        });

        // Video / Aspect Ratio Menu
        QMenu *videoMenu = menuBar()->addMenu("&Video");
        QMenu *aspectSubMenu = videoMenu->addMenu("&Aspect Ratio");
        m_aspectGroup = new QActionGroup(this);
        m_aspectGroup->setExclusive(true);

        struct AspectRatioOption 
        {
            QString label;
            Qt::AspectRatioMode mode;
        };

        QList<AspectRatioOption> options = 
        {
            {"Keep Aspect Ratio (Auto)", Qt::KeepAspectRatio},
            {"Ignore Aspect Ratio (Stretch)", Qt::IgnoreAspectRatio},
            {"Keep Aspect Ratio By Expanding", Qt::KeepAspectRatioByExpanding}
        };

        for(const auto &opt : options) 
        {
            QAction *action = aspectSubMenu->addAction(opt.label);
            action->setCheckable(true);
            action->setData(static_cast<int>(opt.mode));
            m_aspectGroup->addAction(action);

            if(opt.mode == Qt::KeepAspectRatio) 
            {
                action->setChecked(true);
            }
        }

        connect(m_aspectGroup, &QActionGroup::triggered, [this](QAction *action) 
        {
            changeAspectRatio(action);
        });

        // Help Menu
        QMenu *helpMenu = menuBar()->addMenu("&Help");
        QAction *aboutAction = helpMenu->addAction("&About");
        connect(aboutAction, &QAction::triggered, [this]() { showAbout(); });
    }

    void updateTimeLabel(qint64 position, qint64 duration) 
    {
        QString format = (duration >= 3600000) ? "hh:mm:ss" : "mm:ss";
        QTime currentTime((position / 3600000) % 24, (position / 60000) % 60, (position / 1000) % 60);
        QTime totalTime((duration / 3600000) % 24, (duration / 60000) % 60, (duration / 1000) % 60);
        m_timeLabel->setText(currentTime.toString(format) + " / " + totalTime.toString(format));
    }

    QMediaPlayer      *m_mediaPlayer;
    QAudioOutput      *m_audioOutput;
    PlayerVideoWidget *m_videoWidget;
    QWidget      *m_controlsWidget;
    QPushButton  *m_playButton;
    QPushButton  *m_muteButton;
    QSlider      *m_positionSlider;
    QSlider      *m_volumeSlider;
    QLabel       *m_timeLabel;
    QAction      *m_defaultSpeedAction;
    QActionGroup *m_aspectGroup; 
    bool        m_isMuted;
    int         m_savedVolume;
};

int main(int argc, char *argv[]) 
{
    QApplication app(argc, argv);
    VideoPlayer player;
    player.show();

    if(argc > 1) 
    {
        QString filePath = QString::fromUtf8(argv[1]);
        player.loadAndPlayFile(filePath);
    }

    return app.exec();
}

