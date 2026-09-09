#include "SystemMonitorWindow.h"
#include <QApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QApplication>
#include <QProcess>
#include <QDialog>
#include <QTextEdit>
#include <QMenu>
#include <QAction>

#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <unordered_map>

#define APPICON_PATH            "/usr/share/gui/icons/sysmon.png"

bool NumericSortItem::operator<(const QTreeWidgetItem& other) const
{
    int col = treeWidget()->sortColumn();

    // Check if the target column contains numeric measurements
    if(col == 1 || col == 3 || col == 4 || col == 5 || col == 6 || col == 7) 
    {
        // Strip out '%', 'MB', and any accidental spaces to parse pure numbers safely
        QString txtSelf = this->text(col).chopped(this->text(col).endsWith('%') ? 1 : 0).split(' ').first();
        QString txtOther = other.text(col).chopped(other.text(col).endsWith('%') ? 1 : 0).split(' ').first();
        
        return txtSelf.toDouble() < txtOther.toDouble();
    }

    return QTreeWidgetItem::operator<(other);
}

SystemMonitorWindow::SystemMonitorWindow()
{
    initUi();
    startWorkerThread();
}

SystemMonitorWindow::~SystemMonitorWindow()
{
    stopWorkerThread();
}

void SystemMonitorWindow::initUi()
{
    setWindowTitle("System Monitor");
    setWindowIcon(QIcon(APPICON_PATH));

    QScreen *primaryScreen = QGuiApplication::primaryScreen();

    if(primaryScreen) 
    {
        QRect screenGeometry = primaryScreen->availableGeometry();
        int screenWidth = screenGeometry.width();
        int screenHeight = screenGeometry.height();

        // Propose our desired desktop application dimensions
        int proposedWidth = 800;
        int proposedHeight = 600;

        // Enforce a ceiling limit matching hardware screen limits 
        int finalWidth = std::min(proposedWidth, screenWidth - 30);
        int finalHeight = std::min(proposedHeight, screenHeight - 30);

        resize(finalWidth, finalHeight);
    }
    else
    {
        // Fallback hard limit envelope if screen devices fail to report properties 
        resize(800, 600);
    }

    tabWidget = new QTabWidget(this);

    // --- TAB 1: PROCESS TASK MANAGER VIEW ---
    auto* taskWidget = new QWidget(tabWidget);
    auto* taskLayout = new QVBoxLayout(taskWidget);

    searchBar = new QLineEdit(taskWidget);
    searchBar->setPlaceholderText("Search by task name, user, or PID...");
    taskLayout->addWidget(searchBar);

    QObject::connect(searchBar, &QLineEdit::textChanged, [this](const QString& text) 
    {
        std::lock_guard<std::mutex> lock(filterMutex);
        currentFilterText = text.toLower().toStdString();
    });

    processTree = new QTreeWidget(taskWidget);
    processTree->setColumnCount(8);
    processTree->setHeaderLabels({"Task Name", "PID", "User", "Nice", "% CPU", "RSS", "Disk Read", "Disk Write"});
    processTree->setSortingEnabled(true);
    processTree->sortByColumn(4, Qt::DescendingOrder);
    processTree->setContextMenuPolicy(Qt::CustomContextMenu);

    QObject::connect(processTree, &QTreeWidget::customContextMenuRequested, [this](const QPoint& pos) 
    {
        this->showContextMenu(pos);
    });

    taskLayout->addWidget(processTree);

    auto* actionLayout = new QHBoxLayout();
    killButton = new QPushButton("Kill Selected Process", taskWidget);
    killButton->setStyleSheet("background-color: #d9534f; color: white; font-weight: bold; padding: 6px;");
    actionLayout->addStretch();
    actionLayout->addWidget(killButton);
    taskLayout->addLayout(actionLayout);

    QObject::connect(killButton, &QPushButton::clicked, [this]() { onKillPressed(); });

    tabWidget->addTab(taskWidget, "Processes");

    // --- TAB 2: SYSTEM RESOURCES RENDERING LIVE PLOTS ---
    auto* resourceWidget = new QWidget(tabWidget);
    auto* resourceLayout = new QVBoxLayout(resourceWidget);

    cpuGraph = new HistoryGraph(resourceWidget);
    cpuGraph->setLineColor(Qt::green);
    cpuGraph->setLabelText("Global CPU Load");
    resourceLayout->addWidget(cpuGraph);

    memGraph = new HistoryGraph(resourceWidget);
    memGraph->setLineColor(QColor(30, 144, 255)); // Dodger Blue
    memGraph->setLabelText("Global Memory Allocated");
    resourceLayout->addWidget(memGraph);

    tabWidget->addTab(resourceWidget, "Resources");
    setCentralWidget(tabWidget);
}

void SystemMonitorWindow::startWorkerThread()
{
    workerRunning = true;

    workerThread = std::thread([this]() 
    {
        std::vector<CpuTicks> prevGlobal = ProcessWorker::getGlobalCpuTicks();
        std::unordered_map<int, unsigned long long> prevProcTicks;
        auto lastSampleTime = std::chrono::steady_clock::now();

        while(workerRunning) 
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            auto now = std::chrono::steady_clock::now();
            double elapsedSec = std::chrono::duration<double>(now - lastSampleTime).count();
            lastSampleTime = now;

            std::vector<double> cpuPercents;
            double globalMem = 0.0;
            auto processes = ProcessWorker::gatherFrameData(prevGlobal, prevProcTicks, elapsedSec, cpuPercents, globalMem);

            QMetaObject::invokeMethod(QApplication::instance(), [this, cpuPercents, globalMem, procs = std::move(processes)]() mutable 
            {
                this->updateUiData(cpuPercents, globalMem, std::move(procs));
            });
        }
    });
}

void SystemMonitorWindow::stopWorkerThread()
{
    workerRunning = false;

    if(workerThread.joinable()) 
    {
        workerThread.join();
    }
}

void SystemMonitorWindow::updateUiData(const std::vector<double>& cpuPercents, double globalMem, std::vector<ProcessData> processes)
{
    // Ingest metrics to live graphs
    cpuGraph->addMultiSamples(cpuPercents);
    memGraph->addSingleSample(globalMem);

    // Set core text strings on the first loop
    static bool labelsSet = false;
    if(!labelsSet && !cpuPercents.empty()) 
    {
        std::vector<std::string> labels = {"Total CPU"};
        for(size_t i = 1; i < cpuPercents.size(); ++i) labels.push_back("Core " + std::to_string(i));
        cpuGraph->setLabels(labels);
        labelsSet = true;
    }

    QString selectedPid;
    if(auto* curr = processTree->currentItem()) selectedPid = curr->text(1);

    processTree->setSortingEnabled(false);
    processTree->clear();

    std::string localFilter;
    {
        std::lock_guard<std::mutex> lock(filterMutex);
        localFilter = currentFilterText;
    }

    for(const auto& proc : processes) 
    {
        std::string lowerName = proc.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        std::string lowerUser = proc.user;
        std::transform(lowerUser.begin(), lowerUser.end(), lowerUser.begin(), ::tolower);
        std::string pidStrNormal = std::to_string(proc.pid);

        if(!localFilter.empty()) 
        {
            if(lowerName.find(localFilter) == std::string::npos &&
               lowerUser.find(localFilter) == std::string::npos &&
               pidStrNormal.find(localFilter) == std::string::npos) 
            {
                continue;
            }
        }

        auto* item = new NumericSortItem(processTree);
        item->setText(0, QString::fromStdString(proc.name));
        item->setText(1, QString::number(proc.pid));
        item->setText(2, QString::fromStdString(proc.user));
        item->setText(3, QString::number(proc.nice));
        item->setText(4, QString("%1%").arg(proc.cpuPercent, 0, 'f', 1));
        item->setText(5, QString("%1 MB").arg(proc.rssMemoryBytes / (1024.0 * 1024.0), 0, 'f', 1));
        item->setText(6, QString("%1 MB").arg(proc.bytesRead / (1024.0 * 1024.0), 0, 'f', 2));
        item->setText(7, QString("%1 MB").arg(proc.bytesWritten / (1024.0 * 1024.0), 0, 'f', 2));

        // Check if the individual task exceeds a 50.0% load threshold
        if(proc.cpuPercent > 50.0) 
        {
            // Apply a crimson text indicator across all columns of this item
            for(int col = 0; col < processTree->columnCount(); ++col) 
            {
                item->setForeground(col, QBrush(QColor(231, 76, 60)));

                // Make the text bold for extreme consumers
                QFont font = item->font(col);
                font.setBold(true);
                item->setFont(col, font);
            }
        }

        if(QString::number(proc.pid) == selectedPid) 
        {
            processTree->setCurrentItem(item);
        }
    }
    processTree->setSortingEnabled(true);
}

void SystemMonitorWindow::onKillPressed()
{
    auto* selectedItem = processTree->currentItem();
    if(!selectedItem) 
    {
        QMessageBox::warning(this, "Selection Error", "Select a task first.");
        return;
    }

    int pid = selectedItem->text(1).toInt();
    QString name = selectedItem->text(0);
    QString ownerUser = selectedItem->text(2);

    auto choice = QMessageBox::question(this, "Terminate Confirmation",
        QString("Send SIGTERM to %1 (PID: %2)?").arg(name).arg(pid), QMessageBox::Yes | QMessageBox::No);

    if(choice != QMessageBox::Yes) return;

    // Check if the task is owned by the current user
    bool runAsRoot = (ownerUser != QString::fromStdString(getenv("USER")));

    if(!runAsRoot) 
    {
        // Normal kill for user-owned tasks
        if(::kill(pid, SIGTERM) != 0) 
        {
            runAsRoot = true; // Fallback to elevated kill if access denied
        }
        else
        {
            return; 
        }
    }

    if(runAsRoot) 
    {
#ifdef __laylaos__
        QMessageBox::critical(this, "Failed termination", "Failed to terminate process with elevated administrative privileges.");
#else
        // Privilege Escalation using a QProcess configuration wrapper
        QStringList arguments;
        arguments << "kill" << "-15" << QString::number(pid); // SIGTERM

        int exitCode = QProcess::execute("pkexec", arguments);
        if(exitCode != 0) 
        {
            QMessageBox::critical(this, "Failed termination", "Failed to terminate process with elevated administrative privileges.");
        }
#endif
    }
}

void SystemMonitorWindow::showContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = processTree->itemAt(pos);
    if(!item) return;

    int pid = item->text(1).toInt();
    QString name = item->text(0);

    QMenu menu(this);
    
    QAction* actProps = menu.addAction("Detailed Properties");
    QAction* actMaps  = menu.addAction("View Memory Maps");
    QAction* actFiles = menu.addAction("View Open Files");
    menu.addSeparator();
    QAction* actStop  = menu.addAction("Stop Process");
    QAction* actCont  = menu.addAction("Continue Process");
    QAction* actKill  = menu.addAction("Kill Process");

    QAction* selected = menu.exec(processTree->mapToGlobal(pos));
    if(!selected) return;

    if(selected == actProps)       showDetailedProperties(pid, name);
    else if(selected == actMaps)   showMemoryMaps(pid, name);
    else if(selected == actFiles)  showOpenFiles(pid, name);
    else if(selected == actStop)   sendSignalToProcess(pid, SIGSTOP, "Stop");
    else if(selected == actCont)   sendSignalToProcess(pid, SIGCONT, "Continue");
    else if(selected == actKill)   sendSignalToProcess(pid, SIGKILL, "Force Kill");
}

void SystemMonitorWindow::sendSignalToProcess(int pid, int signum, const QString& actionName)
{
    auto choice = QMessageBox::question(this, "Confirm Action",
        QString("Are you sure you want to send %1 to PID %2?").arg(actionName).arg(pid),
        QMessageBox::Yes | QMessageBox::No);
        
    if(choice != QMessageBox::Yes) return;

    if(::kill(pid, signum) != 0) 
    {
#ifdef __laylaos__
        QMessageBox::critical(this, "Signal failed", "Failed to issue signal to target process.");
#else
        // Fallback to escalated administrative privilege invocation if normal access gets rejected
        QStringList args;
        args << "kill" << QString("-%1").arg(signum) << QString::number(pid);
        if(QProcess::execute("pkexec", args) != 0) 
        {
            QMessageBox::critical(this, "Signal failed", "Failed to issue signal to target process.");
        }
#endif
    }
}

void SystemMonitorWindow::showDetailedProperties(int pid, const QString& name)
{
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(QString("Properties: %1 (PID %2)").arg(name).arg(pid));
    dialog->resize(500, 400);

    auto* layout = new QVBoxLayout(dialog);
    auto* tree = new QTreeWidget(dialog);
    tree->setColumnCount(2);

    tree->setHeaderLabels({"Property", "Value"});
    tree->setSortingEnabled(true);
    layout->addWidget(tree);

    std::ifstream file("/proc/" + std::to_string(pid) + "/status");
    std::string line;

    // Cache to avoid hitting system password databases on every line parse
    static std::unordered_map<uid_t, std::string> userCache;
    static std::unordered_map<gid_t, std::string> groupCache;

    while(std::getline(file, line)) 
    {
        size_t delimiter = line.find(':');
        if(delimiter == std::string::npos) continue;

        std::string key = line.substr(0, delimiter);
        std::string val = line.substr(delimiter + 1);

        // Strip leading whitespaces
        val.erase(0, val.find_first_not_of(" \t"));

        // Handle user/group IDs
        if(key == "Uid" || key == "Gid") 
        {
            std::stringstream ss(val);
            std::string firstIdStr;

            // Extract only the very first number (Real UID/GID)
            if(ss >> firstIdStr) 
            {
                try
                {
                    unsigned int numericId = std::stoul(firstIdStr);

                    if(key == "Uid") 
                    {
                        uid_t uid = static_cast<uid_t>(numericId);
                        if(userCache.find(uid) == userCache.end()) 
                        {
                            passwd* pwd = getpwuid(uid);
                            userCache[uid] = pwd ? pwd->pw_name : "";
                        }

                        if(!userCache[uid].empty()) 
                        {
                            val = userCache[uid] + " (" + firstIdStr + ")";
                        }
                        else
                        {
                            val = firstIdStr; // Fallback to raw UID
                        }
                    } 
                    else
                    { // key == "Gid"
                        gid_t gid = static_cast<gid_t>(numericId);
                        if(groupCache.find(gid) == groupCache.end()) 
                        {
                            group* grp = getgrgid(gid);
                            groupCache[gid] = grp ? grp->gr_name : "";
                        }

                        if(!groupCache[gid].empty()) 
                        {
                            val = groupCache[gid] + " (" + firstIdStr + ")";
                        }
                        else
                        {
                            val = firstIdStr; // Fallback to raw GID
                        }
                    }
                } catch (...) {
                    // Fallback to whatever string was parsed if conversion breaks
                    val = firstIdStr;
                }
            }
        }
        
        auto* item = new QTreeWidgetItem(tree);
        item->setText(0, QString::fromStdString(key));
        item->setText(1, QString::fromStdString(val));
    }

    tree->resizeColumnToContents(0);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SystemMonitorWindow::showMemoryMaps(int pid, const QString& name)
{
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(QString("Memory Maps: %1 (PID %2)").arg(name).arg(pid));
    dialog->resize(500, 400);

    QFont monoFont("Monospace");
    auto* layout = new QVBoxLayout(dialog);
    auto* tree = new QTreeWidget(dialog);

    tree->setColumnCount(8);
    tree->setHeaderLabels({"VM Start", "VM End", "VM Size", "Flags", "File Offset", "Device", "Inode", "Path"});
    tree->setSortingEnabled(false); // Retain structural contiguous addresses layout ordering
    layout->addWidget(tree);

    std::ifstream file("/proc/" + std::to_string(pid) + "/maps");
    std::string line;

    while(std::getline(file, line)) 
    {
        std::stringstream ss(line);
        std::string rawAddressRange, perms, offset, dev, inode, region;
        
        // Parse the first 5 mandatory space-separated columns
        if(ss >> rawAddressRange >> perms >> offset >> dev >> inode) 
        {
            std::getline(ss, region);
            region.erase(0, region.find_first_not_of(" \t")); // Trim whitespace

            // Split the raw address range by the dash character
            size_t dashIdx = rawAddressRange.find('-');
            if(dashIdx == std::string::npos) continue;

            std::string startAddrStr = rawAddressRange.substr(0, dashIdx);
            std::string endAddrStr = rawAddressRange.substr(dashIdx + 1);

            QString qStartAddr = QString::fromStdString(startAddrStr);
            QString qEndAddr = QString::fromStdString(endAddrStr);
            QString qSizeStr = "0 B";

            try
            {
                // Convert hexadecimal strings to 64-bit integers
                unsigned long long startVal = std::stoull(startAddrStr, nullptr, 16);
                unsigned long long endVal = std::stoull(endAddrStr, nullptr, 16);

                if(endVal >= startVal) 
                {
                    unsigned long long bytesDelta = endVal - startVal;
                    double sizeNum = static_cast<double>(bytesDelta);

                    // Dynamically scale byte calculation to human-readable size labels
                    if(bytesDelta >= 1024ULL * 1024ULL * 1024ULL) 
                    {
                        qSizeStr = QString("%1 GB").arg(sizeNum / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
                    }
                    else if(bytesDelta >= 1024ULL * 1024ULL) 
                    {
                        qSizeStr = QString("%1 MB").arg(sizeNum / (1024.0 * 1024.0), 0, 'f', 2);
                    }
                    else if(bytesDelta >= 1024ULL) 
                    {
                        qSizeStr = QString("%1 KB").arg(sizeNum / 1024.0, 0, 'f', 2);
                    }
                    else
                    {
                        qSizeStr = QString("%1 B").arg(bytesDelta);
                    }
                }
            } catch (...) {
                qSizeStr = "Unknown";
            }

            auto* item = new QTreeWidgetItem(tree);
            item->setText(0, qStartAddr);
            item->setText(1, qEndAddr);
            item->setText(2, qSizeStr);
            item->setText(3, QString::fromStdString(perms));
            item->setText(4, QString::fromStdString(offset));
            item->setText(5, QString::fromStdString(dev));
            item->setText(6, QString::fromStdString(inode));
            item->setText(7, region.empty() ? "[Anon Priv Memory]" : QString::fromStdString(region));

            // Set monospace font for address ranges and offsets
            item->setFont(0, monoFont);
            item->setFont(1, monoFont);
            item->setFont(3, monoFont);
            item->setFont(4, monoFont);
        }
    }

    // Adjust the header sizing for all metrics fields up to the Path column
    for(int i = 0; i < 7; ++i) 
    {
        tree->resizeColumnToContents(i);
    }

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();

}

void SystemMonitorWindow::showOpenFiles(int pid, const QString& name)
{
    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(QString("Open File Handles: %1 (PID %2)").arg(name).arg(pid));
    dialog->resize(500, 400);

    auto* layout = new QVBoxLayout(dialog);
    auto* tree = new QTreeWidget(dialog);
    tree->setColumnCount(3);
    tree->setHeaderLabels({"File Descriptor", "Type", "Path"});
    tree->setSortingEnabled(true);
    layout->addWidget(tree);

    std::string fdPath = "/proc/" + std::to_string(pid) + "/fd";
    namespace fs = std::filesystem;

    try
    {
        if(fs::exists(fdPath)) 
        {
            for(const auto& entry : fs::directory_iterator(fdPath)) 
            {
                std::error_code ec;
                auto target = fs::read_symlink(entry.path(), ec);
                if(!ec) 
                {
                    QString fdName = QString::fromStdString(entry.path().filename().string());
                    QString targetStr = QString::fromStdString(target.string());
                    QString category = "File";

                    // Determine descriptor types using standard Linux kernel prefix patterns
                    if(targetStr.startsWith("socket:"))   category = "Network Socket";
                    else if(targetStr.startsWith("pipe:")) category = "Pipe";
                    else if(targetStr.startsWith("anon_inode:")) category = "Kernel Event/Timer";
                    else if(targetStr.startsWith("/dev")) category = "Device Node";

                    auto* item = new QTreeWidgetItem(tree);
                    item->setText(0, fdName);
                    item->setText(1, category);
                    item->setText(2, targetStr);
                }
            }
        }
    } catch (...) {
        // Restricted kernel permissions are handled gracefully
    }

    tree->resizeColumnToContents(0);
    tree->resizeColumnToContents(1);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

