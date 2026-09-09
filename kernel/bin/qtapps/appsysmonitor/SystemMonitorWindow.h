#ifndef SYSMONITOR_WINDOW_H
#define SYSMONITOR_WINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <atomic>
#include <thread>
#include <mutex>
#include "ProcessWorker.h"
#include "HistoryGraph.h"

class NumericSortItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& other) const override;
};

class SystemMonitorWindow : public QMainWindow
{
public:
    SystemMonitorWindow();
    ~SystemMonitorWindow();

    void updateUiData(const std::vector<double>& cpuPercents, double globalMem, std::vector<ProcessData> processes);

private:
    void initUi();
    void startWorkerThread();
    void stopWorkerThread();
    void onKillPressed();

    void showContextMenu(const QPoint& pos);
    void showDetailedProperties(int pid, const QString& name);
    void showMemoryMaps(int pid, const QString& name);
    void showOpenFiles(int pid, const QString& name);
    void sendSignalToProcess(int pid, int signum, const QString& actionName);

    QTabWidget* tabWidget = nullptr;
    QLineEdit* searchBar = nullptr;
    QTreeWidget* processTree = nullptr;
    QPushButton* killButton = nullptr;

    HistoryGraph* cpuGraph = nullptr;
    HistoryGraph* memGraph = nullptr;

    std::atomic<bool> workerRunning{false};
    std::thread workerThread;
    std::mutex filterMutex;
    std::string currentFilterText;
    std::vector<double> currentCpuPercents;
};

#endif      /* SYSMONITOR_WINDOW_H */
