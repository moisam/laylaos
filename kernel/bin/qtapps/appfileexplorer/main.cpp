#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QTreeView>
#include <QListView>
#include <QFileSystemModel>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QFileInfo>
#include <QIcon>
#include <QStyle>
#include <QHash>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QLineEdit>
#include <QDesktopServices>
#include <QUrl>
#include <QItemSelectionModel>
#include <QToolBar>
#include <QHBoxLayout>
#include <QWidget>
#include <QToolButton>
#include <QList>
#include <QStackedWidget>
#include <QListWidget>
#include <QHeaderView>
#include <QDirIterator>
#include <QStandardPaths>

#define SYSTEM_ICON_PATH        "/usr/share/gui/icons"
#define APPICON_PATH            "/usr/share/gui/icons/folder.png"

// global icon cache
static QHash<QString, QIcon> g_iconCache;

// specific icons used for folders, unknown file types, root filesystem, ...
static QIcon cachedFolderIcon;
static QIcon cachedHomeIcon;
static QIcon cachedDocsIcon;
static QIcon cachedDownloadsIcon;
static QIcon cachedDiskIcon;
static QIcon cachedFileIcon;

static bool hasFileIcon = false;
static bool hasFolderIcon = false;
static bool hasHomeIcon = false;
static bool hasDocsIcon = false;
static bool hasDownloadsIcon = false;
static bool hasDiskIcon = false;


class CustomFileSystemModel : public QFileSystemModel
{
public:
    using QFileSystemModel::QFileSystemModel;

    QVariant data(const QModelIndex &index, int role) const override
    {
        if(role == Qt::DecorationRole && index.column() == 0)
        {
            QModelIndex nameIndex = index.model()->index(index.row(), 0, index.parent());
            QString path = filePath(nameIndex);
            QFileInfo fileInfo(path);

            if(fileInfo.isDir() && hasFolderIcon)
            {
                return cachedFolderIcon;
            }
            else if(fileInfo.isFile())
            {
                QString ext = fileInfo.suffix().toLower();

                if(g_iconCache.contains(ext))
                {
                    return g_iconCache.value(ext);
                }
                else if(fileInfo.isExecutable())
                {
                    return g_iconCache.value("exe");
                }
                else if(hasFileIcon)
                {
                    return cachedFileIcon;
                }
            }
        }
        
        return QFileSystemModel::data(index, role);
    }
};


class FileExplorer : public QMainWindow
{
public:
    FileExplorer();

private:
    void addBookmarkItem(const QString &displayName, const QString &targetPath, const QIcon &icon);
    void syncListViewSorting(int logicalIndex, Qt::SortOrder order);
    void updateStatusSelection();
    void showItemCountInStatusbar();

    void onBookmarkClicked(QListWidgetItem *item);
    void onDirectoryChanged(const QModelIndex &index);
    void onItemDoubleClicked(const QModelIndex &index);

    void setupBookmarks();
    void setupTopToolBar();
    void setupMenus();
    void updateLocationUi(const QString &path);
    void updateNavigationButtonStates();

    void handleLocationBarNavigation();
    void handleOpenLocation();
    void handleNewFolder();
    void handlePaste();
    void handleCopySelection();
    void handleDeleteSelection();
    void handleItemContextMenu(const QPoint &pos);
    void handleHeaderContextMenu(const QPoint &pos);
    void handleGridViewSelected();
    void handleListViewSelected();

    void handleBackNavigation();
    void handleForwardNavigation();
    void handleUpNavigation();

    bool copyDirectoryRecursively(const QString &src, const QString &dst);
    void navigateTo(const QString &newPath, bool isHistoryNavigation = false);

    void showShortcutsDialog();
    void showAboutDialog();

    CustomFileSystemModel *fileModel;
    QListWidget *leftBookmarkList;
    QStackedWidget *rightStackedWidget;
    QListView *rightListView;
    QTreeView *rightTreeView;
    QLineEdit *locationLineEdit;
    QToolButton *backButton;
    QToolButton *forwardButton;
    QToolButton *upButton;
    QString m_currentPath;
    QList<QString> m_backHistory;
    QList<QString> m_forwardHistory;
    QString m_copiedPath;
};


/********************************
 *
 * Constructor
 *
 ********************************/

FileExplorer::FileExplorer()
{
    setWindowTitle("File Explorer");
    resize(620, 400);
    setWindowIcon(QIcon(APPICON_PATH));

    fileModel = new CustomFileSystemModel(this);
    fileModel->setReadOnly(false); 
    fileModel->setRootPath(QDir::rootPath());

    m_currentPath = QDir::homePath();
    setWindowTitle(m_currentPath);

    setupTopToolBar();

    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);

    leftBookmarkList = new QListWidget(splitter);
    leftBookmarkList->setIconSize(QSize(24, 24));
    setupBookmarks();

    connect(leftBookmarkList, &QListWidget::itemClicked, this, &FileExplorer::onBookmarkClicked);

    rightStackedWidget = new QStackedWidget(splitter);

    // Sub-view A: Grid View mode
    rightListView = new QListView(rightStackedWidget);
    rightListView->setModel(fileModel);
    rightListView->setRootIndex(fileModel->index(m_currentPath));
    rightListView->setViewMode(QListView::IconMode);
    rightListView->setIconSize(QSize(64, 64));
    rightListView->setGridSize(QSize(95, 95));
    rightListView->setResizeMode(QListView::Adjust);

    // Sub-view B: Detailed Table view mode
    rightTreeView = new QTreeView(rightStackedWidget);
    rightTreeView->setModel(fileModel);
    rightTreeView->setRootIndex(fileModel->index(m_currentPath));
    rightTreeView->setAlternatingRowColors(true);
    rightTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);

    QHeaderView *header = rightTreeView->header();
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    header->setSectionsMovable(true);                    
    header->setStretchLastSection(true);                  

    rightTreeView->setSortingEnabled(true); 
    rightTreeView->sortByColumn(0, Qt::AscendingOrder);

    // Allow multi-selection on both panels (Control / Shift clicking)
    rightListView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rightTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(header, &QHeaderView::sortIndicatorChanged, this, &FileExplorer::syncListViewSorting);
    connect(header, &QHeaderView::customContextMenuRequested, this, &FileExplorer::handleHeaderContextMenu);

    // Enable Right-Click Menus for both right-pane views
    rightListView->setContextMenuPolicy(Qt::CustomContextMenu);
    rightTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

    // Connect selection changes to update the status bar
    connect(rightListView->selectionModel(), &QItemSelectionModel::selectionChanged, 
            this, &FileExplorer::updateStatusSelection);
    connect(rightTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, 
            this, &FileExplorer::updateStatusSelection);

    // Also update status bar when the user changes layout view modes
    connect(rightStackedWidget, &QStackedWidget::currentChanged, 
            this, &FileExplorer::updateStatusSelection);

    connect(rightListView, &QListView::customContextMenuRequested, 
            this, &FileExplorer::handleItemContextMenu);
    connect(rightTreeView, &QTreeView::customContextMenuRequested, 
            this, &FileExplorer::handleItemContextMenu);

    rightStackedWidget->addWidget(rightListView); 
    rightStackedWidget->addWidget(rightTreeView); 

    connect(rightListView, &QListView::doubleClicked, this, &FileExplorer::onItemDoubleClicked);
    connect(rightTreeView, &QTreeView::doubleClicked, this, &FileExplorer::onItemDoubleClicked);

    splitter->setSizes(QList<int>() << 120 << 500);

    setupMenus();
        
    updateLocationUi(m_currentPath);
    updateNavigationButtonStates();

    rightTreeView->setColumnWidth(0, 300); 
    rightTreeView->setColumnWidth(1, 50); 
    rightTreeView->setColumnWidth(2, 50); 
    rightTreeView->setColumnWidth(3, 100); 
}


/********************************
 *
 * Handler functions
 *
 ********************************/

void FileExplorer::onBookmarkClicked(QListWidgetItem *item)
{
    if(item != nullptr)
    {
        QString path = item->data(Qt::UserRole).toString();
        navigateTo(path);
    }
}

void FileExplorer::onDirectoryChanged(const QModelIndex &index)
{
    if(fileModel->isDir(index)) navigateTo(fileModel->filePath(index));
}

void FileExplorer::onItemDoubleClicked(const QModelIndex &index)
{
    QModelIndex mainIndex = fileModel->index(index.row(), 0, index.parent());
    QString path = fileModel->filePath(mainIndex);

    if(fileModel->isDir(mainIndex))
    {
        navigateTo(path);
    }
    else
    {
        bool success = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        if(!success)
        {
            QMessageBox::warning(this, "Open Error", QString("Could not open the file:\n%1").arg(fileModel->fileName(mainIndex)));
        }
    }
}

void FileExplorer::handleGridViewSelected()
{
    rightStackedWidget->setCurrentIndex(0);
}

void FileExplorer::handleListViewSelected()
{
    rightStackedWidget->setCurrentIndex(1);
}

void FileExplorer::handleBackNavigation()
{
    if(!m_backHistory.isEmpty())
    {
        QString p = m_backHistory.takeLast();
        m_forwardHistory.append(m_currentPath);
        navigateTo(p, true);
    }
}

void FileExplorer::handleForwardNavigation()
{
    if(!m_forwardHistory.isEmpty())
    {
        QString p = m_forwardHistory.takeLast();
        m_backHistory.append(m_currentPath);
        navigateTo(p, true);
    }
}

void FileExplorer::handleUpNavigation()
{
    QDir d(m_currentPath);
    if(!d.isRoot() && d.cdUp()) navigateTo(d.absolutePath());
}

void FileExplorer::handleLocationBarNavigation()
{
    QString enteredPath = locationLineEdit->text().trimmed();
    if(QDir(enteredPath).exists()) 
        navigateTo(QDir(enteredPath).absolutePath());
    else
    {
        QMessageBox::warning(this, "Invalid Path", "The specified path does not exist."); 
        updateLocationUi(m_currentPath);
    }
}

void FileExplorer::handleOpenLocation()
{
    QString selectedDir = QFileDialog::getExistingDirectory(this, "Open Directory Location", m_currentPath);
    if(!selectedDir.isEmpty()) navigateTo(selectedDir);
}

void FileExplorer::handleNewFolder() {
    QModelIndex currentIndex = (rightStackedWidget->currentIndex() == 0) ? 
        rightListView->rootIndex() : rightTreeView->rootIndex();

    QInputDialog dialog(this);
    dialog.setWindowTitle("Create New Folder");
    dialog.setLabelText("Folder Name:");
    dialog.setTextValue("New Folder");

    QLineEdit *lineEdit = dialog.findChild<QLineEdit *>();
    QRegularExpression regex("^[^\\/:?\"<>|]$");
    QRegularExpressionValidator validator(regex, &dialog);
    if(lineEdit != nullptr) lineEdit->setValidator(&validator);

    if(dialog.exec() == QDialog::Accepted)
    {
        QString name = dialog.textValue().trimmed();
        if(!name.isEmpty() && !QDir(fileModel->filePath(currentIndex)).exists(name))
        {
            fileModel->mkdir(currentIndex, name);
        }
    }
}

void FileExplorer::handleItemContextMenu(const QPoint &pos)
{
    QAbstractItemView *activeView = qobject_cast<QAbstractItemView*>(sender());
    if(!activeView) return;

    QModelIndex clickedIndex = activeView->indexAt(pos);
    QMenu contextMenu(this);

    if(clickedIndex.isValid())
    {
        // An item was directly clicked (File or Folder actions)
        QModelIndex mainIndex = fileModel->index(clickedIndex.row(), 0, clickedIndex.parent());
        QString name = fileModel->fileName(mainIndex);

        QAction *openAction = contextMenu.addAction(QString("&Open '%1'").arg(name));
        contextMenu.addSeparator();
        QAction *copyAction = contextMenu.addAction("&Copy");
        QAction *deleteAction = contextMenu.addAction("&Delete");
        QAction *selectedAction = contextMenu.exec(activeView->mapToGlobal(pos));

        if(selectedAction == openAction) onItemDoubleClicked(mainIndex);
        else if(selectedAction == copyAction) handleCopySelection();
        else if(selectedAction == deleteAction) handleDeleteSelection();
    }
    else
    {
        // Empty space was clicked (Directory backdrop actions)
        QAction *newFolderAction = contextMenu.addAction("&New Folder...");
        QAction *pasteAction = contextMenu.addAction("&Paste");

        pasteAction->setEnabled(!m_copiedPath.isEmpty());

        QAction *selectedAction = contextMenu.exec(activeView->mapToGlobal(pos));

        if(selectedAction == newFolderAction) handleNewFolder();
        else if(selectedAction == pasteAction) handlePaste();
    }
}

void FileExplorer::handleHeaderContextMenu(const QPoint &pos)
{
    QHeaderView *header = rightTreeView->header();
    QMenu contextMenu(this);

    QAction *toggleSize = contextMenu.addAction("Show Size");
    toggleSize->setCheckable(true);
    toggleSize->setChecked(!header->isSectionHidden(1));

    QAction *toggleType = contextMenu.addAction("Show Type");
    toggleType->setCheckable(true);
    toggleType->setChecked(!header->isSectionHidden(2));

    QAction *toggleDate = contextMenu.addAction("Show Date Modified");
    toggleDate->setCheckable(true);
    toggleDate->setChecked(!header->isSectionHidden(3));

    QAction *selectedAction = contextMenu.exec(header->mapToGlobal(pos));
    if(selectedAction == toggleSize) header->setSectionHidden(1, !header->isSectionHidden(1));
    if(selectedAction == toggleType) header->setSectionHidden(2, !header->isSectionHidden(2));
    if(selectedAction == toggleDate) header->setSectionHidden(3, !header->isSectionHidden(3));
}

void FileExplorer::handlePaste()
{
    if(m_copiedPath.isEmpty()) return;
    QFileInfo srcInfo(m_copiedPath);
    QString destPath = fileModel->filePath((rightStackedWidget->currentIndex() == 0) ? 
        rightListView->rootIndex() : rightTreeView->rootIndex()) + QDir::separator() + srcInfo.fileName();

    if(m_copiedPath == destPath) destPath += "_copy";
    bool success = srcInfo.isDir() ? copyDirectoryRecursively(m_copiedPath, destPath) : QFile::copy(m_copiedPath, destPath);

    if(success) statusBar()->showMessage("Pasted successfully.", 3000);
}

void FileExplorer::handleCopySelection()
{
    QAbstractItemView *activeView = (rightStackedWidget->currentIndex() == 0) ? 
        static_cast<QAbstractItemView*>(rightListView) : 
        static_cast<QAbstractItemView*>(rightTreeView);
    QModelIndexList selectedIndexes = activeView->selectionModel()->selectedIndexes();

    if(!selectedIndexes.isEmpty())
    {
        QModelIndex targetIndex = fileModel->index(selectedIndexes.first().row(), 0, selectedIndexes.first().parent());
        m_copiedPath = fileModel->filePath(targetIndex);
        statusBar()->showMessage(QString("Copied: %1").arg(fileModel->fileName(targetIndex)), 3000);
    }
}

void FileExplorer::handleDeleteSelection()
{
    QAbstractItemView *activeView = (rightStackedWidget->currentIndex() == 0) ? 
            static_cast<QAbstractItemView*>(rightListView) : 
            static_cast<QAbstractItemView*>(rightTreeView);
    QModelIndexList selectedIndexes = activeView->selectionModel()->selectedIndexes();

    if(selectedIndexes.isEmpty())
    {
        QMessageBox::information(this, "Delete", "Please select an item to delete first.");
        return;
    }

    QModelIndex targetIndex = fileModel->index(selectedIndexes.first().row(), 0, selectedIndexes.first().parent());
    QString itemName = fileModel->fileName(targetIndex);
    bool isDir = fileModel->isDir(targetIndex);

    if(QMessageBox::question(this, "Confirm Deletion", QString("Permanently delete '%1'?").arg(itemName), QMessageBox::Yes|QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    bool success = isDir ? fileModel->rmdir(targetIndex) : fileModel->remove(targetIndex);
    if(!success) QMessageBox::critical(this, "Error", QString("Could not delete %1.").arg(itemName));
}


/********************************
 *
 * Dialog boxes
 *
 ********************************/

void FileExplorer::showShortcutsDialog()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Keyboard Shortcuts");
    msgBox.setTextFormat(Qt::MarkdownText);
    
    msgBox.setText(
        "| Shortcut | Action |\n"
        "| :--- | :--- |\n"
        "| **Ctrl + C** | Copy Selected Item |\n"
        "| **Ctrl + V** | Paste Copied Item |\n"
        "| **Delete** | Delete Selected Item |\n"
        "| **Enter** | Navigate Path / Open File |\n"
        "| **Alt + Left** | Go Back in History |\n"
        "| **Alt + Right**| Go Forward in History |\n"
        "| **Alt + Up**   | Go Up to Parent Folder |\n"
    );
    
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

void FileExplorer::showAboutDialog()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About File Explorer");
    msgBox.setTextFormat(Qt::MarkdownText);

    msgBox.setText(
        "A highly efficient multi-view desktop environment utility built using **Qt 6**.\n\n"
        "Tailored to run seamlessly across traditional GNU/Linux host environments, LaylaOS, and "
        "custom bare-metal hobby operating system platforms."
    );

    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}


/********************************
 *
 * Utility functions
 *
 ********************************/

bool FileExplorer::copyDirectoryRecursively(const QString &src, const QString &dst)
{
    QDir sDir(src);
    QDir dDir(dst);
    if(!sDir.exists() || (!dDir.exists() && !dDir.mkpath(dst))) return false;

    for(const QString &entry : sDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QString sItem = src + QDir::separator() + entry;
        QString dItem = dst + QDir::separator() + entry;

        if(QFileInfo(sItem).isDir())
        {
            if(!copyDirectoryRecursively(sItem, dItem)) return false;
        }
        else
        {
            if(QFile::exists(dItem)) QFile::remove(dItem);
            if(!QFile::copy(sItem, dItem)) return false;
        }
    }

    return true;
}

void FileExplorer::syncListViewSorting(int logicalIndex, Qt::SortOrder order)
{
    fileModel->sort(logicalIndex, order);
}

void FileExplorer::addBookmarkItem(const QString &displayName, const QString &targetPath, const QIcon &icon) 
{
    if(targetPath.isEmpty() || !QDir(targetPath).exists()) return;

    QListWidgetItem *item = new QListWidgetItem(icon, displayName, leftBookmarkList);
    // Store hidden string data directly inside the item structure using Qt::UserRole
    item->setData(Qt::UserRole, targetPath);
}

void FileExplorer::navigateTo(const QString &newPath, bool isHistoryNavigation)
{
    QDir dir(newPath);
    if(!dir.exists()) return;

    QString cleanNewPath = QDir::cleanPath(newPath);
    if(cleanNewPath == m_currentPath) return;

    if(!isHistoryNavigation)
    {
        m_backHistory.append(m_currentPath);
        m_forwardHistory.clear();
    }

    m_currentPath = cleanNewPath;

    QModelIndex targetIndex = fileModel->index(m_currentPath);
    if(targetIndex.isValid())
    {
        rightListView->setRootIndex(targetIndex);
        rightTreeView->setRootIndex(targetIndex);
    }

    setWindowTitle(m_currentPath);

    updateLocationUi(m_currentPath);
    updateNavigationButtonStates();
}

void FileExplorer::updateStatusSelection()
{
    QAbstractItemView *activeView = (rightStackedWidget->currentIndex() == 0) 
        ? static_cast<QAbstractItemView*>(rightListView) 
        : static_cast<QAbstractItemView*>(rightTreeView);

    QModelIndexList selectedIndexes = activeView->selectionModel()->selectedIndexes();
    
    // QTreeView returns an index for EVERY column in a selected row. 
    // We filter to only count unique rows so multi-column selection counts accurately.
    QList<int> uniqueRows;
    for(const QModelIndex &idx : selectedIndexes)
    {
        if(!uniqueRows.contains(idx.row()))
        {
            uniqueRows.append(idx.row());
        }
    }

    int selectionCount = uniqueRows.size();

    if(selectionCount == 0)
    {
        showItemCountInStatusbar();
    }
    else if(selectionCount == 1)
    {
        // Exactly one item selected -> find its primary column index
        QModelIndex targetIdx = selectedIndexes.first();
        QModelIndex nameIndex = fileModel->index(targetIdx.row(), 0, targetIdx.parent());
        QString fileName = fileModel->fileName(nameIndex);
        statusBar()->showMessage(fileName);
    }
    else
    {
        statusBar()->showMessage(QString("%1 items selected").arg(selectionCount));
    }
}

/********************************
 *
 * UI setup functions
 *
 ********************************/

void FileExplorer::showItemCountInStatusbar()
{
    int folderCount = 0;
    int fileCount = 0;

    QDirIterator it(m_currentPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);

    while(it.hasNext())
    {
        it.next();
        if(it.fileInfo().isDir())
        {
            folderCount++;
        }
        else
        {
            fileCount++;
        }
    }

    QString statusText = QString("Total of %1 Folder(s), %2 File(s)")
                            .arg(folderCount)
                            .arg(fileCount);

    statusBar()->showMessage(statusText);
}

void FileExplorer::updateLocationUi(const QString &path)
{
    locationLineEdit->setText(QDir::toNativeSeparators(path));
    showItemCountInStatusbar();
}

void FileExplorer::updateNavigationButtonStates()
{
    backButton->setEnabled(!m_backHistory.isEmpty());
    forwardButton->setEnabled(!m_forwardHistory.isEmpty());
    upButton->setEnabled(!QDir(m_currentPath).isRoot());
}

void FileExplorer::setupBookmarks()
{
    QIcon folderIcon = hasFolderIcon ? cachedFolderIcon : style()->standardIcon(QStyle::SP_DirIcon);
    QIcon homeIcon = hasHomeIcon ? cachedHomeIcon : style()->standardIcon(QStyle::SP_DirIcon);
    QIcon docsIcon = hasDocsIcon ? cachedDocsIcon : style()->standardIcon(QStyle::SP_DirIcon);
    QIcon downloadsIcon = hasDownloadsIcon ? cachedDownloadsIcon : style()->standardIcon(QStyle::SP_DirIcon);
    QIcon rootIcon = hasDiskIcon ? cachedDiskIcon : style()->standardIcon(QStyle::SP_DriveHDIcon);

    addBookmarkItem("Home", QDir::homePath(), homeIcon);
    addBookmarkItem("Documents", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), docsIcon);
    addBookmarkItem("Downloads", QStandardPaths::writableLocation(QStandardPaths::DownloadLocation), downloadsIcon);
    addBookmarkItem("Desktop", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation), folderIcon);
    addBookmarkItem("Root", QDir::rootPath(), rootIcon);
}

void FileExplorer::setupMenus()
{
    QMenuBar *menu = menuBar();
    QMenu *fileMenu = menu->addMenu("&File");

    QAction *openLocAction = fileMenu->addAction("&Open Location...");
    connect(openLocAction, &QAction::triggered, this, &FileExplorer::handleOpenLocation);

    QAction *newFolderAction = fileMenu->addAction("&New Folder...");
    connect(newFolderAction, &QAction::triggered, this, &FileExplorer::handleNewFolder);

    fileMenu->addSeparator();
    fileMenu->addAction("&Exit", qApp, &QApplication::quit);

    QMenu *editMenu = menu->addMenu("&Edit");
    QAction *copyAction = editMenu->addAction("&Copy");
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &FileExplorer::handleCopySelection);

    QAction *pasteAction = editMenu->addAction("&Paste");
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, &FileExplorer::handlePaste);

    QAction *deleteAction = editMenu->addAction("&Delete");
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &FileExplorer::handleDeleteSelection);

    QMenu *viewMenu = menu->addMenu("&View");
    QAction *gridAction = viewMenu->addAction("&Grid View Mode");
    connect(gridAction, &QAction::triggered, this, &FileExplorer::handleGridViewSelected);

    QAction *listAction = viewMenu->addAction("&Detailed List View Mode");
    connect(listAction, &QAction::triggered, this, &FileExplorer::handleListViewSelected);

    QMenu *helpMenu = menu->addMenu("&Help");
    QAction *shortcutsAction = helpMenu->addAction("&Shortcuts");
    shortcutsAction->setShortcut(QKeySequence("F1"));
    connect(shortcutsAction, &QAction::triggered, this, &FileExplorer::showShortcutsDialog);
    
    helpMenu->addSeparator();
    
    QAction *aboutAction = helpMenu->addAction("&About Explorer");
    connect(aboutAction, &QAction::triggered, this, &FileExplorer::showAboutDialog);
}

void FileExplorer::setupTopToolBar()
{
    QToolBar *topBar = addToolBar("Navigation");
    topBar->setMovable(false);
    topBar->setFloatable(false);

    QWidget *container = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(container);
    layout->setContentsMargins(5, 2, 5, 2);
    layout->setSpacing(5);

    QWidget *leftNavWidget = new QWidget(this);
    QHBoxLayout *leftNavLayout = new QHBoxLayout(leftNavWidget);
    leftNavLayout->setContentsMargins(0, 0, 0, 0);
    leftNavLayout->setSpacing(2);

    backButton = new QToolButton(this);
    backButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    backButton->setToolTip("Back");
    backButton->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Left));
    connect(backButton, &QToolButton::clicked, this, &FileExplorer::handleBackNavigation);

    forwardButton = new QToolButton(this);
    forwardButton->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    forwardButton->setToolTip("Forward");
    forwardButton->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Right));
    connect(forwardButton, &QToolButton::clicked, this, &FileExplorer::handleForwardNavigation);

    upButton = new QToolButton(this);
    upButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    upButton->setToolTip("Up to parent directory");
    upButton->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
    connect(upButton, &QToolButton::clicked, this, &FileExplorer::handleUpNavigation);

    leftNavLayout->addWidget(backButton);
    leftNavLayout->addWidget(forwardButton);
    leftNavLayout->addWidget(upButton);
    layout->addWidget(leftNavWidget);

    locationLineEdit = new QLineEdit(this);
    layout->addWidget(locationLineEdit, 1);
    connect(locationLineEdit, &QLineEdit::returnPressed, this, &FileExplorer::handleLocationBarNavigation);

    QWidget *rightNavWidget = new QWidget(this);
    QHBoxLayout *rightNavLayout = new QHBoxLayout(rightNavWidget);
    rightNavLayout->setContentsMargins(0, 0, 0, 0);
    rightNavLayout->setSpacing(2);

    QToolButton *gridViewButton = new QToolButton(this);
    gridViewButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    gridViewButton->setToolTip("Grid View Mode");

    connect(gridViewButton, &QToolButton::clicked, this, [this]()
    {
        rightStackedWidget->setCurrentIndex(0);
    });

    QToolButton *detailedViewButton = new QToolButton(this);
    detailedViewButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    detailedViewButton->setToolTip("Detailed List View Mode");

    connect(detailedViewButton, &QToolButton::clicked, this, [this]()
    {
        rightStackedWidget->setCurrentIndex(1);
    });

    rightNavLayout->addWidget(gridViewButton);
    rightNavLayout->addWidget(detailedViewButton);
    layout->addWidget(rightNavWidget);

    container->setLayout(layout);
    topBar->addWidget(container);
}


/********************************
 *
 * Global functions
 *
 ********************************/

void loadCustomIconsFromDisk()
{
    QDir iconDir(QCoreApplication::applicationDirPath() + "/icons");
    //qDebug() << "dirpath " << QCoreApplication::applicationDirPath();

    if(!iconDir.exists())
        iconDir = QDir(SYSTEM_ICON_PATH);

    if(!iconDir.exists()) return;

    QStringList iconFiles = iconDir.entryList(QStringList() << "*.png", QDir::Files);

    for(const QString &fileName : iconFiles)
    {
        QString basename = QFileInfo(fileName).baseName().toLower();
        //qDebug() << "Found: " << fileName;

        if(basename == "apk")
            g_iconCache.insert("apk", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "archive")
        {
            g_iconCache.insert("zip", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("gzip", g_iconCache.value("zip"));
            g_iconCache.insert("7z", g_iconCache.value("zip"));
            g_iconCache.insert("tar", g_iconCache.value("zip"));
            g_iconCache.insert("gz", g_iconCache.value("zip"));
            g_iconCache.insert("xz", g_iconCache.value("zip"));
        }
        else if(basename == "audio")
        {
            g_iconCache.insert("mp3", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("wav", g_iconCache.value("mp3"));
            g_iconCache.insert("flac", g_iconCache.value("mp3"));
        }
        else if(basename == "code-chdr")
        {
            g_iconCache.insert("h", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("hpp", g_iconCache.value("h"));
        }
        else if(basename == "code-csrc")
            g_iconCache.insert("c", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "code-cppsrc")
            g_iconCache.insert("cpp", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "code-sh")
            g_iconCache.insert("sh", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "css")
            g_iconCache.insert("css", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "database")
        {
            g_iconCache.insert("db", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("sql", g_iconCache.value("db"));
            g_iconCache.insert("sqlite", g_iconCache.value("db"));
        }
        else if(basename == "executable")
            g_iconCache.insert("exe", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "html")
        {
            g_iconCache.insert("html", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("htm", g_iconCache.value("html"));
        }
        else if(basename == "iso")
            g_iconCache.insert("iso", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "image")
        {
            g_iconCache.insert("jpg", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("jpeg", g_iconCache.value("jpg"));
            g_iconCache.insert("png", g_iconCache.value("jpg"));
            g_iconCache.insert("tiff", g_iconCache.value("jpg"));
            g_iconCache.insert("png", g_iconCache.value("jpg"));
            g_iconCache.insert("ico", g_iconCache.value("jpg"));
        }
        else if(basename == "java")
            g_iconCache.insert("java", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "javascript")
            g_iconCache.insert("js", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "pdf")
            g_iconCache.insert("pdf", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "php")
            g_iconCache.insert("php", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "package")
        {
            g_iconCache.insert("rpm", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("deb", g_iconCache.value("rpm"));
        }
        else if(basename == "presentation")
        {
            g_iconCache.insert("ppt", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("pptx", g_iconCache.value("ppt"));
            g_iconCache.insert("odp", g_iconCache.value("ppt"));
        }
        else if(basename == "textdoc")
        {
            g_iconCache.insert("doc", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("docx", g_iconCache.value("doc"));
            g_iconCache.insert("odt", g_iconCache.value("doc"));
        }
        else if(basename == "ttf")
        {
            g_iconCache.insert("ttf", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("otf", g_iconCache.value("ttf"));
        }
        else if(basename == "spreadsheet")
        {
            g_iconCache.insert("xls", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("xlsx", g_iconCache.value("xls"));
            g_iconCache.insert("ods", g_iconCache.value("xls"));
        }
        else if(basename == "video")
        {
            g_iconCache.insert("mp4", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("mpeg", g_iconCache.value("mp4"));
        }
        else if(basename == "xml")
            g_iconCache.insert("xml", QIcon(iconDir.absoluteFilePath(fileName)));
        else if(basename == "harddisk")
        {
            cachedDiskIcon = QIcon(iconDir.absoluteFilePath(fileName));
            hasDiskIcon = true;
        }
        else if(basename == "folder")
        {
            cachedFolderIcon = QIcon(iconDir.absoluteFilePath(fileName));
            hasFolderIcon = true;
        }
        else if(basename == "folderdocs")
        {
            cachedDocsIcon = QIcon(iconDir.absoluteFilePath(fileName));
            hasDocsIcon = true;
        }
        else if(basename == "folderdownloads")
        {
            cachedDownloadsIcon = QIcon(iconDir.absoluteFilePath(fileName));
            hasDownloadsIcon = true;
        }
        else if(basename == "folderhome")
        {
            cachedHomeIcon = QIcon(iconDir.absoluteFilePath(fileName));
            hasHomeIcon = true;
        }
        else if(basename == "file_generic")
        {
            cachedFileIcon = QIcon(iconDir.absoluteFilePath(fileName));
            hasFileIcon = true;
            g_iconCache.insert("txt", QIcon(iconDir.absoluteFilePath(fileName)));
            g_iconCache.insert("text", g_iconCache.value("txt"));
        }
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    loadCustomIconsFromDisk();
    FileExplorer window;

    window.show();

    return app.exec();
}

