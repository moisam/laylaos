#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QDataStream>
#include <QImage>
#include <QPixmap>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QStatusBar>
#include <QEvent>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <set>
#include "CursorViewer.hpp"

#define APPICON_PATH            "/usr/share/gui/icons/mouse.png"

#ifdef __laylaos__
#include <gui/cursor.h>
#else
#define CURSOR_NONE             0
#define CURSOR_NORMAL           1
#define CURSOR_WE               2
#define CURSOR_NS               3
#define CURSOR_NWSE             4
#define CURSOR_NESW             5
#define CURSOR_FLEUR            6
#define CURSOR_CROSSHAIR        7
#define CURSOR_WAITING          8
#define CURSOR_IBEAM            9
#define CURSOR_OPEN_HAND        10
#define CURSOR_X                11
#define CURSOR_E                12
#define CURSOR_N                13
#define CURSOR_NE               14
#define CURSOR_NW               15
#define CURSOR_S                16
#define CURSOR_SE               17
#define CURSOR_SW               18
#define CURSOR_W                19
#define CURSOR_UP               20
#define CURSOR_CLOSED_HAND      21
#define CURSOR_POINTING_HAND    22
#define CURSOR_HELP             23
#define CURSOR_FORBIDDEN        24
#define CURSOR_ARROW_WAITING    25
#define CURSOR_VIBEAM           26
#define CURSOR_COLRESIZE        27
#define CURSOR_ROWRESIZE        28
#define CURSOR_CELL             29
#define CURSOR_CONTEXT_MENU     30
#define CURSOR_ZOOMIN           31
#define CURSOR_ZOOMOUT          32
#define CURSOR_DND_LINK         33
#define CURSOR_DND_COPY         34
#define CURSOR_DND_MOVE         35
#define CURSOR_DND_NODROP       CURSOR_X
#endif      /* __laylaos__ */

/*
 * Standard Freedesktop cursor mapping to legacy X11 counterparts.
 * Cursor names are not very well documented. Below names are collated from
 * multiple sources:
 *
 * https://www.w3.org/TR/css-ui-3/#predefined-cursors
 * https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Properties/cursor
 * https://www.pixelbeat.org/programming/x_cursors/
 */
struct CursorMapping
{
    QString fdoName;
    QString x11Name;
    int curId;
};

const std::vector<CursorMapping> cursorMappings =
{
    // Standard Selection & Status Indicators
    {"crosshair",   "crosshair",            CURSOR_CROSSHAIR    },
    {"default",     "left_ptr",             CURSOR_NORMAL       },
    {"help",        "whats_this",           CURSOR_HELP         },
    {"not-allowed", "crossed_circle",       CURSOR_FORBIDDEN    },
    {"pointer",     "hand2",                CURSOR_POINTING_HAND},
    {"progress",    "left_ptr_watch",       CURSOR_ARROW_WAITING},
    {"text",        "xterm",                CURSOR_IBEAM        },
    {"vertical-text","xterm",               CURSOR_VIBEAM       },
    {"wait",        "watch",                CURSOR_WAITING      },
    {"up-arrow",    "sb_up_arrow",          CURSOR_UP           },

    // Drag & Drop
    {"dnd-link",    "alias",                CURSOR_DND_LINK     },
    {"dnd-copy",    "copy",                 CURSOR_DND_COPY     },
    {"grab",        "hand1",                CURSOR_OPEN_HAND    },
    {"grabbing",    "hand2",                CURSOR_CLOSED_HAND  },
    {"dnd-move",    "move",                 CURSOR_DND_MOVE     },

    // Window Resizing
    {"e-resize",    "right_side",           CURSOR_E            },
    {"ew-resize",   "h_double_arrow",       CURSOR_WE           },
    {"n-resize",    "top_side",             CURSOR_N            },
    {"ne-resize",   "top_right_corner",     CURSOR_NE           },
    {"nesw-resize", "fd_double_arrow",      CURSOR_NESW         },
    {"ns-resize",   "v_double_arrow",       CURSOR_NS           },
    {"nw-resize",   "top_left_corner",      CURSOR_NW           },
    {"nwse-resize", "bd_double_arrow",      CURSOR_NWSE         },
    {"s-resize",    "bottom_side",          CURSOR_S            },
    {"se-resize",   "bottom_right_corner",  CURSOR_SE           },
    {"sw-resize",   "bottom_left_corner",   CURSOR_SW           },
    {"w-resize",    "left_side",            CURSOR_W            },

    // UI Panel Splitting
    {"col-resize",  "sb_h_double_arrow",    CURSOR_COLRESIZE    },
    {"row-resize",  "sb_v_double_arrow",    CURSOR_ROWRESIZE    },

    // Miscellaneous
    {"move",        "fleur",                CURSOR_FLEUR        },
    {"cell",        "cross",                CURSOR_CELL         },
    {"context-menu","middlebutton",         CURSOR_CONTEXT_MENU },
    {"no-drop",     "X_cursor",             CURSOR_DND_NODROP   },
    {"zoom-in",     "left_ptr",             CURSOR_ZOOMIN       },
    {"zoom-out",    "left_ptr",             CURSOR_ZOOMOUT      },
};


CursorViewer::CursorViewer(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("Cursor theme viewer");
    resize(760, 580);
    setWindowIcon(QIcon(APPICON_PATH));

    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    auto *mainLayout = new QHBoxLayout(centralWidget);

    // LEFT PANEL: File Selection Tree
    auto *leftLayout = new QVBoxLayout();
    auto *themeLabel = new QLabel("<b>Active Cursor Theme:</b>", this);
    m_themeComboBox = new QComboBox(this);
    leftLayout->addWidget(themeLabel);
    leftLayout->addWidget(m_themeComboBox);

    m_fileList = new QListWidget(this);
    leftLayout->addWidget(m_fileList);
    mainLayout->addLayout(leftLayout, 1);

    m_searchPaths << QDir::homePath() + "/.local/share/icons"
                  << QDir::homePath() + "/.icons"
                  << "/usr/share/icons"
                  << "/usr/local/share/icons";

    // RIGHT PANEL: Display, Sizes Dropdown, & Sandbox
    auto *rightLayout = new QVBoxLayout();

    // Size Selector Dropdown Box
    auto *sizeSelectorLayout = new QHBoxLayout();
    auto *sizeLabel = new QLabel("<b>Available Sizes:</b>", this);
    m_sizeComboBox = new QComboBox(this);
    sizeSelectorLayout->addWidget(sizeLabel);
    sizeSelectorLayout->addWidget(m_sizeComboBox, 1);
    rightLayout->addLayout(sizeSelectorLayout);

    // Grid container for frames
    auto *framesTitle = new QLabel("<b>Native Frames:</b>", this);
    rightLayout->addWidget(framesTitle);

    m_framesScroll = new QScrollArea(this);
    m_framesScroll->setWidgetResizable(true);
    m_framesWidget = new QWidget(this);
    m_framesGrid = new QGridLayout(m_framesWidget);
    m_framesGrid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_framesScroll->setWidget(m_framesWidget);
    //m_framesScroll->setStyleSheet("background-color: #333333; border: 1px solid #555555;");
    rightLayout->addWidget(m_framesScroll, 2);

    // Sandbox row layout container
    auto *sandboxLayout = new QHBoxLayout();

    auto *interactiveVBox = new QVBoxLayout();
    auto *interactiveTitle = new QLabel("<b>Interactive Sandbox:</b>", this);
    m_testPadLabel = new QLabel("Hover here to test<br>(Automatically loaded)", this);
    m_testPadLabel->setAlignment(Qt::AlignCenter);
    m_testPadLabel->setStyleSheet("background-color: #222222; color: #aaaaaa; border: 2px dashed #666666;");
    m_testPadLabel->setFixedSize(250, 120);
    interactiveVBox->addWidget(interactiveTitle);
    interactiveVBox->addWidget(m_testPadLabel);
    sandboxLayout->addLayout(interactiveVBox);

    rightLayout->addLayout(sandboxLayout, 1);
    mainLayout->addLayout(rightLayout, 3);

    // Connections
    m_animTimer = new QTimer(this);
    QObject::connect(m_animTimer, &QTimer::timeout, this, [this]()
    {
        this->tickLiveAnimation();
    });

    QObject::connect(m_fileList, &QListWidget::currentTextChanged, this, [this](const QString &currentText)
    {
        if(!currentText.isEmpty())
        {
            this->m_currentFilePath = m_currentCursorsPath + "/" + currentText;
            this->parseAvailableSizes();
        }
    });

    QObject::connect(m_sizeComboBox, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        if(index >= 0)
        {
            quint32 selectedSize = m_sizeComboBox->itemData(index).toUInt();
            this->loadXcursorSize(selectedSize);
        }
    });

    QObject::connect(m_themeComboBox, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        if(index >= 0)
        {
            QString targetPath = m_themeComboBox->itemData(index).toString();
            this->loadThemeDirectory(targetPath);
        }
    });

    this->createMenuBar();

    m_testPadLabel->setMouseTracking(true);
    m_testPadLabel->installEventFilter(this);

    QObject::connect(m_fileList, &QListWidget::currentTextChanged, this, [this](const QString &currentText)
    {
        if(!currentText.isEmpty())
        {
            this->m_currentFilePath = m_currentCursorsPath + "/" + currentText;
            this->parseAvailableSizes();
            this->startLivePreviewLoop();
        }
    });

    statusBar()->showMessage("Ready");
}

#ifdef __laylaos__

void CursorViewer::setAsSystemTheme()
{
    int cursorCount = m_fileList->count();

    if(cursorCount == 0)
    {
        statusBar()->showMessage("No cursors available to set");
        return;
    }

    qint32 targetSize = m_sizeComboBox->currentData().toInt();

    if(targetSize == 0)
    {
        statusBar()->showMessage("No valid cursor size selected");
        return;
    }

    for(int i = 0; i < cursorCount; ++i)
    {
        QListWidgetItem *item = m_fileList->item(i);
        QString cursorName = item->text();

        int curId = CURSOR_NONE;

        for(const auto &mapping : cursorMappings)
        {
            if(cursorName == mapping.fdoName || cursorName == mapping.x11Name)
            {
                curId = mapping.curId;
                break;
            }
        }

        if(curId == CURSOR_NONE) continue; 

        QString fullPath = m_currentCursorsPath + "/" + cursorName;
        QByteArray byteBuffer = fullPath.toUtf8();
        const char *c_str = byteBuffer.constData();

        cursor_change_syscursor(curId, targetSize, c_str);
    }

    statusBar()->showMessage("Set new system cursor theme");
}

#endif

void CursorViewer::createMenuBar()
{
    auto *fileMenu = menuBar()->addMenu("&File");

#ifdef __laylaos__
    auto *setAction = fileMenu->addAction("&Set as system theme");
    setAction->setShortcut(QKeySequence::Save);

    QObject::connect(setAction, &QAction::triggered, this, [this]()
    {
        this->setAsSystemTheme();
    });
#endif

    auto *openAction = fileMenu->addAction("&Open theme folder...");
    openAction->setShortcut(QKeySequence::Open);

    QObject::connect(openAction, &QAction::triggered, this, [this]()
    {
        this->openCustomFolderDialog();
    });

    auto *actAbout = fileMenu->addAction("&About...");

    QObject::connect(actAbout, &QAction::triggered, this, [this]()
    {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("About Cursor theme viewer");
        msgBox.setTextFormat(Qt::MarkdownText);

        msgBox.setText(
            "A small utility to view X11 cursor themes, built using **Qt 6**.\n\n"
            "Tailored to run seamlessly across traditional GNU/Linux host environments, LaylaOS, and "
            "custom bare-metal hobby operating system platforms."
        );

        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });

    fileMenu->addSeparator();

    auto *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    QObject::connect(exitAction, &QAction::triggered, this, &QWidget::close);
}

void CursorViewer::openCustomFolderDialog()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Theme Folder", "/usr/share/icons");
    if (!dir.isEmpty()) {
        handleTargetDirectory(dir);
    }
}

void CursorViewer::processCommandLineArgument(const QString &argPath)
{
    QFileInfo targetInfo(argPath);
    if(!targetInfo.exists())
    {
        statusBar()->showMessage("Command line argument path does not exist");
        return;
    }

    if(targetInfo.isDir())
    {
        handleTargetDirectory(targetInfo.absoluteFilePath());
    } 
    else if(targetInfo.isFile())
    {
        QDir parentDir = targetInfo.dir();

        // If file resides directly in a 'cursors' folder
        if(parentDir.dirName() == "cursors")
        {
            handleTargetDirectory(parentDir.absolutePath(), targetInfo.fileName());
        } 
        // If parent has a 'cursors' subfolder, assume the file was referenced relative to theme root
        else if(parentDir.exists("cursors"))
        {
            handleTargetDirectory(parentDir.absoluteFilePath("cursors"), targetInfo.fileName());
        } 
        else
        {
            // Fallback: Use the file's direct directory folder context
            handleTargetDirectory(parentDir.absolutePath(), targetInfo.fileName());
        }
    }
}

void CursorViewer::handleTargetDirectory(const QString &dirPath, const QString &selectFileName)
{
    m_themeComboBox->blockSignals(true);
    m_themeComboBox->clear();

    QDir targetDir(dirPath);
    QString resolvedCursorsPath;
    QString themeName;

    // Case 1: Passed folder *is* the explicit cursors folder
    if(targetDir.dirName() == "cursors")
    {
        resolvedCursorsPath = targetDir.absolutePath();
        targetDir.cdUp(); // Go up one level to grab actual theme
        themeName = targetDir.dirName();
    }
    // Case 2: Passed folder contains a "cursors" subfolder
    else if(targetDir.exists("cursors"))
    {
        resolvedCursorsPath = targetDir.absoluteFilePath("cursors");
        themeName = targetDir.dirName();
    }
    // Case 3: Treat the current folder directly as the target
    else
    {
        resolvedCursorsPath = targetDir.absolutePath();
        themeName = targetDir.dirName();
    }

    // Populate combobox with a single entry
    m_themeComboBox->addItem(QString("%1 (Custom)").arg(themeName), resolvedCursorsPath);
    m_themeComboBox->blockSignals(false);

    m_themeComboBox->setCurrentIndex(-1);
    m_themeComboBox->setCurrentIndex(0);
    this->loadThemeDirectory(resolvedCursorsPath);

    // If an explicit cursor filename parameter was requested, jump to it
    if(!selectFileName.isEmpty())
    {
        int rowCount = m_fileList->count();
        for(int i = 0; i < rowCount; ++i)
        {
            if(m_fileList->item(i)->text() == selectFileName)
            {
                m_fileList->setCurrentRow(i);
                break;
            }
        }
    }
}

void CursorViewer::discoverCursorThemes()
{
    m_themeComboBox->blockSignals(true);
    m_themeComboBox->clear();

    std::set<QString> addedThemeNames; // Prevent showing duplicate names

    for(const QString &searchPath : m_searchPaths)
    {
        QDir baseDir(searchPath);
        if(!baseDir.exists()) continue;

        // Scan subdirectories for potential cursor themes
        QStringList subDirs = baseDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for(const QString &dirName : subDirs)
        {
            QDir themeDir(baseDir.filePath(dirName));

            // A directory is a valid cursor theme if it contains a "cursors" subfolder
            if(themeDir.exists("cursors"))
            {
                QString absoluteCursorsPath = themeDir.absoluteFilePath("cursors");

                // Disambiguate names if a theme exists locally and globally (e.g. Adwaita)
                QString uniqueDisplayName = dirName;
                if(addedThemeNames.find(dirName) != addedThemeNames.end())
                {
                    uniqueDisplayName += QString(" (%1)").arg(baseDir.dirName());
                }

                m_themeComboBox->addItem(uniqueDisplayName, absoluteCursorsPath);
                addedThemeNames.insert(dirName);
            }
        }
    }

    m_themeComboBox->blockSignals(false);

    if(m_themeComboBox->count() > 0)
    {
        m_themeComboBox->setCurrentIndex(-1);
        m_themeComboBox->setCurrentIndex(0);
    }
    else
    {
        statusBar()->showMessage("No cursor themes found");
    }
}

void CursorViewer::loadThemeDirectory(const QString &path)
{
    m_fileList->clear();
    QDir dir(path);
    if(!dir.exists())
    {
        statusBar()->showMessage("Directory does not exist");
        return;
    }

    m_currentCursorsPath = path;

    QStringList sanitizedFiles;
    for(const auto &mapping : cursorMappings)
    {
        QString fdoName = mapping.fdoName;
        QString x11Name = mapping.x11Name;

        // Skip hex/hash named duplicates; look only for standard files
        QFileInfo fdoFile(dir.filePath(fdoName));
        if(fdoFile.exists() && !fdoFile.isSymLink() && fdoName.indexOf(QRegularExpression("^[0-9a-fA-F]{32}$")) == -1)
        {
            sanitizedFiles.append(fdoName);
        }
        else
        {
            // Fallback: If Freedesktop standard name isn't there, look for the matching X11 standard asset
            QFileInfo x11File(dir.filePath(x11Name));
            if(x11File.exists() && !x11File.isSymLink())
            {
                if(!sanitizedFiles.contains(x11Name))
                {
                    sanitizedFiles.append(x11Name);
                }
            }
        }
    }

    m_fileList->addItems(sanitizedFiles);
    resetViewerState();

    if(!sanitizedFiles.isEmpty())
    {
        int startIndex = 0;
        if(sanitizedFiles.contains("default"))
        {
            startIndex = sanitizedFiles.indexOf("default");
        }
        else if(sanitizedFiles.contains("left_ptr"))
        {
            startIndex = sanitizedFiles.indexOf("left_ptr");
        }

        // Set the row, which triggers parseAvailableSizes()
        m_fileList->setCurrentRow(startIndex);

        statusBar()->showMessage(QString("Loaded %1 cursors from %2")
                                         .arg(sanitizedFiles.size())
                                         .arg(path));
    }
    else
    {
        statusBar()->showMessage(QString("Loaded %1 cursors from %2 (but it contains no valid cursors)")
                                         .arg(sanitizedFiles.size())
                                         .arg(path));
    }
}

void CursorViewer::resetViewerState()
{
    m_animTimer->stop();
    m_parsedFrames.clear();
    m_testPadLabel->setCursor(Qt::ArrowCursor);
    m_testPadLabel->setText("Hover here to test<br>(Automatically loaded)");

    // Clear layout grid
    QLayoutItem *item;
    while((item = m_framesGrid->takeAt(0)) != nullptr)
    {
        delete item->widget();
        delete item;
    }
}

// Phase 1: Read TOC and populate sizes dropdown
void CursorViewer::parseAvailableSizes()
{
    m_sizeComboBox->blockSignals(true);
    m_sizeComboBox->clear();
    resetViewerState();

    QFileInfo checkLink(m_currentFilePath);
    m_resolvedPath = checkLink.isSymLink() ? checkLink.symLinkTarget() : m_currentFilePath;

    QFile file(m_resolvedPath);
    if(!file.open(QIODevice::ReadOnly))
    {
        statusBar()->showMessage("Failed to open cursor file");
        m_sizeComboBox->blockSignals(false);
        return;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    quint32 magic, headerSize, version, ntoc;
    in >> magic >> headerSize >> version >> ntoc;

    if(magic != 0x72756358)
    {
        statusBar()->showMessage("Cursor lacks expected 'Xcur' file signature");
        m_sizeComboBox->blockSignals(false);
        return;
    }

    m_cachedToc.resize(ntoc);
    std::set<quint32> uniqueSizes;
    std::vector<quint32> orderedSizes; // Tracks insertion order to match the first item in the file

    for(quint32 i = 0; i < ntoc; ++i)
    {
        in >> m_cachedToc[i].type >> m_cachedToc[i].subtype >> m_cachedToc[i].position;

        // Type 0xfffd0002 refers to image blocks, subtype handles the target resolution size scale
        if(m_cachedToc[i].type == 0xfffd0002)
        {
            if(uniqueSizes.find(m_cachedToc[i].subtype) == uniqueSizes.end())
            {
                uniqueSizes.insert(m_cachedToc[i].subtype);
                orderedSizes.push_back(m_cachedToc[i].subtype);
            }
        }
    }

    if(orderedSizes.empty())
    {
        statusBar()->showMessage("No readable image frames inside cursor file");
        m_sizeComboBox->blockSignals(false);
        return;
    }

    // Populate ComboBox dropdown
    for(quint32 size : orderedSizes)
    {
        m_sizeComboBox->addItem(QString("%1 px").arg(size), size);
    }

    m_sizeComboBox->blockSignals(false);

    // Automatically trigger the first size layout found sequentially inside the file
    m_sizeComboBox->setCurrentIndex(0);
    loadXcursorSize(orderedSizes[0]);
}

// Phase 2: Extract frame images matching specific user selected scale size metric
void CursorViewer::loadXcursorSize(quint32 targetSizeSubtype)
{
    resetViewerState();

    QFile file(m_resolvedPath);
    if(!file.open(QIODevice::ReadOnly)) return;

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    for(const auto &entry : m_cachedToc)
    {
        if(entry.type == 0xfffd0002 && entry.subtype == targetSizeSubtype)
        {
            file.seek(entry.position);

            quint32 imgHeader, imgType, imgSubtype, imgVersion;
            quint32 width, height, xhot, yhot, delay;
            in >> imgHeader >> imgType >> imgSubtype >> imgVersion;
            in >> width >> height >> xhot >> yhot >> delay;

            if(width > 512 || height > 512 || width == 0 || height == 0) continue;

            QImage cursorImg(width, height, QImage::Format_ARGB32);

            for(quint32 y = 0; y < height; ++y)
            {
                quint32 *rowPixel = reinterpret_cast<quint32*>(cursorImg.scanLine(y));

                for(quint32 x = 0; x < width; ++x)
                {
                    in >> rowPixel[x];
                }
            }

            CursorFrame frame;
            frame.pixmap = QPixmap::fromImage(cursorImg);
            frame.width = width;
            frame.height = height;
            frame.xhot = xhot;
            frame.yhot = yhot;
            frame.delay = (delay == 0) ? 100 : delay;
            m_parsedFrames.push_back(frame);
        }
    }

    if(m_parsedFrames.empty())
    {
        statusBar()->showMessage("No valid frames for this cursor size");
        return;
    }

    // Render every frame onto grid
    for(size_t i = 0; i < m_parsedFrames.size(); ++i)
    {
        int row = static_cast<int>(i / 5);
        int col = static_cast<int>(i % 5);
        auto *frameBox = new QWidget(m_framesWidget);
        auto *boxLayout = new QVBoxLayout(frameBox);boxLayout->setContentsMargins(4, 4, 4, 4);
        auto *imgLabel = new QLabel(frameBox);

        imgLabel->setPixmap(m_parsedFrames[i].pixmap);
        imgLabel->setAlignment(Qt::AlignCenter);
        imgLabel->setStyleSheet("border: 1px dashed #666666;");
        imgLabel->setFixedSize(64, 64);

        auto *metaLabel = new QLabel(QString("#%1 (%2ms)").arg(i).arg(m_parsedFrames[i].delay), frameBox);
        metaLabel->setAlignment(Qt::AlignCenter);
        metaLabel->setStyleSheet("color: #555555; font-size: 10px;");
        boxLayout->addWidget(imgLabel);
        boxLayout->addWidget(metaLabel);
        m_framesGrid->addWidget(frameBox, row, col);
    }

    QFileInfo originalFile(m_currentFilePath);
    statusBar()->showMessage(QString("Active File: %1 | Frames: %2 | Canvas: %3x%4 px")
                                 .arg(originalFile.fileName())
                                 .arg(m_parsedFrames.size())
                                 .arg(m_parsedFrames[0].width)
                                 .arg(m_parsedFrames[0].height));
}

void CursorViewer::tickLiveAnimation()
{
    if(m_parsedFrames.empty()) return;
    const auto &frame = m_parsedFrames[m_currentFrameIndex];
    QCursor liveCursor(frame.pixmap, static_cast<int>(frame.xhot), static_cast<int>(frame.yhot));
    m_testPadLabel->setCursor(liveCursor);
    m_testPadLabel->setText(QString("Live Testing Active!\nFrame Loop: %1 / %2")
        .arg(m_currentFrameIndex + 1)
        .arg(m_parsedFrames.size()));
    m_animTimer->start(static_cast<int>(frame.delay));
    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_parsedFrames.size();
}

bool CursorViewer::eventFilter(QObject *watched, QEvent *event)
{
    if(watched == m_testPadLabel)
    {
        if(event->type() == QEvent::Enter)
        {
            m_isMouseInsideSandbox = true;
            setAnimationPlayback(true);
        } 
        else if(event->type() == QEvent::Leave)
        {
            m_isMouseInsideSandbox = false;
            setAnimationPlayback(false);
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void CursorViewer::setAnimationPlayback(bool active)
{
    if(m_parsedFrames.empty()) return;

    if(active && m_isMouseInsideSandbox)
    {
        if(m_parsedFrames.size() <= 1)
        {
            const auto &frame = m_parsedFrames[0];
            QCursor staticCursor(frame.pixmap, static_cast<int>(frame.xhot), static_cast<int>(frame.yhot));
            m_testPadLabel->setCursor(staticCursor);
            m_testPadLabel->setText("<b>Live Test Active!</b><br>Static Cursor Loaded.");
        }
        else
        {
            if(!m_animTimer->isActive())
            {
                tickLiveAnimation();
            }
        }
    }
    else
    {
        m_animTimer->stop();

        if(m_parsedFrames.size() > 1)
        {
            m_testPadLabel->setText("<b>Animation Paused</b><br>Hover back inside to resume.");
        }
    }
}

void CursorViewer::startLivePreviewLoop()
{
    if(m_parsedFrames.empty()) return;

    m_animTimer->stop();
    m_currentFrameIndex = 0;

    setAnimationPlayback(m_isMouseInsideSandbox);

    if(!m_isMouseInsideSandbox)
    {
        m_testPadLabel->setText("Hover here to test<br>(Automatically loaded)");
        m_testPadLabel->setCursor(Qt::ArrowCursor);
    }
}

