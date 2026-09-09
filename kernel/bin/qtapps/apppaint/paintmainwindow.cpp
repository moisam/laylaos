#include "paintmainwindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QComboBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QIcon>
#include <QDockWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsProxyWidget>
#include <QSlider>
#include <QSpacerItem>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>

#define APPICON_PATH            "/usr/share/gui/icons/paint.png"

PaintMainWindow::PaintMainWindow(QWidget *parent)
    : QMainWindow(parent), m_currentFilePath(""), m_zoomLevel(1.0),
      m_canvas(nullptr), m_view(nullptr), m_scene(nullptr),
      m_coordsLabel(nullptr), m_zoomLabel(nullptr),
      m_styleCombo(nullptr), m_patternCombo(nullptr), m_bucketModeCombo(nullptr),
      m_styleAction(nullptr), m_patternAction(nullptr), m_bucketModeAction(nullptr),
      m_sprayLabelWidget(nullptr), m_spraySliderWidget(nullptr), m_sprayLabelAction(nullptr), 
      m_spraySliderAction(nullptr),
      m_frontColorPreview(nullptr), m_backColorPreview(nullptr)
{
    setWindowTitle("Paint");
    resize(600, 400);
    setWindowIcon(QIcon(APPICON_PATH));

    m_canvas = new PaintCanvas(nullptr);
    
    setupZoomingView();
    initMenus();
    initToolbars();
    initStatusBar();
    initPaletteSidebar();

    m_canvas->onMouseMoved = [this](const QPoint &pos) { updateStatusBarCoords(pos); };
    m_canvas->onCanvasResized = [this]() { syncSceneBounds(); };
    m_canvas->onColorsChanged = [this]() { refreshColorDisplayPanels(); };

    // Set layout positions for hidden components
    updateVisibilityStates();
    refreshColorDisplayPanels();
}

void PaintMainWindow::setupZoomingView() 
{
    m_scene = new QGraphicsScene(this);
    QGraphicsProxyWidget *proxy = m_scene->addWidget(m_canvas);
    proxy->setPos(0, 0);
    
    m_view = new QGraphicsView(m_scene, this);
    m_view->setRenderHint(QPainter::Antialiasing, false);
    m_view->setRenderHint(QPainter::SmoothPixmapTransform, false);
    m_view->setBackgroundRole(QPalette::Dark);
    
    setCentralWidget(m_view);
    m_scene->setSceneRect(0, 0, m_canvas->imageSize().width(), m_canvas->imageSize().height());
}

QIcon PaintMainWindow::createVectorIcon(const QString &toolType) 
{
    // Generate a transparent pixmap square at standard application sizing
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    
    QPen pen(Qt::black, 2);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if(toolType == "Free Draw") 
    {
        QPainterPath path(QPointF(4, 20));
        path.cubicTo(QPointF(8, 4), QPointF(16, 4), QPointF(20, 12));
        painter.drawPath(path);
    }
    else if(toolType == "Eraser") 
    {
        painter.setBrush(QBrush(Qt::white));
        painter.drawRect(4, 6, 14, 12);
        painter.fillRect(12, 6, 6, 12, Qt::lightGray); // shaded edge
    }
    else if(toolType == "Line") 
    {
        painter.drawLine(4, 20, 20, 4);
    }
    else if(toolType == "Rectangle") 
    {
        painter.drawRect(4, 4, 16, 16);
    }
    else if(toolType == "Ellipse") 
    {
        painter.drawEllipse(4, 4, 16, 16);
    }
    else if(toolType == "Bucket Fill") 
    {
        painter.setBrush(QBrush(Qt::darkCyan));
        painter.drawRect(6, 4, 10, 10);
        painter.drawLine(11, 14, 6, 20); // drip indicator line
    }
    else if(toolType == "Text") 
    {
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(18);
        painter.setFont(font);
        painter.drawText(QRect(0, 0, 24, 24), Qt::AlignCenter, "A");
    }
    else if(toolType == "Select") 
    {
        QPen dashPen(Qt::blue, 1, Qt::DashLine);
        painter.setPen(dashPen);
        painter.drawRect(3, 3, 18, 18);
    }
    else if(toolType == "Magic Wand") 
    {
        painter.drawLine(4, 20, 16, 8);
        painter.setBrush(QBrush(Qt::yellow));
        painter.drawEllipse(15, 3, 6, 6); // star flare head node
    }
    else if(toolType == "Eyedropper") 
    {
        painter.drawLine(4, 20, 14, 10);
        painter.fillRect(14, 6, 4, 4, Qt::darkGray);
    }

    return QIcon(pixmap);
}

bool PaintMainWindow::maybeSave() 
{
    // Check if the canvas has any unsaved pixel modifications
    if(!m_canvas->isDirty()) return true;

    auto result = QMessageBox::warning(this, "Unsaved changes",
        "The workspace contains unsaved changes. Do you want to save them?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    if(result == QMessageBox::Save) { fileSave(); return !m_canvas->isDirty(); } 
    else if(result == QMessageBox::Cancel) { return false; }
    return true; // Discard selected
}

void PaintMainWindow::closeEvent(QCloseEvent *event) 
{
    if(maybeSave()) { event->accept(); } 
    else { event->ignore(); }
}

void PaintMainWindow::initMenus() 
{
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *newAct = fileMenu->addAction("&New...");
    connect(newAct, &QAction::triggered, this, [this]() { fileNew(); });
    newAct->setShortcut(QKeySequence::New);

    QAction *openAct = fileMenu->addAction(QIcon::fromTheme("document-open"), "&Open...");
    connect(openAct, &QAction::triggered, this, [this]() { fileOpen(); });
    openAct->setShortcut(QKeySequence::Open);

    QAction *saveAct = fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save");
    connect(saveAct, &QAction::triggered, this, [this]() { fileSave(); });
    saveAct->setShortcut(QKeySequence::Save);

    QAction *saveAsAct = fileMenu->addAction(QIcon::fromTheme("document-save-as"), "Save &As...");
    connect(saveAsAct, &QAction::triggered, this, [this]() { fileSaveAs(); });
    saveAsAct->setShortcut(QKeySequence::SaveAs);

    QAction *resizeAct = fileMenu->addAction("&Resize Canvas...");
    connect(resizeAct, &QAction::triggered, this, [this]() { fileResizeCanvas(); });

    fileMenu->addSeparator();
    QAction *quitAct = fileMenu->addAction(QIcon::fromTheme("application-exit"), "&Quit");
    connect(quitAct, &QAction::triggered, this, [this]() { close(); });
    quitAct->setShortcut(QKeySequence::Quit);

    QMenu *editMenu = menuBar()->addMenu("&Edit");
    QAction *undoAct = editMenu->addAction(QIcon::fromTheme("edit-undo"), "&Undo");
    connect(undoAct, &QAction::triggered, this, [this]() { editUndo(); });
    undoAct->setShortcut(QKeySequence::Undo);

    QAction *redoAct = editMenu->addAction(QIcon::fromTheme("edit-redo"), "&Redo");
    connect(redoAct, &QAction::triggered, this, [this]() { editRedo(); });
    redoAct->setShortcut(QKeySequence::Redo);

    editMenu->addSeparator();
    QAction *copyAct = editMenu->addAction(QIcon::fromTheme("edit-copy"), "&Copy Selection");
    connect(copyAct, &QAction::triggered, this, [this]() { editCopy(); });
    copyAct->setShortcut(QKeySequence::Copy);

    QAction *pasteAct = editMenu->addAction(QIcon::fromTheme("edit-paste"), "&Paste Image");
    connect(pasteAct, &QAction::triggered, this, [this]() { editPaste(); });
    pasteAct->setShortcut(QKeySequence::Paste);

    QAction *cropAct = editMenu->addAction(QIcon::fromTheme("transform-crop"), "Cr&op to Selection");
    connect(cropAct, &QAction::triggered, this, [this]() { editCrop(); });

    QMenu *viewMenu = menuBar()->addMenu("&View");
    QAction *zInAct = viewMenu->addAction(QIcon::fromTheme("zoom-in"), "Zoom &In");
    connect(zInAct, &QAction::triggered, this, [this]() { zoomIn(); });
    zInAct->setShortcut(QKeySequence::ZoomIn);

    QAction *zOutAct = viewMenu->addAction(QIcon::fromTheme("zoom-out"), "Zoom &Out");
    connect(zOutAct, &QAction::triggered, this, [this]() { zoomOut(); });
    zOutAct->setShortcut(QKeySequence::ZoomOut);

    QAction *zResAct = viewMenu->addAction(QIcon::fromTheme("zoom-original"), "&Actual Size");
    connect(zResAct, &QAction::triggered, this, [this]() { zoomReset(); });

    QMenu *filterMenu = menuBar()->addMenu("Fi&lters");
    QAction *invAct = filterMenu->addAction("Invert Colors");
    connect(invAct, &QAction::triggered, this, [this]() { filterInvert(); });

    QAction *grayAct = filterMenu->addAction("Convert to Greyscale");
    connect(grayAct, &QAction::triggered, this, [this]() { filterGreyscale(); });

    QAction *blurAct = filterMenu->addAction("Apply Box Blur");
    connect(blurAct, &QAction::triggered, this, [this]() { filterBlur(); });

    QAction *sepiaAct = filterMenu->addAction("Apply Sepia Vintage Tint");
    connect(sepiaAct, &QAction::triggered, this, [this]() { filterSepia(); });

    QAction *brightAct = filterMenu->addAction("Boost Image Brightness (+25)");
    connect(brightAct, &QAction::triggered, this, [this]() { filterBrightness(); });

    QMenu *helpMenu = menuBar()->addMenu("&Help");
    QAction *shortAct = helpMenu->addAction("Keyboard &Shortcuts");
    connect(shortAct, &QAction::triggered, this, [this]() { helpShortcuts(); });
    QAction *aboutAct = helpMenu->addAction(QIcon::fromTheme("help-about"), "&About Paint");
    connect(aboutAct, &QAction::triggered, this, [this]() { helpAbout(); });
}

void PaintMainWindow::initToolbars() 
{
    QToolBar *toolbar = addToolBar("Main ToolBar");

    toolbar->setIconSize(QSize(20, 20));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    QStringList tools = 
    {
        "Free Draw", "Eraser", "Line", "Rectangle", "Ellipse",
        "Bucket Fill", "Text", "Select", "Magic Wand", "Eyedropper"
    };

    for(const auto &toolName : tools) 
    {
        QIcon customIcon = createVectorIcon(toolName);
        QAction *action = new QAction(customIcon, toolName, this);
        action->setToolTip(toolName);
        action->setCheckable(true);
        if(toolName == "Free Draw") action->setChecked(true);

        connect(action, &QAction::triggered, this, [this, action]() 
        { 
            changeTool(action); 
        });
        
        toolbar->addAction(action);
        m_toolActions.append(action);
    }

    toolbar->addSeparator();

    QComboBox *thickCombo = new QComboBox(this);
    thickCombo->addItems({"2 px", "5 px", "10 px", "20 px"});
    int structuralThicknesses[] = {2, 5, 10, 20};
    connect(thickCombo, &QComboBox::currentIndexChanged, this, [this, structuralThicknesses](int index)
    {
        m_canvas->setThickness(structuralThicknesses[index]);
    });
    toolbar->addWidget(thickCombo);

    m_styleCombo = new QComboBox(this);
    m_styleCombo->addItem("Solid Line", static_cast<int>(Qt::SolidLine));
    m_styleCombo->addItem("Dotted Line", static_cast<int>(Qt::DotLine));
    m_styleCombo->addItem("Dashed Line", static_cast<int>(Qt::DashLine));
    
    connect(m_styleCombo, &QComboBox::currentIndexChanged, this, [this](int index)
    {
        m_canvas->setLineStyle(static_cast<Qt::PenStyle>(m_styleCombo->itemData(index).toInt()));
    });

    m_styleAction = toolbar->addWidget(m_styleCombo);

    m_patternCombo = new QComboBox(this);
    m_patternCombo->addItems({"Solid Brush", "Airbrush Spray", "Calligraphy Slant"});
    QString structuralPatterns[] = {"Solid", "Airbrush", "Calligraphy"};
    connect(m_patternCombo, &QComboBox::currentIndexChanged, this, [this, structuralPatterns](int index)
    {
        m_canvas->setBrushPattern(structuralPatterns[index]);
    });

    m_patternAction = toolbar->addWidget(m_patternCombo);

    m_bucketModeCombo = new QComboBox(this);
    m_bucketModeCombo->addItems({"Bucket: Solid", "Bucket: Vertical Grad", "Bucket: Horizontal Grad"});
    QString structuralBucketModes[] = {"Solid", "Vertical Gradient", "Horizontal Gradient"};
    connect(m_bucketModeCombo, &QComboBox::currentIndexChanged, this, [this, structuralBucketModes](int index)
    {
        m_canvas->setBucketMode(structuralBucketModes[index]);
    });

    m_bucketModeAction = toolbar->addWidget(m_bucketModeCombo);

    m_sprayLabelWidget = new QLabel("  Spray: ", this);
    m_sprayLabelAction = toolbar->addWidget(m_sprayLabelWidget);

    m_spraySliderWidget = new QSlider(Qt::Horizontal, this);
    m_spraySliderWidget->setRange(5, 150);
    m_spraySliderWidget->setValue(30);
    m_spraySliderWidget->setFixedWidth(80);
    connect(m_spraySliderWidget, &QSlider::valueChanged, this, [this](int value)
    {
        m_canvas->setAirbrushDensity(value);
    });
    m_spraySliderAction = toolbar->addWidget(m_spraySliderWidget);

    QLabel *opacityLabel = new QLabel("  Alpha: ", this);
    toolbar->addWidget(opacityLabel);

    QSlider *opacitySlider = new QSlider(Qt::Horizontal, this);
    opacitySlider->setRange(0, 255);
    opacitySlider->setValue(255);
    opacitySlider->setFixedWidth(80);
    connect(opacitySlider, &QSlider::valueChanged, this, [this](int value)
    {
        m_canvas->setAlpha(value);
    });
    toolbar->addWidget(opacitySlider);
    
    toolbar->addSeparator();
    
    QAction *zInAct = toolbar->addAction(QIcon::fromTheme("zoom-in"), "+");
    connect(zInAct, &QAction::triggered, this, [this]() { zoomIn(); });

    QAction *zOutAct = toolbar->addAction(QIcon::fromTheme("zoom-out"), "-");
    connect(zOutAct, &QAction::triggered, this, [this]() { zoomOut(); });
}

void PaintMainWindow::initStatusBar() 
{
    m_coordsLabel = new QLabel("Coordinates: 0, 0 px", this);
    m_zoomLabel = new QLabel("Zoom: 100%", this);
    statusBar()->addPermanentWidget(m_zoomLabel);
    statusBar()->addPermanentWidget(m_coordsLabel);
    statusBar()->showMessage("Ready", 3000);
}

void PaintMainWindow::initPaletteSidebar() 
{
    QDockWidget *dock = new QDockWidget("Color Swatches", this);
    QWidget *paletteContainer = new QWidget(dock);

    // Create a vertical main layout to separate color buttons from the empty spaces
    QVBoxLayout *mainVBox = new QVBoxLayout(paletteContainer);
    mainVBox->setContentsMargins(5, 5, 5, 5);

    QWidget *gridWidget = new QWidget(paletteContainer);
    QGridLayout *gridLayout = new QGridLayout(gridWidget);
    gridLayout->setSpacing(4);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    QList<QColor> presetColors = 
    {
        Qt::black, Qt::darkGray, Qt::gray, Qt::lightGray, Qt::white,
        Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta, Qt::yellow,
        QColor("#e67e22"), QColor("#8e44ad"), QColor("#27ae60"), QColor("#2980b9"), QColor("#f1c40f")
    };

    int row = 0, col = 0;

    for(const QColor &color : presetColors) 
    {
        QPushButton *colorBtn = new QPushButton(paletteContainer);
        colorBtn->setFixedSize(26, 26);
        colorBtn->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 2px;").arg(color.name()));
        
        connect(colorBtn, &QPushButton::clicked, this, [this, color]() 
        {
            m_canvas->setPrimaryColor(color);
        });
        
        colorBtn->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(colorBtn, &QWidget::customContextMenuRequested, this, [this, color](const QPoint&)
        {
            m_canvas->setSecondaryColor(color);
        });

        gridLayout->addWidget(colorBtn, row, col);
        if(++col > 1) { col = 0; row++; }
    }

    gridWidget->setLayout(gridLayout);
    mainVBox->addWidget(gridWidget);

    mainVBox->addSpacing(15);

    m_frontColorPreview = new QLabel("", paletteContainer);
    m_frontColorPreview->setFixedSize(60, 30);
    m_frontColorPreview->setAlignment(Qt::AlignCenter);
    m_frontColorPreview->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_frontColorPreview->setCursor(Qt::PointingHandCursor);
    m_frontColorPreview->installEventFilter(this);

    m_backColorPreview = new QLabel("", paletteContainer);
    m_backColorPreview->setFixedSize(60, 30);
    m_backColorPreview->setAlignment(Qt::AlignCenter);
    m_backColorPreview->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_backColorPreview->setCursor(Qt::PointingHandCursor);
    m_backColorPreview->installEventFilter(this);

    mainVBox->addWidget(m_frontColorPreview);
    mainVBox->addWidget(m_backColorPreview);

    // Install a vertical expanding layout spacer down under the button grid widget layer.
    // This pins the swatches to the top of the sidebar when resizing the window.
    mainVBox->addSpacerItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

    paletteContainer->setLayout(mainVBox);
    dock->setWidget(paletteContainer);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

bool PaintMainWindow::eventFilter(QObject *watched, QEvent *event) 
{
    if(event->type() == QEvent::MouseButtonPress) 
    {
        if(watched == m_frontColorPreview) 
        {
            chooseFrontColor();
            return true;
        }
        else if(watched == m_backColorPreview) 
        {
            chooseBackColor();
            return true;
        }
    }
    
    return QMainWindow::eventFilter(watched, event);
}

void PaintMainWindow::updateVisibilityStates() 
{
    if(!m_canvas || !m_styleAction || !m_patternAction || !m_bucketModeAction || 
       !m_sprayLabelAction || !m_spraySliderAction)
        return;

    QString tool = m_canvas->activeTool();
    QString pattern = m_canvas->activeBrushPattern();

    m_bucketModeAction->setVisible(tool == "Bucket Fill");
    m_patternAction->setVisible(tool == "Free Draw");
    
    m_styleAction->setVisible(tool == "Free Draw" || tool == "Line" || 
                              tool == "Rectangle" || tool == "Ellipse");

    bool sprayVisible = (tool == "Free Draw" && pattern == "Airbrush");
    m_sprayLabelAction->setVisible(sprayVisible);
    m_spraySliderAction->setVisible(sprayVisible);
}

void PaintMainWindow::refreshColorDisplayPanels() 
{
    if(!m_canvas || !m_frontColorPreview || !m_backColorPreview) return;
    QColor front = m_canvas->primaryColor();
    QColor back = m_canvas->secondaryColor();

    m_frontColorPreview->setStyleSheet(QString("background-color: %1; border: 2px solid #333;").arg(front.name()));
    m_backColorPreview->setStyleSheet(QString("background-color: %1; border: 2px solid #333;").arg(back.name()));
}

void PaintMainWindow::changeTool(QAction *senderAction) 
{
    if(!senderAction) return;

    for(QAction *action : m_toolActions) 
    {
        action->setChecked(action == senderAction);
    }

    QString toolName = senderAction->text();
    m_canvas->setTool(toolName);
    updateVisibilityStates();

    Qt::CursorShape targetShape = Qt::ArrowCursor;

    if(toolName == "Free Draw" || toolName == "Eraser" || toolName == "Line" || 
       toolName == "Rectangle" || toolName == "Ellipse" || toolName == "Bucket Fill") 
    {
        targetShape = Qt::CrossCursor;
    }
    else if(toolName == "Text") 
    {
        targetShape = Qt::IBeamCursor;
    }

    // Clear out any previous override cursor stacks so they don't pile up in memory
    while(QGuiApplication::overrideCursor() != nullptr) 
    {
        QGuiApplication::restoreOverrideCursor();
    }

    // Force the application to swap the icon instantly
    QGuiApplication::setOverrideCursor(QCursor(targetShape));

    // Fallback assignments to ensure layout consistency
    m_canvas->setCursor(targetShape);
    if(m_view) 
    {
        m_view->setCursor(targetShape);
        m_view->viewport()->update();
    }
}

void PaintMainWindow::keyPressEvent(QKeyEvent *event) 
{
    if(event->modifiers() != Qt::NoModifier) 
    {
        QMainWindow::keyPressEvent(event);
        return;
    }

    QString toolTarget = "";

    switch (event->key()) 
    {
        case Qt::Key_B: toolTarget = "Free Draw"; break;
        case Qt::Key_E: toolTarget = "Eraser"; break;
        case Qt::Key_L: toolTarget = "Line"; break;
        case Qt::Key_R: toolTarget = "Rectangle"; break;
        case Qt::Key_O: toolTarget = "Ellipse"; break;
        case Qt::Key_G: toolTarget = "Bucket Fill"; break;
        case Qt::Key_T: toolTarget = "Text"; break;
        case Qt::Key_S: toolTarget = "Select"; break;
        case Qt::Key_W: toolTarget = "Magic Wand"; break;
        case Qt::Key_D: toolTarget = "Eyedropper"; break;
        default: break;
    }

    if(!toolTarget.isEmpty()) 
    {
        for(QAction *action : m_toolActions) 
        {
            if(action->text() == toolTarget) 
            {
                changeTool(action);
                statusBar()->showMessage(QString("Hotkey Switched Tool: %1").arg(toolTarget), 1500);
                break;
            }
        }
    } 
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

void PaintMainWindow::zoomIn() 
{
    if(m_zoomLevel >= 8.0) return;

    m_zoomLevel *= 1.25;
    m_view->setTransform(QTransform().scale(m_zoomLevel, m_zoomLevel));
    updateZoomLabel();
}

void PaintMainWindow::zoomOut() 
{
    if(m_zoomLevel <= 0.15) return;

    m_zoomLevel /= 1.25;
    m_view->setTransform(QTransform().scale(m_zoomLevel, m_zoomLevel));
    updateZoomLabel();
}

void PaintMainWindow::zoomReset() 
{
    m_zoomLevel = 1.0;
    m_view->resetTransform();
    updateZoomLabel();
}

void PaintMainWindow::updateZoomLabel() 
{
    m_zoomLabel->setText(QString("Zoom: %1%").arg(static_cast<int>(m_zoomLevel * 100)));
}

void PaintMainWindow::updateStatusBarCoords(const QPoint &pos) 
{
    m_coordsLabel->setText(QString("Coordinates: %1, %2 px").arg(pos.x()).arg(pos.y()));
}

void PaintMainWindow::chooseFrontColor() 
{
    QColor color = QColorDialog::getColor(m_canvas->primaryColor(), this, "Select Front Color");

    if(color.isValid()) m_canvas->setPrimaryColor(color);
}

void PaintMainWindow::chooseBackColor() 
{
    QColor color = QColorDialog::getColor(m_canvas->secondaryColor(), this, "Select Back Color");

    if(color.isValid()) m_canvas->setSecondaryColor(color);
}

void PaintMainWindow::fileNew() 
{
    if(!maybeSave()) return;

    QDialog dialog(this);
    dialog.setWindowTitle("New Canvas Dimensions");
    dialog.setModal(true);

    QFormLayout form(&dialog);
    form.addRow(new QLabel("Configure structural pixels layout specifications:"));

    QSpinBox *widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(100, 5000);
    widthSpin->setValue(800);
    widthSpin->setSuffix(" px");
    form.addRow("Width:", widthSpin);

    QSpinBox *heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(100, 5000);
    heightSpin->setValue(600);
    heightSpin->setSuffix(" px");
    form.addRow("Height:", heightSpin);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, [&dialog]() { dialog.accept(); });
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, [&dialog]() { dialog.reject(); });

    if(dialog.exec() == QDialog::Accepted) 
    {
        int w = widthSpin->value();
        int h = heightSpin->value();

        m_canvas->createNewCanvas(w, h);
        m_currentFilePath = "";
        statusBar()->showMessage(QString("Created fresh canvas: %1 x %2 px").arg(w).arg(h), 3000);
    }
}

void PaintMainWindow::fileResizeCanvas() 
{
    QSize currentSize = m_canvas->imageSize();
    QDialog dialog(this);
    dialog.setWindowTitle("Resize Active Canvas Layout");
    dialog.setModal(true);

    QFormLayout form(&dialog);
    form.addRow(new QLabel("Expands/shrinks canvas border frame boundaries non-destructively:"));

    QSpinBox *widthSpin = new QSpinBox(&dialog);
    widthSpin->setRange(100, 5000);
    widthSpin->setValue(currentSize.width());
    widthSpin->setSuffix(" px");
    form.addRow("Target Width:", widthSpin);

    QSpinBox *heightSpin = new QSpinBox(&dialog);
    heightSpin->setRange(100, 5000);
    heightSpin->setValue(currentSize.height());
    heightSpin->setSuffix(" px");
    form.addRow("Target Height:", heightSpin);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, [&dialog]() { dialog.accept(); });
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, [&dialog]() { dialog.reject(); });

    if(dialog.exec() == QDialog::Accepted) 
    {
        m_canvas->resizeCanvasGeometry(widthSpin->value(), heightSpin->value());
        statusBar()->showMessage("Canvas boundaries reallocated successfully.", 3000);
    }
}

void PaintMainWindow::fileOpen() 
{
    if(!maybeSave()) return;

    QString filePath = QFileDialog::getOpenFileName(this, "Open Image File", "", "Images (*.png *.jpg *.bmp *.tiff)");

    if(!filePath.isEmpty() && m_canvas->openImage(filePath)) 
    {
        m_currentFilePath = filePath;
        syncSceneBounds();
    }
}

void PaintMainWindow::fileSave() 
{
    if(!m_currentFilePath.isEmpty()) m_canvas->saveImage(m_currentFilePath);
    else fileSaveAs();
}

void PaintMainWindow::fileSaveAs() 
{
    QString selectedFilter;
    QString filePath = QFileDialog::getSaveFileName(
        this, 
        "Save Image As", 
        m_currentFilePath, 
        "PNG Image (*.png);;JPEG Image (*.jpg);;Bitmap Image (*.bmp);;TIFF (*.tiff)",
        &selectedFilter
    );

    if(filePath.isEmpty()) return;

    // Verify if the user manually typed an extension. If missing, add it
    QFileInfo fileInfo(filePath);

    if(fileInfo.suffix().isEmpty()) 
    {
        if(selectedFilter.contains("*.png")) 
        {
            filePath += ".png";
        }
        else if(selectedFilter.contains("*.jpg")) 
        {
            filePath += ".jpg";
        }
        else if(selectedFilter.contains("*.bmp")) 
        {
            filePath += ".bmp";
        }
        else if(selectedFilter.contains("*.tiff")) 
        {
            filePath += ".tiff";
        }
    }

    if(m_canvas->saveImage(filePath)) 
    {
        m_currentFilePath = filePath;
        statusBar()->showMessage("Image saved successfully.", 3000);
    }
    else
    {
        QMessageBox::critical(this, "Error", "Could not save image to target destination.");
    }
}

void PaintMainWindow::editCopy() 
{
    QPixmap pixmap = m_canvas->copySelection();

    if(!pixmap.isNull()) QApplication::clipboard()->setPixmap(pixmap);
    else QMessageBox::information(this, "No selection", "There is no current selection to copy.");
}

void PaintMainWindow::editPaste() 
{
    QPixmap pixmap = QApplication::clipboard()->pixmap();

    if(!pixmap.isNull()) m_canvas->pasteImage(pixmap);
    else QMessageBox::information(this, "Clipboard empty", "There is no image in the clipboard.");
}

void PaintMainWindow::editUndo() 
{
    m_canvas->undo();
}

void PaintMainWindow::editRedo() 
{
    m_canvas->redo();
}

void PaintMainWindow::editCrop() 
{
    m_canvas->cropToSelection();
}

void PaintMainWindow::syncSceneBounds() 
{
    m_scene->setSceneRect(0, 0, m_canvas->imageSize().width(), m_canvas->imageSize().height());
    zoomReset();
}

void PaintMainWindow::filterInvert() 
{
    m_canvas->applyFilter(PaintCanvas::Invert);
    statusBar()->showMessage("Applied invert filter.", 2000);
}

void PaintMainWindow::filterGreyscale() 
{
    m_canvas->applyFilter(PaintCanvas::Greyscale);
    statusBar()->showMessage("Applied greyscale filter.", 2000);
}

void PaintMainWindow::filterBlur() 
{
    m_canvas->applyFilter(PaintCanvas::Blur);
    statusBar()->showMessage("Applied blur filter.", 2000);
}

void PaintMainWindow::filterSepia() 
{
    m_canvas->applyFilter(PaintCanvas::Sepia);
    statusBar()->showMessage("Applied vintage sepia look.", 2000);
}

void PaintMainWindow::filterBrightness() 
{
    m_canvas->applyFilter(PaintCanvas::Brightness);
    statusBar()->showMessage("Boosted color channel brightness.", 2000);
}

void PaintMainWindow::helpShortcuts() 
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Keyboard Shortcuts");
    msgBox.setTextFormat(Qt::MarkdownText);
    
    msgBox.setText(
        "| Shortcut | Action |\n"
        "| :--- | :--- |\n"
        "| **B** | Free draw (Brush) |\n"
        "| **E** | Eraser node path |\n"
        "| **L** | Straigth line draw |\n"
        "| **R** | Rectangle draw |\n"
        "| **O** | Oval/ellipse draw |\n"
        "| **G** | Bucket flood fill |\n"
        "| **T** | Text draw |\n"
        "| **S** | Lasso selection tool |\n"
        "| **W** | Magic wand pixel selection |\n"
        "| **D** | Eyedropper color pixer |\n"
        "| **Shift + Drag** | Snap lines to straight axis / Constrain shapes 1:1 |\n"
        "| **Ctrl + C** | Copy selection |\n"
        "| **Ctrl + V** | Paste from clipboard |\n"
    );
    
    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

void PaintMainWindow::helpAbout() 
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("About Paint");
    msgBox.setTextFormat(Qt::MarkdownText);

    msgBox.setText(
        "Advanced paint program built using **Qt 6**.\n\n"
        "Tailored to run seamlessly across traditional GNU/Linux host environments, LaylaOS, and "
        "custom bare-metal hobby operating system platforms."
    );

    msgBox.setIcon(QMessageBox::Information);
    msgBox.exec();
}

