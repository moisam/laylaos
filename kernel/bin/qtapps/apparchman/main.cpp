#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QFormLayout>
#include <QLabel>
#include <QStatusBar>
#include <QCommandLineParser>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QProgressDialog>
#include <QShortcut>
#include <QKeySequence>
#include <QIcon>
#include <QDirIterator>
#include <QDateTime>
#include <memory>
#include <functional>
#include <vector>
#include "archive_backend.hpp"

#define APPICON_PATH            "/usr/share/gui/icons/archive.png"

class ArchiveTableItem : public QTableWidgetItem
{
public:
    ArchiveTableItem(const QString& text, bool isDirectory, bool isParentLink = false)
        : QTableWidgetItem(text), m_isDir(isDirectory), m_isParent(isParentLink) {}
        
    ArchiveTableItem(const QIcon& icon, const QString& text, bool isDirectory, bool isParentLink = false)
        : QTableWidgetItem(icon, text), m_isDir(isDirectory), m_isParent(isParentLink) {}

    // Overriding the less-than operator dictates how columns sort themselves
    bool operator<(const QTableWidgetItem& other) const override
    {
        const auto *rhs = dynamic_cast<const ArchiveTableItem*>(&other);
        if(!rhs) return QTableWidgetItem::operator<(other);

        // Rule 1: The ".." parent directory link always stays at the absolute top
        if(m_isParent) return true;
        if(rhs->m_isParent) return false;

        // Rule 2: Folders always stack above files
        if(m_isDir != rhs->m_isDir) return m_isDir;

        // Rule 3: Fall back to default alphabetical sorting if types match
        return QTableWidgetItem::operator<(other);
    }

private:
    bool m_isDir;
    bool m_isParent;
};

class FunctionalEventFilter : public QObject
{
public:
    using FilterFunction = std::function<bool(QObject*, QEvent*)>;
    explicit FunctionalEventFilter(FilterFunction filterFunc, QObject* parent = nullptr)
        : QObject(parent), m_filterFunc(std::move(filterFunc)) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if(m_filterFunc)
        {
            return m_filterFunc(watched, event);
        }

        return QObject::eventFilter(watched, event);
    }

private:
    FilterFunction m_filterFunc;
};


QString formatSize(int64_t bytes, bool isDir)
{
    if(isDir) return "--";
    if(bytes < 1024) return QString("%1 B").arg(bytes);
    if(bytes < 1024 * 1024) return QString("%1 KB").arg(double(bytes) / 1024, 0, 'f', 1);
    return QString("%1 MB").arg(double(bytes) / (1024 * 1024), 0, 'f', 1);
}

QString formatPermissions(uint16_t mode)
{
    static const char* chars[] = { "---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx" };
    QString res = "";
    res += chars[(mode >> 6) & 7];
    res += chars[(mode >> 3) & 7];
    res += chars[mode & 7];
    return res;
}

void updateStatusBarText(QTableWidget* tableWidget, QStatusBar* statusBar)
{
    int totalRows = tableWidget->rowCount();
    int visibleRows = 0, dataRows = 0, selectedFilesCount = 0, selectedFoldersCount = 0;
    int64_t selectedBytes = 0;

    QList<QTableWidgetItem*> selectedItems = tableWidget->selectedItems();
    QSet<int> uniqueSelectedRows;

    for(auto *item : selectedItems)
    {
        uniqueSelectedRows.insert(tableWidget->row(item));
    }

    for(int i = 0; i < totalRows; ++i)
    {
        auto *nameItem = tableWidget->item(i, 0);
        if(!nameItem) continue;

        QString rawName = nameItem->data(Qt::UserRole).toString();
        if(rawName == "..") continue; 

        dataRows++;
        if(!tableWidget->isRowHidden(i)) visibleRows++;

        if(uniqueSelectedRows.contains(i))
        {
            auto *typeItem = tableWidget->item(i, 1);
            auto *sizeItem = tableWidget->item(i, 2);

            if(typeItem && typeItem->text() == "File")
            {
                selectedFilesCount++;
                QString cleanSizeStr = sizeItem->text();

                if(cleanSizeStr != "--")
                {
                    double val = cleanSizeStr.section(' ', 0, 0).toDouble();
                    QString unit = cleanSizeStr.section(' ', 1, 1);

                    if(unit == "B") selectedBytes += static_cast<int64_t>(val);
                    else if(unit == "KB") selectedBytes += static_cast<int64_t>(val * 1024);
                    else if(unit == "MB") selectedBytes += static_cast<int64_t>(val * 1024 * 1024);
                }
            }
            else if(typeItem && typeItem->text() == "Folder")
            {
                selectedFoldersCount++;
            }
        }
    }

    QString statusMsg = QString("Items: %1 total").arg(dataRows);
    if(visibleRows != dataRows)
    {
        statusMsg += QString(" (%1 matched filter)").arg(visibleRows);
    }

    if(!uniqueSelectedRows.isEmpty())
    {
        statusMsg += " | Selected: ";
        QStringList parts;
        if(selectedFoldersCount > 0) parts << QString("%1 folder(s)").arg(selectedFoldersCount);
        if(selectedFilesCount > 0) parts << QString("%1 file(s) (%2)")
                                                    .arg(selectedFilesCount)
                                                    .arg(formatSize(selectedBytes, false));
        statusMsg += parts.join(", ");
    }

    statusBar->showMessage(statusMsg);
}

void updateTableView(QTableWidget *tableWidget, const ArchiveBackend& backend,
                     const QString& currentPath, QLineEdit *searchBar,
                     QStatusBar *statusBar, QLineEdit *locationBar)
{
    tableWidget->setSortingEnabled(false); // Disable temporarily during batch insert
    tableWidget->setRowCount(0);
    if(searchBar) searchBar->clear(); 
    if(locationBar && locationBar->text() != currentPath) locationBar->setText(currentPath);
    int row = 0;

    // Load standard unified icons dynamically from the OS's native theme
    QIcon folderIcon = QIcon::fromTheme("folder", QApplication::style()->standardIcon(QStyle::SP_DirIcon));
    QIcon fileIcon = QIcon::fromTheme("text-x-generic", 
                                    QApplication::style()->standardIcon(QStyle::SP_FileIcon));
    QIcon goUpIcon = QIcon::fromTheme("go-up", 
                                    QApplication::style()->standardIcon(QStyle::SP_FileDialogToParent));

    if(currentPath != "/")
    {
        tableWidget->insertRow(row);
        auto *upItem = new ArchiveTableItem(goUpIcon, "..", true, true);
        tableWidget->setItem(row, 0, upItem);
        tableWidget->setItem(row, 1, new ArchiveTableItem("--", false));
        tableWidget->setItem(row, 2, new ArchiveTableItem("--", false));
        tableWidget->setItem(row, 3, new ArchiveTableItem("--", false));
        tableWidget->item(row, 0)->setData(Qt::UserRole, "..");
        row++;
    }

    QList<ArchiveNode> contents = backend.getDirectoryContents(currentPath);
    for(const auto& node : contents)
    {
        tableWidget->insertRow(row);
        QString displayName = node.name + (node.isDirectory ? "/" : "");
        QIcon itemIcon = node.isDirectory ? folderIcon : fileIcon;
        auto *nameItem = new ArchiveTableItem(itemIcon, displayName, node.isDirectory);
        nameItem->setData(Qt::UserRole, node.name);

        tableWidget->setItem(row, 0, nameItem);
        tableWidget->setItem(row, 1, new ArchiveTableItem(node.isDirectory ? "Folder" : "File", 
                                                                                node.isDirectory));
        tableWidget->setItem(row, 2, new ArchiveTableItem(formatSize(node.size, node.isDirectory), 
                                                                                node.isDirectory));
        tableWidget->setItem(row, 3, new ArchiveTableItem(formatPermissions(node.mode), 
                                                                                node.isDirectory));
        row++;
    }

    // Re-enable sorting and trigger an immediate alphabetical sort on column 0 (Name)
    tableWidget->setSortingEnabled(true);
    tableWidget->sortByColumn(0, Qt::AscendingOrder);

    if(statusBar)
    {
        updateStatusBarText(tableWidget, statusBar);
    }
}

void showPropertiesDialog(QWidget *parent, const ArchiveBackend& backend)
{
    if(!backend.hasActiveArchive())
    {
        QMessageBox::warning(parent, "Properties", "No archive is currently open.");
        return;
    }

    ArchiveStats stats = backend.getStats();
    auto *dialog = new QDialog(parent);
    dialog->setWindowTitle("Archive Properties");
    dialog->setMinimumWidth(300);
    auto *layout = new QFormLayout(dialog);

    double ratio = 100.0;
    if(stats.totalUncompressedSize > 0)
    {
        ratio = (double(stats.totalCompressedSize) / double(stats.totalUncompressedSize)) * 100.0;
    }

    layout->addRow(new QLabel("<b>File Name:</b>"), new QLabel(stats.fileName, dialog));
    layout->addRow(new QLabel("<b>Full Path:</b>"), new QLabel(stats.filePath, dialog));
    layout->addRow(new QLabel("<b>Archive Type:</b>"), new QLabel(stats.formatName, dialog));
    layout->addRow(new QLabel("<b>File Count:</b>"), 
                            new QLabel(QString::number(stats.totalFilesCount), dialog));
    layout->addRow(new QLabel("<b>Compressed Size:</b>"), 
                            new QLabel(formatSize(stats.totalCompressedSize, false), dialog));
    layout->addRow(new QLabel("<b>Uncompressed Size:</b>"), 
                            new QLabel(formatSize(stats.totalUncompressedSize, false), dialog));
    layout->addRow(new QLabel("<b>Compression Ratio:</b>"), 
                            new QLabel(QString("%1%").arg(ratio, 0, 'f', 1), dialog));

    auto *closeBtn = new QPushButton("Close", dialog);
    QObject::connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addRow(closeBtn);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->exec();
}


int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Archive Manager");

    auto *mainWindow = new QMainWindow();
    auto *centralWidget = new QWidget(mainWindow);
    auto *mainLayout = new QVBoxLayout(centralWidget);
    auto *topActionLayout = new QHBoxLayout();

    // --- Layout Components Initialization ---
    auto *backBtn = new QPushButton(QIcon::fromTheme("go-previous", QApplication::style()->standardIcon(QStyle::SP_ArrowBack)), "", centralWidget);
    auto *forwardBtn = new QPushButton(QIcon::fromTheme("go-next", QApplication::style()->standardIcon(QStyle::SP_ArrowForward)), "", centralWidget);
    auto *upBtn = new QPushButton(QIcon::fromTheme("go-up", QApplication::style()->standardIcon(QStyle::SP_FileDialogToParent)), "", centralWidget);
    auto *homeBtn = new QPushButton(QIcon::fromTheme("go-home", QApplication::style()->standardIcon(QStyle::SP_DirHomeIcon)), "", centralWidget);

    backBtn->setFixedWidth(30);
    forwardBtn->setFixedWidth(30);
    upBtn->setFixedWidth(30);
    homeBtn->setFixedWidth(30);

    auto *locationBar = new QLineEdit(centralWidget);
    locationBar->setPlaceholderText("Virtual Path...");

    auto *searchBar = new QLineEdit(centralWidget);
    searchBar->setPlaceholderText("Filter current folder...");
    searchBar->setClearButtonEnabled(true);

    auto *tableWidget = new QTableWidget(centralWidget);
    tableWidget->setColumnCount(4);
    tableWidget->setHorizontalHeaderLabels({"Name", "Type", "Size", "Permissions"});

    // Changes: Stretch Name column, make others compact, and hide line numbers
    tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tableWidget->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tableWidget->verticalHeader()->setVisible(false); // Removes line numbers on the left

    tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection); 
    tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    tableWidget->setIconSize(QSize(20, 20)); 
    tableWidget->setShowGrid(false);

    auto *statusBar = new QStatusBar(mainWindow);
    mainWindow->setStatusBar(statusBar);
    statusBar->showMessage("Ready. Open or Drag & Drop an archive to begin.");

    // Assemble the top action row
    topActionLayout->addWidget(backBtn);
    topActionLayout->addWidget(forwardBtn);
    topActionLayout->addWidget(upBtn);
    topActionLayout->addWidget(homeBtn);
    topActionLayout->addWidget(locationBar, 2);     // make it stretch
    topActionLayout->addWidget(searchBar, 1);

    mainLayout->addLayout(topActionLayout);
    mainLayout->addWidget(tableWidget);

    centralWidget->setLayout(mainLayout);
    mainWindow->setCentralWidget(centralWidget);

    // --- Shared Paths ---
    auto backend = std::make_shared<ArchiveBackend>();
    auto currentVirtualPath = std::make_shared<QString>("/");
    auto dynamicLoadedPath = std::make_shared<QString>("");

    // Create a unique session folder name based on this process ID
    QString sessionSandboxPath = QDir::tempPath() + QString("/apparchman_extracted_%1")
                                                        .arg(QCoreApplication::applicationPid());

    auto historyBackwardStack = std::make_shared<std::vector<QString>>();
    auto historyForwardStack = std::make_shared<std::vector<QString>>();

    std::shared_ptr<std::function<void(bool)>> setInterfaceStateActive = std::make_shared<std::function<void(bool)>>();

    // --- Navigation & Dynamic History Management ---
    auto navigateToPath = [=](const QString& targetPath, bool clearForwardHistory) mutable
    {
        if(!backend->hasActiveArchive()) return;
        QString cleanTarget = targetPath.trimmed();

        if(!cleanTarget.startsWith("/")) cleanTarget.prepend("/");
        if(cleanTarget.endsWith("/") && cleanTarget.length() > 1) cleanTarget.chop(1);

        if(!backend->isDir(cleanTarget) && cleanTarget != "/")
        {
            QMessageBox::warning(mainWindow, "Navigation Error", "The specified virtual path does not exist (or is not a folder).");
            locationBar->setText(*currentVirtualPath);  // Restore last position
            return;
        }

        // Push current path onto history stack before moving
        if(*currentVirtualPath != cleanTarget)
        {
            historyBackwardStack->push_back(*currentVirtualPath);
            if(clearForwardHistory)
            {
                historyForwardStack->clear(); // New navigation branch truncates active forward options
            }
        }

        *currentVirtualPath = cleanTarget;
        updateTableView(tableWidget, *backend, *currentVirtualPath, searchBar, statusBar, locationBar);

        if(*setInterfaceStateActive) (*setInterfaceStateActive)(true); // Sync buttons state
    };

    // --- Load File At Startup ---
    auto handleLoadFile = [tableWidget, backend, currentVirtualPath, dynamicLoadedPath, 
                           searchBar, statusBar, mainWindow, setInterfaceStateActive, 
                           locationBar, historyBackwardStack, historyForwardStack](const QString& path)
    {

        if(path.isEmpty()) return;
        if(backend->loadArchive(path))
        {
            *dynamicLoadedPath = path;
            *currentVirtualPath = "/";
            historyBackwardStack->clear();
            historyForwardStack->clear();

            QString archiveName = QFileInfo(path).fileName();
            mainWindow->setWindowTitle(QString("%1 - Archive Manager").arg(archiveName));

            if(*setInterfaceStateActive) (*setInterfaceStateActive)(true);
            updateTableView(tableWidget, *backend, *currentVirtualPath, searchBar, statusBar, locationBar);
        }
        else
        {
            if(*setInterfaceStateActive) (*setInterfaceStateActive)(false);
            QMessageBox::critical(nullptr, "Error", "Failed to load archive.");
        }
    };

    // --- Drag and Drop Filters ---
    mainWindow->setAcceptDrops(true);

    auto *dropFilter = new FunctionalEventFilter([handleLoadFile, backend, currentVirtualPath, tableWidget, searchBar, statusBar, locationBar](QObject*, QEvent* event) -> bool
    {
        if(event->type() == QEvent::DragEnter)
        {
            auto *dragEvent = static_cast<QDragEnterEvent*>(event);

            if(dragEvent->mimeData()->hasUrls())
            {
                dragEvent->acceptProposedAction();
                return true;
            }
        }
        else if(event->type() == QEvent::Drop)
        {
            auto *dropEvent = static_cast<QDropEvent*>(event);
            if(dropEvent->mimeData()->hasUrls())
            {
                QList<QUrl> urlList = dropEvent->mimeData()->urls();
                if(!urlList.isEmpty())
                {
                    QString localFilePath = urlList.first().toLocalFile();
                    if(!localFilePath.isEmpty())
                    {
                        QFileInfo dropInfo(localFilePath);

                        // If an archive is open and the dropped item is a folder or file, append it
                        if(backend->hasActiveArchive() &&
                           (dropInfo.isDir() ||
                             !localFilePath.endsWith(".zip") && !localFilePath.endsWith(".Z") &&
                             !localFilePath.endsWith(".tar") &&
                             !localFilePath.endsWith(".gz") && !localFilePath.endsWith(".bz2") &&
                             !localFilePath.endsWith(".xz") && !localFilePath.endsWith(".tgz") &&
                             !localFilePath.endsWith(".lz") && !localFilePath.endsWith(".lz4") &&
                             !localFilePath.endsWith(".lzma") &&
                             !localFilePath.endsWith(".zst") && !localFilePath.endsWith(".7z")))
                        {
                            if(backend->addItems(QStringList{localFilePath}, *currentVirtualPath))
                            {
                                updateTableView(tableWidget, *backend, *currentVirtualPath, searchBar, statusBar, locationBar);
                            }
                        }
                        else
                        {
                            // Open the dropped file as a new archive
                            handleLoadFile(localFilePath);
                        }

                        dropEvent->acceptProposedAction();
                        return true;
                    }
                }
            }
        }

        return false;
    }, mainWindow);

    mainWindow->installEventFilter(dropFilter);

    // --- Double-Click & Item Activation ---
    auto handleItemActivation = [=](int row) mutable
    {
        auto *nameItem = tableWidget->item(row, 0);
        if(!nameItem) return;

        QString extractedName = nameItem->data(Qt::UserRole).toString();
        if(extractedName == "..")
        {
            QFileInfo info(*currentVirtualPath);
            QString parent = info.path();
            navigateToPath((parent == "." || parent.isEmpty()) ? "/" : parent, true);
            return;
        }

        QString targetInternalPath = *currentVirtualPath;
        if(!targetInternalPath.endsWith("/")) targetInternalPath += "/";
        targetInternalPath += extractedName;

        if(backend->isDir(targetInternalPath))
        {
            navigateToPath(targetInternalPath, true);
        }
        else
        {
            QString tmpLocation = sessionSandboxPath + "/" + extractedName;
            if(backend->extractFile(targetInternalPath, tmpLocation))
            {
                QDesktopServices::openUrl(QUrl::fromLocalFile(tmpLocation));
            }
            else
            {
                QMessageBox::critical(mainWindow, "Extraction Error", "Could not extract selected file.");
            }
        }
    };

    // --- Search Bar ---
    QObject::connect(searchBar, &QLineEdit::textChanged, mainWindow, [tableWidget, statusBar](const QString& text)
    {
        QString filterText = text.trimmed();
        for(int i = 0; i < tableWidget->rowCount(); ++i)
        {
            auto *item = tableWidget->item(i, 0);
            if(!item) continue;

            QString internalRawName = item->data(Qt::UserRole).toString();
            if(internalRawName == "..")
            {
                tableWidget->setRowHidden(i, false);
                continue;
            }

            if(filterText.isEmpty() || item->text().contains(filterText, Qt::CaseInsensitive))
            {
                tableWidget->setRowHidden(i, false);
            }
            else
            {
                tableWidget->setRowHidden(i, true);
            }
        }

        updateStatusBarText(tableWidget, statusBar);
    });

    // --- Main Menu ---
    // --- File Menu ---
    QMenu *fileMenu = mainWindow->menuBar()->addMenu("&File");
    QAction *openAction = fileMenu->addAction("&Open Archive...");
    openAction->setShortcut(QKeySequence::Open);

    QObject::connect(openAction, &QAction::triggered, mainWindow, [handleLoadFile, mainWindow]()
    {
        QString file = QFileDialog::getOpenFileName(mainWindow, "Select Archive", "", "Archives (*.zip *.lz *.lz4 *.lzma *.gz *.bz2 *.xz *.tgz *.zst *.7z *.Z *.tar)");
        handleLoadFile(file);
    });

    QAction *addAction = fileMenu->addAction("Add File(s)...");
    QObject::connect(addAction, &QAction::triggered, mainWindow, [=]() mutable
    {
        if(!backend->hasActiveArchive()) return;
        QStringList files = QFileDialog::getOpenFileNames(mainWindow, "Select File(s) to Add");
        if(!files.isEmpty() && backend->addItems(files, *currentVirtualPath))
        {
            updateTableView(tableWidget, *backend, *currentVirtualPath, searchBar, statusBar, locationBar);
        }
    });

    QAction *addFolderAction = fileMenu->addAction("Add Folder...");
    QObject::connect(addFolderAction, &QAction::triggered, mainWindow, [=]() mutable
    {
        if(!backend->hasActiveArchive()) return;
        QString folder = QFileDialog::getExistingDirectory(mainWindow, "Select Folder to Add");
        if(!folder.isEmpty() && backend->addItems(QStringList{folder}, *currentVirtualPath))
        {
            updateTableView(tableWidget, *backend, *currentVirtualPath, searchBar, statusBar, locationBar);
        }
    });

    QAction *propsAction = fileMenu->addAction("&Properties");
    propsAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Return));

    QObject::connect(propsAction, &QAction::triggered, mainWindow, [mainWindow, backend]()
    {
        showPropertiesDialog(mainWindow, *backend);
    });

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction("&Quit");
    quitAction->setShortcut(QKeySequence::Quit);
    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);

    // --- Extract Menu ---
    QMenu *extractMenu = mainWindow->menuBar()->addMenu("&Extract");
    QAction *extractSelectedMenuAction = extractMenu->addAction("Extract Selected");
    QAction *extractAllMenuAction = extractMenu->addAction("Extract Full Archive...");
    extractAllMenuAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));

    // --- Navigate Menu ---
    QMenu *navigateMenu = mainWindow->menuBar()->addMenu("&Navigate");
    QAction *navBackAction = navigateMenu->addAction("Backward");
    navBackAction->setShortcut(QKeySequence::Back);

    QAction *navForwardAction = navigateMenu->addAction("Forward");
    navForwardAction->setShortcut(QKeySequence::Forward);

    QAction *navUpAction = navigateMenu->addAction("Up to Parent");
    navUpAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));

    QAction *navHomeAction = navigateMenu->addAction("Go to Home (Root)");
    navHomeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Home));

    // --- Help Menu ---
    QMenu *menuHelp = mainWindow->menuBar()->addMenu("&Help");
    QAction *actShortcuts = menuHelp->addAction("&Keyboard shortcuts");
    actShortcuts->setShortcut(QKeySequence(Qt::Key_F1));

    QAction *actAbout = menuHelp->addAction("&About...");

    QObject::connect(actShortcuts, &QAction::triggered, centralWidget, [mainWindow]()
    {
        QMessageBox msgBox(mainWindow);
        msgBox.setWindowTitle("Keyboard Shortcuts");
        msgBox.setTextFormat(Qt::MarkdownText);

        msgBox.setText(
            "| Shortcut | Action |\n"
            "| :--- | :--- |\n"
            "| **Delete** | Remove selected files from archive |\n"
            "| **Alt + Enter** | Show archive properties |\n"
            "| **Alt + Left** | Go backward (history navigation) |\n"
            "| **Alt + Right** | Go forward (history navigation) |\n"
            "| **Alt + Up** | Go to parent folder |\n"
            "| **Ctrl + E** | Extract archive |\n"
            "| **Ctrl + O** | Open archive |\n"
            "| **Ctrl + Q** | Quit |\n"
            "| **Ctrl + Home** | Go to archive root |\n"
        );
    
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });

    QObject::connect(actAbout, &QAction::triggered, centralWidget, [mainWindow]()
    {
        QMessageBox msgBox(mainWindow);
        msgBox.setWindowTitle("About Archive Manager");
        msgBox.setTextFormat(Qt::MarkdownText);

        msgBox.setText(
            "Archive manager built using **Qt 6**.\n\n"
            "Tailored to run seamlessly across traditional GNU/Linux host environments, LaylaOS, and "
            "custom bare-metal hobby operating system platforms."
        );

        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });

    // --- Shortcut Event Triggers ---
    auto triggerBackwardNav = [=]() mutable
    {
        if(historyBackwardStack->empty()) return;
        QString lastPath = historyBackwardStack->back();
        historyBackwardStack->pop_back();
        historyForwardStack->push_back(*currentVirtualPath);
        navigateToPath(lastPath, false); // Bypass clearing the forward stack
    };

    auto triggerForwardNav = [=]() mutable
    {
        if(historyForwardStack->empty()) return;
        QString nextPath = historyForwardStack->back();
        historyForwardStack->pop_back();
        navigateToPath(nextPath, false);
    };

    auto triggerUpNav = [=]() mutable
    {
        if(!backend->hasActiveArchive() || *currentVirtualPath == "/") return;
        QFileInfo info(*currentVirtualPath);
        QString parent = info.path();
        navigateToPath((parent == "." || parent.isEmpty()) ? "/" : parent, true);
    };

    auto triggerHomeNav = [=]() mutable
    {
        navigateToPath("/", true);
    };

    // Button Clicks
    QObject::connect(backBtn, &QPushButton::clicked, mainWindow, triggerBackwardNav);
    QObject::connect(forwardBtn, &QPushButton::clicked, mainWindow, triggerForwardNav);
    QObject::connect(upBtn, &QPushButton::clicked, mainWindow, triggerUpNav);
    QObject::connect(homeBtn, &QPushButton::clicked, mainWindow, triggerHomeNav);

    // Navigate Menu Actions
    QObject::connect(navBackAction, &QAction::triggered, mainWindow, triggerBackwardNav);
    QObject::connect(navForwardAction, &QAction::triggered, mainWindow, triggerForwardNav);
    QObject::connect(navUpAction, &QAction::triggered, mainWindow, triggerUpNav);
    QObject::connect(navHomeAction, &QAction::triggered, mainWindow, triggerHomeNav);

    // --- Keyboard shortcut handling ---
    auto *backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), mainWindow);
    QObject::connect(backspaceShortcut, &QShortcut::activated, mainWindow, triggerUpNav);

    auto *focusSearchShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), mainWindow);
    QObject::connect(focusSearchShortcut, &QShortcut::activated, mainWindow, [searchBar]()
    {
        searchBar->setFocus(Qt::ShortcutFocusReason);
        searchBar->selectAll();
    });

    auto *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Return), mainWindow);
    QObject::connect(enterShortcut, &QShortcut::activated, mainWindow, [=]() mutable
    {
        if(locationBar->hasFocus())
        {
            navigateToPath(locationBar->text(), true);
        }
        else
        {
            QList<QTableWidgetItem*> selected = tableWidget->selectedItems();
            if(!selected.isEmpty()) handleItemActivation(tableWidget->row(selected.first()));
        }
    });

    auto *numpadEnterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), mainWindow);
    QObject::connect(numpadEnterShortcut, &QShortcut::activated, mainWindow, [=]() mutable
    {
        if(locationBar->hasFocus())
        {
            navigateToPath(locationBar->text(), true);
        }
        else
        {
            QList<QTableWidgetItem*> selected = tableWidget->selectedItems();
            if(!selected.isEmpty()) handleItemActivation(tableWidget->row(selected.first()));
        }
    });

    auto *deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), mainWindow);
    QObject::connect(deleteShortcut, &QShortcut::activated, mainWindow, [=]() mutable
    {
        QList<QTableWidgetItem*> selectedItems = tableWidget->selectedItems();
        QSet<int> selectedRows;
        for(auto *item : selectedItems) selectedRows.insert(tableWidget->row(item));
        if(selectedRows.isEmpty()) return;

        QStringList internalPathsToProcess;
        for(int row : selectedRows)
        {
            auto *nameItem = tableWidget->item(row, 0);
            if(!nameItem) continue;
            QString nameText = nameItem->data(Qt::UserRole).toString();
            if(nameText == "..") continue; // Skip navigation shortcuts
            
            QString targetInternalPath = *currentVirtualPath;
            if(!targetInternalPath.endsWith("/")) targetInternalPath += "/";
            targetInternalPath += nameText;
            internalPathsToProcess.append(targetInternalPath);
        }

        if(internalPathsToProcess.isEmpty()) return;

        auto confirm = QMessageBox::question(mainWindow, "Delete items", "Are you sure you want to delete selected items?", QMessageBox::Yes | QMessageBox::No);
        if(confirm == QMessageBox::Yes && backend->removeItems(internalPathsToProcess))
        {
            if(!backend->hasActiveArchive())
            {
                if(*setInterfaceStateActive) (*setInterfaceStateActive)(false);
                mainWindow->setWindowTitle("Archive Manager");
            }
            updateTableView(tableWidget, *backend, *currentVirtualPath, searchBar, statusBar, locationBar);
        }
    });

    QObject::connect(locationBar, &QLineEdit::returnPressed, mainWindow, [=]() mutable
    {
        navigateToPath(locationBar->text(), true);
    });

    // --- Batch File Extraction ---
    auto runBatchExtraction = [=](const QStringList& targets, bool fullArchive)
    {
        QString targetDir = QFileDialog::getExistingDirectory(mainWindow, "Select Extraction Path");
        if(targetDir.isEmpty()) return;

        size_t totalCount = fullArchive ? backend->getStats().totalFilesCount : targets.size();
        auto *progressDialog = new QProgressDialog("Extracting items...", "Cancel", 0, static_cast<int>(totalCount), mainWindow);
        progressDialog->setWindowModality(Qt::WindowModal);
        progressDialog->show();

        auto cb = [progressDialog](size_t currentIdx, const QString& currentFileName) -> bool
        {
            if(progressDialog->wasCanceled()) return false;
            progressDialog->setValue(static_cast<int>(currentIdx));
            progressDialog->setLabelText(QString("Extracting: %1").arg(currentFileName));
            QCoreApplication::processEvents();
            return true;
        };

        bool ok = false;
        if(fullArchive)
        {
            ok = backend->extractAll(targetDir, cb);
        }
        else
        {
            size_t idx = 0;
            ok = true;
            for(const auto& path : targets)
            {
                if (!cb(idx++, path)) { ok = false; break; }
                QString dest = targetDir + "/" + QFileInfo(path).fileName();
                backend->extractFile(path, dest);
            }
        }

        progressDialog->close();
        delete progressDialog;

        if(ok) QMessageBox::information(mainWindow, "Success", "Extraction completed successfully.");
    };

    // --- Extract Actions ---
    QObject::connect(extractAllMenuAction, &QAction::triggered, mainWindow, [=]()
    {
        runBatchExtraction({}, true);
    });

    QObject::connect(extractSelectedMenuAction, &QAction::triggered, mainWindow, [=]() mutable
    {
        QList<QTableWidgetItem*> selectedItems = tableWidget->selectedItems();
        QSet<int> selectedRows;
        for(auto *item : selectedItems) selectedRows.insert(tableWidget->row(item));

        if(selectedRows.isEmpty()) return;

        QStringList internalPathsToProcess;
        for(int row : selectedRows)
        {
            auto *nameItem = tableWidget->item(row, 0);
            if(!nameItem) continue;
            QString nameText = nameItem->data(Qt::UserRole).toString();
            if(nameText == "..") continue; // Skip the parent directory

            QString targetInternalPath = *currentVirtualPath;
            if(!targetInternalPath.endsWith("/")) targetInternalPath += "/";
            targetInternalPath += nameText;
            internalPathsToProcess.append(targetInternalPath);
        }

        if(!internalPathsToProcess.isEmpty())
        {
            runBatchExtraction(internalPathsToProcess, false);
        }
    });

    // --- Table Widget Connections ---
    QObject::connect(tableWidget, &QTableWidget::cellDoubleClicked, mainWindow, [handleItemActivation](int row, int col) mutable
    {
        Q_UNUSED(col);
        handleItemActivation(row);
    });

    QObject::connect(tableWidget, &QTableWidget::itemSelectionChanged, mainWindow, [=]()
    {
        updateStatusBarText(tableWidget, statusBar);
        QList<QTableWidgetItem*> currentSel = tableWidget->selectedItems();
        extractSelectedMenuAction->setEnabled(!currentSel.isEmpty());
    });

    QObject::connect(tableWidget, &QTableWidget::customContextMenuRequested, mainWindow, [=](const QPoint& pos) mutable
    {
        QList<QTableWidgetItem*> selectedItems = tableWidget->selectedItems();
        QSet<int> selectedRows;
        for(auto *item : selectedItems) selectedRows.insert(tableWidget->row(item));

        if(selectedRows.isEmpty()) return;

        // Strip the navigation index out of selections
        bool containsParentLink = false;
        QStringList internalPathsToProcess;
        for(int row : selectedRows)
        {
            auto *nameItem = tableWidget->item(row, 0);
            if(!nameItem) continue;
            QString nameText = nameItem->data(Qt::UserRole).toString();
            if(nameText == "..") { containsParentLink = true; continue; }
            
            QString targetInternalPath = *currentVirtualPath;
            if(!targetInternalPath.endsWith("/")) targetInternalPath += "/";
            targetInternalPath += nameText;
            internalPathsToProcess.append(targetInternalPath);
        }

        if(internalPathsToProcess.isEmpty() && containsParentLink) return;

        QMenu contextMenu(tableWidget);
        
        QAction *openAct = nullptr;
        if(internalPathsToProcess.size() == 1)
        {
            openAct = contextMenu.addAction("Open");
            contextMenu.addSeparator();
        }

        QString extractLabel = (internalPathsToProcess.size() > 1) ? "Extract Files..." : "Extract File...";
        QAction *extractAct = contextMenu.addAction(extractLabel);
        
        QAction *deleteAct = contextMenu.addAction("Delete from Archive");

        QAction *selectedAction = contextMenu.exec(tableWidget->viewport()->mapToGlobal(pos));
        if(!selectedAction) return;

        if(selectedAction == openAct)
        {
            int firstRow = *selectedRows.begin();
            handleItemActivation(firstRow);
        }
        else if(selectedAction == extractAct)
        {
            runBatchExtraction(internalPathsToProcess, false);
        }
        else if(selectedAction == deleteAct)
        {
            auto confirm = QMessageBox::question(mainWindow, "Delete items", "Are you sure you want to delete selected items?", QMessageBox::Yes | QMessageBox::No);
            if(confirm == QMessageBox::Yes && backend->removeItems(internalPathsToProcess))
            {
                // If the delete operation empties out the archive state entirely, disable actions
                if(!backend->hasActiveArchive())
                {
                    if(*setInterfaceStateActive) (*setInterfaceStateActive)(false);
                    mainWindow->setWindowTitle("Archive Manager");
                }
                updateTableView(tableWidget, *backend, *currentVirtualPath, searchBar, statusBar, locationBar);
            }
        }
    });

    // --- Visibility State Modifier Helper ---
    *setInterfaceStateActive = [=](bool active)
    {
        searchBar->setEnabled(active);
        locationBar->setEnabled(active);

        // Dynamic history button updates
        backBtn->setEnabled(active && !historyBackwardStack->empty());
        forwardBtn->setEnabled(active && !historyForwardStack->empty());
        upBtn->setEnabled(active && *currentVirtualPath != "/");
        homeBtn->setEnabled(active && *currentVirtualPath != "/");

        // Sync top Navigate Menu options status
        navBackAction->setEnabled(active && !historyBackwardStack->empty());
        navForwardAction->setEnabled(active && !historyForwardStack->empty());
        navUpAction->setEnabled(active && *currentVirtualPath != "/");
        navHomeAction->setEnabled(active && *currentVirtualPath != "/");

        extractSelectedMenuAction->setEnabled(false); // Default to disabled until rows are highlighted
        extractAllMenuAction->setEnabled(active);
        addAction->setEnabled(active);
        addFolderAction->setEnabled(active);
        propsAction->setEnabled(active);
    };

    // Default layout: Disable actions until a file is opened
    (*setInterfaceStateActive)(false);

    // --- Clean up /tmp subdirectory on exit ---
    QObject::connect(&app, &QApplication::aboutToQuit, mainWindow, [sessionSandboxPath]()
    {
        QDir sandboxDir(sessionSandboxPath);
        if(sandboxDir.exists())
        {
            sandboxDir.removeRecursively();
        }
    });

    // --- Show Main Window ---
    mainWindow->setWindowTitle("Archive Manager");
    mainWindow->setWindowIcon(QIcon(APPICON_PATH));
    mainWindow->resize(450, 350);
    mainWindow->show();

    // --- Parse Commandline ---
    QCommandLineParser parser;
    parser.setApplicationDescription("Archive Manager.");
    parser.addHelpOption();
    parser.addPositionalArgument("archive", "Archive path to load.");
    parser.process(app);

    if(!parser.positionalArguments().isEmpty())
    {
        handleLoadFile(parser.positionalArguments().first());
    }

    return app.exec();
}

