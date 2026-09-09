#include <QApplication>
#include <QSystemTrayIcon>
#include <QSlider>
#include <QVBoxLayout>
#include <QTimer>
#include <QFile>
#include <QIcon>
#include <QCursor>
#include <QScreen>
#include <QThread>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>

#include <map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>

#include "../../desktop/include/gui.h"
#include "../../desktop/include/keys.h"

#define GLOB                        __global_gui_data


int g_lastVolume = -1;
std::map<QString, QIcon> g_volumeIconCache;


class VolumeKeyFilter : public QObject
{
private:
    QSlider* m_slider;
    QSystemTrayIcon* m_trayIcon;
    QWidget* m_window;

public:
    VolumeKeyFilter(QSlider* slider, QSystemTrayIcon* tray, QWidget* win) 
            : QObject(win), m_slider(slider), m_trayIcon(tray), m_window(win) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if(event->type() == QEvent::KeyPress)
        {
            auto *keyEvent = static_cast<QKeyEvent*>(event);
            int currentVal = m_slider->value();

            if(keyEvent->key() == Qt::Key_VolumeUp || keyEvent->key() == Qt::Key_Up)
            {
                m_slider->setValue(std::min(100, currentVal + 5)); // Step up 5%
                return true;
            }
            else if(keyEvent->key() == Qt::Key_VolumeDown || keyEvent->key() == Qt::Key_Down)
            {
                m_slider->setValue(std::max(0, currentVal - 5)); // Step down 5%
                return true;
            }
            else if(keyEvent->key() == Qt::Key_VolumeMute)
            {
                if(currentVal > 0)
                {
                    m_slider->setValue(0);
                }
                else
                {
                    m_slider->setValue(50);
                }

                return true;
            }
            else if(keyEvent->key() == Qt::Key_Escape)
            {
                m_window->hide();
                return true;
            }
        }

        return QObject::eventFilter(watched, event);
    }
};


void playStartupChimeAsync(void)
{
    QThread *audioThread = QThread::create([]()
    {
        int dspFd = open("/dev/dsp", O_WRONLY);
        if(dspFd < 0) return;

        audio_info_t ainfo;
        if(ioctl(dspFd, AUDIO_GETINFO, &ainfo) < 0)
        {
            close(dspFd);
            return;
        }

        QFile wavFile("/usr/share/gui/audio/Appear-48.wav");
        if(wavFile.open(QIODevice::ReadOnly))
        {
            // Skip the first 44 bytes containing header tags 
            wavFile.seek(44);
            QByteArray pcmData = wavFile.readAll();
            const char *data = pcmData.constData();
            size_t bytes = 0, remaining = pcmData.size();
            size_t bufsz = ainfo.play.buffer_size;

            while(remaining)
            {
                if(remaining < bufsz)
                {
                    write(dspFd, data + bytes, remaining);
                    bytes += remaining;
                    remaining = 0;
                }
                else
                {
                    write(dspFd, data + bytes, bufsz);
                    bytes += bufsz;
                    remaining -= bufsz;
                }
            }

            //write(dspFd, pcmData.constData(), pcmData.size());
            wavFile.close();
        }

        close(dspFd);
    });

    audioThread->start();

    QObject::connect(audioThread, &QThread::finished, audioThread, &QThread::deleteLater);
}

int getHardwareVolume(void)
{
    int fd = open("/dev/dsp", O_RDONLY | O_NONBLOCK);
    if(fd < 0) return 50;

    audio_info_t ainfo;
    if(ioctl(fd, AUDIO_GETINFO, &ainfo) < 0)
    {
        close(fd);
        return 50;
    }
    close(fd);

    if(ainfo.output_muted) return 0;

    int volInfo = ainfo.play.gain;
    int volPercentage = (volInfo * 100) / 255; 

    return volPercentage;
}

void setHardwareVolume(int percentage)
{
    int fd = open("/dev/dsp", O_WRONLY | O_NONBLOCK);
    if(fd < 0) return;

    audio_info_t ainfo;
    if(ioctl(fd, AUDIO_GETINFO, &ainfo) < 0)
    {
        close(fd);
        return;
    }

    int volInfo = (percentage * 255) / 100;
    ainfo.play.gain = volInfo;

    if(percentage == 0) ainfo.output_muted = 1;
    else ainfo.output_muted = 0;

    ioctl(fd, AUDIO_SETINFO, &ainfo);

    close(fd);
}

void populateVolumeCache(void)
{
    const QStringList keys = { "volume-high", "volume-medium", "volume-low", "volume-mute" };

    for(const QString& key : keys)
    {
        QString path = QString(DEFAULT_ICON_PATH "/%1.png").arg(key);
        g_volumeIconCache[key] = QFile::exists(path) ? QIcon(path) : QIcon(DEFAULT_ICON_PATH "/sysmon.png");
    }
}

void updateVolumeTelemetry(QSystemTrayIcon *trayIcon)
{
    QString iconKey = "volume-mute";
    if(g_lastVolume > 70)       iconKey = "volume-high";
    else if(g_lastVolume > 30)  iconKey = "volume-medium";
    else if(g_lastVolume > 0)   iconKey = "volume-low";

    auto it = g_volumeIconCache.find(iconKey);
    if(it != g_volumeIconCache.end())
    {
        trayIcon->setIcon(it->second);
    }

    trayIcon->setToolTip(QString("Volume: %1%").arg(QString::number(g_lastVolume)));
}

void initKeybindings(winid_t mywinid)
{
    key_bind(mywinid, KEYCODE_VOLUP, 0, KEYBINDING_NOTIFY_ONCE);
    key_bind(mywinid, KEYCODE_VOLDN, 0, KEYBINDING_NOTIFY_ONCE);
    key_bind(mywinid, KEYCODE_AUD_MUTE, 0, KEYBINDING_NOTIFY_ONCE);
}

int main(int argc, char *argv[])
{
    // this must be called before Qt initialises the system layer
    setenv("QT_LAYLAOS_THEME", "dark", 1);

    QApplication app(argc, argv);

    // Play start sound
    playStartupChimeAsync();

    populateVolumeCache();

    // Create a hidden window that will receive VolUp and VolDown keypresses
    auto *hiddenWindow = new QWidget(nullptr);
    hiddenWindow->setWindowFlags(Qt::Popup | Qt::WindowDoesNotAcceptFocus);
    hiddenWindow->setAttribute(Qt::WA_ShowWithoutActivating);
    hiddenWindow->setGeometry(0, 0, 1, 1);
    hiddenWindow->show();
    hiddenWindow->hide();

    auto *trayIcon = new QSystemTrayIcon();
    g_lastVolume = getHardwareVolume();
    updateVolumeTelemetry(trayIcon);
    trayIcon->show();

    // Slider overlay
    auto *sliderWindow = new QWidget(nullptr);
    sliderWindow->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    sliderWindow->setFixedSize(45, 160);
    sliderWindow->setStyleSheet("QWidget { background-color: #1F1F1F; border-radius: 4px; }");

    auto *layout = new QVBoxLayout(sliderWindow);
    layout->setContentsMargins(8, 12, 8, 12);

    auto *volumeSlider = new QSlider(Qt::Vertical, sliderWindow);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(g_lastVolume);
    volumeSlider->setStyleSheet(
        "QSlider::groove:vertical { background: #252525; width: 6px; border-radius: 3px; }"
        "QSlider::sub-page:vertical { background: #252525; }"
        "QSlider::add-page:vertical { background: #1ABC9C; border-radius: 3px; }"
        "QSlider::handle:vertical { background: #ECF0F1; height: 12px; margin-left: -3px; margin-right: -3px; border-radius: 6px; }"
    );
    layout->addWidget(volumeSlider, 0, Qt::AlignCenter);

    auto *valueLabel = new QLabel("50%", sliderWindow);
    valueLabel->setAlignment(Qt::AlignCenter);
    valueLabel->setStyleSheet("color: #ECF0F1; font-weight: bold; font-size: 11px; border: none; background: transparent;");

    // Set the initial starting text based on current volume
    valueLabel->setText(QString::number(g_lastVolume) + "%");
    layout->addWidget(valueLabel, 0, Qt::AlignCenter);

    QObject::connect(volumeSlider, &QSlider::valueChanged, [trayIcon, valueLabel](int value)
    {
        g_lastVolume = value;
        setHardwareVolume(value);
        valueLabel->setText(QString::number(value) + "%");
        updateVolumeTelemetry(trayIcon);
    });

    QObject::connect(trayIcon, &QSystemTrayIcon::activated, [sliderWindow, volumeSlider, trayIcon](QSystemTrayIcon::ActivationReason reason)
    {
        if(reason == QSystemTrayIcon::Trigger)  // Left-click
        {
            if(sliderWindow->isVisible())
            {
                //sliderWindow->hide();
            }
            else
            {
                QRect iconGeometry = trayIcon->geometry();
                volumeSlider->setValue(g_lastVolume);

                int targetX = iconGeometry.x() - (sliderWindow->width() / 2);
                int targetY = iconGeometry.y() - sliderWindow->height() - 5;

                sliderWindow->move(targetX, targetY);
                sliderWindow->show();
                sliderWindow->raise();
                sliderWindow->activateWindow();
            }
        }
    });

    auto *keyFilter = new VolumeKeyFilter(volumeSlider, trayIcon, sliderWindow);
    sliderWindow->installEventFilter(keyFilter);
    hiddenWindow->installEventFilter(keyFilter);

    // Ensure we get volume keys
    winid_t mywinid = TO_WINID(GLOB.mypid, 0);
    initKeybindings(mywinid);

    return app.exec();
}

