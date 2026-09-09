#include "settings_base.h"
#include "settings_single_instance_lock.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <sys/utsname.h>

#define APPICON_PATH            "/usr/share/gui/icons/settings2.png"

const char *LOCK_FILE = "/tmp/desktop-settings-sysinfo.lock";

class SysInfoWindow : public SettingsBaseWindow
{
private:
    QLabel *m_memLabel = nullptr;

public:
    explicit SysInfoWindow(QWidget *parent = nullptr);

private:
    void updateMem();
    void parseCpuInfo(QVBoxLayout *layout);
};

SysInfoWindow::SysInfoWindow(QWidget *parent) : SettingsBaseWindow("System Info", parent)
{
    struct utsname osinfo;
    if(uname(&osinfo) < 0)
    {
        strcpy(osinfo.sysname, "Layla OS");
        strcpy(osinfo.release, "Unknown");
    }

    QString sysnameStr = QString("<b>Operating System:</b> %1").arg(osinfo.sysname);
    QString releaseStr = QString("<b>Kernel version:</b> %1").arg(osinfo.release);

    setFixedSize(420, 220);
    setWindowIcon(QIcon(APPICON_PATH));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(15, 15, 15, 15);
    layout->setSpacing(10);

    layout->addWidget(new QLabel(sysnameStr, this));
    layout->addWidget(new QLabel(releaseStr, this));

    parseCpuInfo(layout);

    m_memLabel = new QLabel("Querying System Memory Tables...", this);
    layout->addWidget(m_memLabel);
    updateMem();

    auto *timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, [this]() { updateMem(); });
    timer->start(2000);
}

void SysInfoWindow::updateMem()
{
    QFile file("/proc/meminfo");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Could not open /proc/meminfo";
        return;
    }

    QTextStream in(&file);
    long long total = 0, free = 0;

    while(true)
    {
        QString line = in.readLine();
        if(line.isNull()) break;

        QStringList parts = line.split(' ', Qt::SkipEmptyParts);

        if(parts.size() >= 2)
        {
            if(line.startsWith("MemTotal:"))
            {
                total = parts[1].toLongLong();
            } 
            else if(line.startsWith("MemFree:") || line.startsWith("MemAvailable:"))
            {
                free = parts[1].toLongLong();
            }
        }
    }

    file.close();

    m_memLabel->setText(QString("<b>RAM Status:</b> %1 MB / %2 MB Used")
                                .arg((total - free) / 1024)
                                .arg(total / 1024));
}

void SysInfoWindow::parseCpuInfo(QVBoxLayout *layout)
{
    QFile file("/proc/cpuinfo");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Could not open /proc/cpuinfo";
        layout->addWidget(new QLabel("Processor: Status unavailable", this));
        return;
    }

    QTextStream in(&file);
    int cpuCount = 0;

    QString modelName = "Unknown";
    QString cacheSize = "Unknown";
    QString cpuMhz = "Unknown";

    while(true)
    {
        QString line = in.readLine();
        if(line.isNull()) break;

        line = line.trimmed();
        if(line.isEmpty()) continue;

        QStringList parts = line.split(':', Qt::KeepEmptyParts);
        if(parts.size() < 2) continue;

        QString key = parts[0].trimmed();
        QString value = parts[1].trimmed();

        if(key == "processor")
        {
            cpuCount++;
        }
        else if(cpuCount <= 1)
        {
            if(key == "model name")
            {
                modelName = value;
            }
            else if(key == "cache size")
            {
                cacheSize = value;
            }
            else if(key == "cpu MHz")
            {
                cpuMhz = value;
            }
        }
    }

    file.close();

    if(cpuCount == 0) cpuCount = 1;

    QLabel *modelLabel = new QLabel(QString("<b>Processor:</b> %1").arg(modelName), this);
    modelLabel->setWordWrap(true);
    layout->addWidget(modelLabel);

    layout->addWidget(new QLabel(QString("<b>CPU Core Count:</b> %1 Core(s)").arg(QString::number(cpuCount)), this));
    layout->addWidget(new QLabel(QString("<b>Core Clock Speed:</b> %1 MHz").arg(cpuMhz), this));
    layout->addWidget(new QLabel(QString("<b>L2/L3 Cache Allocation:</b> %1").arg(cacheSize), this));
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Check for a running instance
    if(!SingleInstanceLock::grabSingleInstanceLock(QString(LOCK_FILE)))
    {
        return 0;
    }

    SysInfoWindow sysWindow;
    sysWindow.show();

    // Ensure the lock file is released when the window closes
    QObject::connect(&app, &QApplication::aboutToQuit, []()
    {
        SingleInstanceLock::releaseSingleInstanceLock(QString(LOCK_FILE));
    });

    return app.exec();
}

