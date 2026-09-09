#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QPixmap>
#include <QImageReader>
#include <QMessageBox>
#include <QEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeySequence>
#include <QTimer>
#include <QPainter>
#include <QPen>
#include <QImageWriter>
#include <QSlider>
#include <QFormLayout>

#include <cmath>
#include <algorithm>

#define APPICON_PATH            "/usr/share/gui/icons/image.png"

class ImageViewer : public QObject
{
public:
    ImageViewer();
    void show() { mainWindow->show(); }
    void openFile(const QString& filePath) { if (!filePath.isEmpty()) loadImage(filePath); }
    ~ImageViewer() { delete mainWindow; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QMainWindow* mainWindow;
    QLabel* imageLabel;
    QScrollArea* scrollArea;
    QTimer* smoothRenderTimer;
    
    QImage rawImage;          // RAM-backed master copy
    QSize targetDisplaySize;  // Current mathematical target size
    
    double zoomFactor;
    bool fitToWindow;
    QString currentFilePath;

    bool isPanning;
    QPoint lastMousePos;

    double rotationAngle = 0.0;
    bool isFlippedHorizontal = false;
    bool isFlippedVertical = false;

    bool isCroppingMode = false;
    bool isSelectingCrop = false;
    QPoint cropStartPos;
    QPoint cropEndPos;
    QRect cropSelectionRect;

    int brightnessValue = 0; // Ranges from -100 to 100
    int contrastValue = 0;   // Ranges from -100 to 100
    QWidget* adjustmentsWidget = nullptr;

    QAction* adjustmentsAct = nullptr;
    QAction* saveAct = nullptr;
    QAction* propAct = nullptr;
    QAction* cropToggleAct = nullptr;
    QAction* zoomInAct = nullptr;
    QAction* zoomOutAct = nullptr;
    QAction* normalSizeAct = nullptr;
    QAction* fitAct = nullptr;
    QAction* fitWidthAct = nullptr;
    QAction* fitHeightAct = nullptr;

    void createActions();
    void loadImage(const QString& fileName);
    void navigateToSiblingImage(int offset);
    void scaleImage(double factor);
    void updateImageDisplay();
    void renderOptimizedImage(Qt::TransformationMode mode);
    void updateWindowTitle();
    void applyImageCrop();
    void saveTransformedImage(const QString& path);
    void openAdjustmentsPanel();
    QSize getTransformedSize() const;
};


ImageViewer::ImageViewer() : zoomFactor(1.0), fitToWindow(false), isPanning(false)
{
    mainWindow = new QMainWindow();

    updateWindowTitle();
    mainWindow->setAcceptDrops(true);
    mainWindow->setWindowIcon(QIcon(APPICON_PATH));

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

        mainWindow->resize(finalWidth, finalHeight);
    }
    else
    {
        // Fallback hard limit envelope if screen devices fail to report properties 
        mainWindow->resize(800, 600);
    }

    imageLabel = new QLabel();
    imageLabel->setBackgroundRole(QPalette::Base);
    imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    imageLabel->setScaledContents(true);

    scrollArea = new QScrollArea();
    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(imageLabel);
    scrollArea->setAlignment(Qt::AlignCenter);

    mainWindow->installEventFilter(this);
    scrollArea->viewport()->installEventFilter(this);
    scrollArea->installEventFilter(this);

    mainWindow->setCentralWidget(scrollArea);

    // Setup a high-quality render timer
    smoothRenderTimer = new QTimer(this);
    smoothRenderTimer->setSingleShot(true);

    QObject::connect(smoothRenderTimer, &QTimer::timeout, [this]() {
        renderOptimizedImage(Qt::SmoothTransformation);
    });

    createActions();
}

bool ImageViewer::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == mainWindow)
    {
        if(event->type() == QEvent::DragEnter)
        {
            auto* dragEvent = static_cast<QDragEnterEvent*>(event);

            if(dragEvent->mimeData()->hasUrls())
            {
                dragEvent->acceptProposedAction();
                return true;
            }
        }
        else if(event->type() == QEvent::Drop)
        {
            auto* dropEvent = static_cast<QDropEvent*>(event);

            if(dropEvent->mimeData()->hasUrls() && !dropEvent->mimeData()->urls().isEmpty())
            {
                QString localPath = dropEvent->mimeData()->urls().first().toLocalFile();

                if(!localPath.isEmpty()) loadImage(localPath);

                dropEvent->acceptProposedAction();
                return true;
            }
        }
    }

    if(event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);

        if(keyEvent->key() == Qt::Key_Left)
        {
            navigateToSiblingImage(-1);
            return true;
        }
        else if(keyEvent->key() == Qt::Key_Right)
        {
            navigateToSiblingImage(1);
            return true;
        }
    }

    if(watched == scrollArea && event->type() == QEvent::Resize)
    {
        if (fitToWindow) updateImageDisplay();
        return false;
    }

    if(watched == scrollArea->viewport())
    {
        if(rawImage.isNull()) return false;

        switch(event->type())
        {
            case QEvent::MouseButtonPress:
            {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);

                if(mouseEvent->button() == Qt::LeftButton)
                {
                    if(isCroppingMode)
                    {
                        isSelectingCrop = true;
                        cropStartPos = mouseEvent->position().toPoint();
                        cropEndPos = cropStartPos;
                        cropSelectionRect = QRect(cropStartPos, cropEndPos);
                    }
                    else
                    {
                        isPanning = true;
                        lastMousePos = mouseEvent->globalPosition().toPoint();
                        scrollArea->setCursor(Qt::ClosedHandCursor);
                    }
                    return true;
                }
                break;
            }

            case QEvent::MouseButtonRelease:
            {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);

                if(mouseEvent->button() == Qt::LeftButton)
                {
                    if(isCroppingMode && isSelectingCrop)
                    {
                        isSelectingCrop = false;
                        cropEndPos = mouseEvent->position().toPoint();
                        cropSelectionRect = QRect(cropStartPos, cropEndPos).normalized();

                        // Ask confirmation to cut the pixel boundaries
                        if(cropSelectionRect.width() > 5 && cropSelectionRect.height() > 5)
                        {
                            auto reply = QMessageBox::question(mainWindow, "Apply Crop", 
                                "Do you want to crop the image to the selected area?", 
                                QMessageBox::Yes | QMessageBox::No);
                            if(reply == QMessageBox::Yes)
                            {
                                applyImageCrop();
                            }
                        }

                        // Clear bounding lines and reset
                        cropSelectionRect = QRect();
                        renderOptimizedImage(Qt::SmoothTransformation);
                    }
                    else if(isPanning)
                    {
                        isPanning = false;
                        scrollArea->setCursor(Qt::ArrowCursor);
                        smoothRenderTimer->start(150); 
                    }
                    return true;
                }
                break;
            }

            case QEvent::MouseMove:
            {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);

                if(isCroppingMode && isSelectingCrop)
                {
                    cropEndPos = mouseEvent->position().toPoint();
                    cropSelectionRect = QRect(cropStartPos, cropEndPos).normalized();
                    renderOptimizedImage(Qt::FastTransformation); // Forces repaint to draw selection lines
                }
                else if(isPanning)
                {
                    QPoint currentMousePos = mouseEvent->globalPosition().toPoint();
                    QPoint delta = currentMousePos - lastMousePos;
                    lastMousePos = currentMousePos;
                    scrollArea->horizontalScrollBar()->setValue(scrollArea->horizontalScrollBar()->value() - delta.x());
                    scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->value() - delta.y());
                    renderOptimizedImage(Qt::FastTransformation);
                }
                return true;
            }

            case QEvent::Wheel:
            {
                auto* wheelEvent = static_cast<QWheelEvent*>(event);
                if(wheelEvent->angleDelta().y() > 0) scaleImage(1.15);
                else scaleImage(0.85);
                return true;
            }

            default:
                break;
        }
    }

    return QObject::eventFilter(watched, event);
}

void ImageViewer::createActions()
{
    // File menu actions
    QMenu* fileMenu = mainWindow->menuBar()->addMenu("&File");

    QAction* openAct = fileMenu->addAction("&Open...", [this]()
    {
        QString fileName = QFileDialog::getOpenFileName(mainWindow, "Open Image", QString(), "Images (*.png *.jpg *.jpeg *.bmp *.gif)");
        if (!fileName.isEmpty()) loadImage(fileName);
    });
    openAct->setShortcut(QKeySequence::Open);

    saveAct = fileMenu->addAction("&Save As...", [this]()
    {
        if(rawImage.isNull()) return;
        QString savePath = QFileDialog::getSaveFileName(
            mainWindow, "Save Image As", currentFilePath, 
            "PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp)"
        );
        if(!savePath.isEmpty())
        {
            saveTransformedImage(savePath);
        }
    });
    saveAct->setShortcut(QKeySequence::SaveAs);
    saveAct->setEnabled(false);

    fileMenu->addSeparator();

    propAct = fileMenu->addAction("&Properties", [this]()
    {
        if(currentFilePath.isEmpty()) return;
        QFileInfo info(currentFilePath);
        QString msg = QString("<b>Path:</b> %1<br>"
                              "<b>Dimensions:</b> %2 x %3 px<br>"
                              "<b>Type:</b> %4<br>"
                              "<b>Size:</b> %5 KB")
                      .arg(info.absoluteFilePath())
                      .arg(rawImage.width()).arg(rawImage.height())
                      .arg(info.suffix().toUpper())
                      .arg(QString::number(info.size() / 1024.0, 'f', 2));
        QMessageBox::information(mainWindow, "Image Properties", msg);
    });
    propAct->setShortcuts({QKeySequence(Qt::ALT | Qt::Key_Enter), QKeySequence(Qt::ALT | Qt::Key_Return)});
    propAct->setEnabled(false);

    fileMenu->addSeparator();

    QAction* exitAct = fileMenu->addAction("E&xit", [this]() { mainWindow->close(); });
    exitAct->setShortcut(QKeySequence::Quit);

    // View menu actions
    QMenu* viewMenu = mainWindow->menuBar()->addMenu("&View");

    zoomInAct = viewMenu->addAction("Zoom &In (15%)", [this]()
    {
        scaleImage(1.15);
    });
    zoomInAct->setShortcuts({QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    zoomInAct->setEnabled(false);

    zoomOutAct = viewMenu->addAction("Zoom &Out (15%)", [this]()
    {
        scaleImage(0.85);
    });
    zoomOutAct->setShortcuts({QKeySequence::ZoomOut});
    zoomOutAct->setEnabled(false);

    normalSizeAct = viewMenu->addAction("&Normal Size", [this]()
    {
        fitToWindow = false;
        zoomFactor = 1.0;
        updateImageDisplay();
    });
    normalSizeAct->setShortcut(Qt::CTRL | Qt::Key_0);
    normalSizeAct->setEnabled(false);

    viewMenu->addSeparator();

    fitAct = viewMenu->addAction("&Fit to Window", [this](bool checked)
    {
        fitToWindow = checked;
        if(fitToWindow) scrollArea->setWidgetResizable(false);
        updateImageDisplay();
    });
    fitAct->setCheckable(true);
    fitAct->setChecked(true);
    fitAct->setShortcut(Qt::CTRL | Qt::Key_F);

    fitWidthAct = viewMenu->addAction("Fit to Window &Width", [this]()
    {
        if(rawImage.isNull()) return;
        fitToWindow = false;
        zoomFactor = static_cast<double>(scrollArea->viewport()->width()) / getTransformedSize().width();
        updateImageDisplay();
    });
    fitWidthAct->setShortcut(Qt::CTRL | Qt::Key_W);
    fitWidthAct->setEnabled(false);

    fitHeightAct = viewMenu->addAction("Fit to Window &Height", [this]()
    {
        if(rawImage.isNull()) return;
        fitToWindow = false;
        zoomFactor = static_cast<double>(scrollArea->viewport()->height()) / getTransformedSize().height();
        updateImageDisplay();
    });
    fitHeightAct->setShortcut(Qt::CTRL | Qt::Key_H);
    fitHeightAct->setEnabled(false);

    // Edit menu actions
    QMenu* editMenu = mainWindow->menuBar()->addMenu("&Edit");

    QAction* flipHAct = editMenu->addAction("Flip &Horizontally", [this]()
    {
        isFlippedHorizontal = !isFlippedHorizontal;
        updateImageDisplay();
    });
    flipHAct->setShortcut(Qt::CTRL | Qt::Key_L);

    QAction* flipVAct = editMenu->addAction("Flip &Vertically", [this]()
    {
        isFlippedVertical = !isFlippedVertical;
        updateImageDisplay();
    });
    flipVAct->setShortcut(Qt::CTRL | Qt::Key_M);

    QAction* rotLAct = editMenu->addAction("Rotate 90° &Left", [this]()
    {
        rotationAngle = std::fmod(rotationAngle - 90.0 + 360.0, 360.0);
        updateImageDisplay();
    });
    rotLAct->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Left);

    QAction* rotRAct = editMenu->addAction("Rotate 90° &Right", [this]()
    {
        rotationAngle = std::fmod(rotationAngle + 90.0, 360.0);
        updateImageDisplay();
    });
    rotRAct->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_Right);

    editMenu->addSeparator();

    adjustmentsAct = editMenu->addAction("Color &Adjustments...", [this]()
    {
        openAdjustmentsPanel();
    });
    adjustmentsAct->setEnabled(false);

    editMenu->addSeparator();

    cropToggleAct = editMenu->addAction("Enable &Crop Selection", [this](bool checked)
    {
        isCroppingMode = checked;

        if(!isCroppingMode)
        {
            isSelectingCrop = false;
            cropSelectionRect = QRect();
            renderOptimizedImage(Qt::SmoothTransformation); // Refresh view to clear overlays
        }

        scrollArea->setCursor(isCroppingMode ? Qt::CrossCursor : Qt::ArrowCursor);
    });
    cropToggleAct->setCheckable(true);
    cropToggleAct->setShortcut(Qt::CTRL | Qt::Key_K);
    cropToggleAct->setEnabled(false);

    // Help menu actions
    QMenu* helpMenu = mainWindow->menuBar()->addMenu("&Help");

    QAction *shortcutsAction = helpMenu->addAction("Keyboard &Shortcuts");
    QAction *aboutAction = helpMenu->addAction("&About...");

    QObject::connect(shortcutsAction, &QAction::triggered, this, [this]()
    {
        QMessageBox msgBox(mainWindow);
        msgBox.setWindowTitle("Keyboard Shortcuts");
        msgBox.setTextFormat(Qt::MarkdownText);
    
        msgBox.setText(
            "| Shortcut | Action |\n"
            "| :--- | :--- |\n"
            "| **Ctrl + +** | Zoom in |\n"
            "| **Ctrl + -** | Zoom out |\n"
            "| **Ctrl + 0** | Normal size |\n"
            "| **Ctrl + F** | Fit to window |\n"
            "| **Ctrl + H** | Fit to height |\n"
            "| **Ctrl + K** | Enable crop selection |\n"
            "| **Ctrl + L** | Flip horizontally |\n"
            "| **Ctrl + M** | Flip vertically |\n"
            "| **Ctrl + O** | Open image |\n"
            "| **Ctrl + Q** | Exit the viewer |\n"
            "| **Ctrl + W** | Fit to width |\n"
            "| **Ctrl + Shift + S** | Save image as... |\n"
            "| **Ctrl + Shift + Left** | Rotate 90° left |\n"
            "| **Ctrl + Shift + Right** | Rotate 90° right |\n"
            "| **Alt + Enter** | Show image properties |\n"
        );
    
        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });

    QObject::connect(aboutAction, &QAction::triggered, this, [this]()
    {
        QMessageBox msgBox(mainWindow);
        msgBox.setWindowTitle("About Image Viewer");
        msgBox.setTextFormat(Qt::MarkdownText);

        msgBox.setText(
            "Simple image viewer built using **Qt 6**.\n\n"
            "Tailored to run seamlessly across traditional GNU/Linux host environments, LaylaOS, and "
            "custom bare-metal hobby operating system platforms."
        );

        msgBox.setIcon(QMessageBox::Information);
        msgBox.exec();
    });
}

void ImageViewer::loadImage(const QString& fileName)
{
    // Use QImageReader to check sizes before loading into RAM
    QImageReader reader(fileName);
    reader.setAutoTransform(true); // Correct orientation automatically

    if(reader.canRead())
    {
        rawImage = reader.read();
        currentFilePath = fileName;
        fitToWindow = true;
        scrollArea->setWidgetResizable(false);
        zoomFactor = 1.0;

        // Reset geometric transform state metrics for new files
        rotationAngle = 0.0;
        isFlippedHorizontal = false;
        isFlippedVertical = false;

        updateImageDisplay();

        zoomInAct->setEnabled(true);
        zoomOutAct->setEnabled(true);
        normalSizeAct->setEnabled(true);
        fitWidthAct->setEnabled(true);
        fitHeightAct->setEnabled(true);
        saveAct->setEnabled(true);
        propAct->setEnabled(true);
        cropToggleAct->setEnabled(true);
        cropToggleAct->setChecked(false);

        isCroppingMode = false;
        isSelectingCrop = false;
        cropSelectionRect = QRect();

        adjustmentsAct->setEnabled(true);
        brightnessValue = 0;
        contrastValue = 0;

        if(adjustmentsWidget)
        {
            adjustmentsWidget->close(); // Reset open panels when swapping files
        }
    }
    else
    {
        QMessageBox::information(mainWindow, "Error", "Cannot load image.");
    }
}

void ImageViewer::navigateToSiblingImage(int offset)
{
    if(currentFilePath.isEmpty()) return;

    QFileInfo currentInfo(currentFilePath);
    QDir directory = currentInfo.dir();
    QStringList fileList = directory.entryList({"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"}, QDir::Files, QDir::Name);
    if(fileList.size() <= 1) return;

    int currentIndex = fileList.indexOf(currentInfo.fileName());
    if(currentIndex == -1) return;

    int nextIndex = (currentIndex + offset + fileList.size()) % fileList.size();
    loadImage(directory.filePath(fileList.at(nextIndex)));
}

void ImageViewer::scaleImage(double factor)
{
    fitToWindow = false;
    zoomFactor *= factor;
    zoomFactor = std::max(0.05, std::min(zoomFactor, 10.0)); // Boundary protections
    updateImageDisplay();
}

QSize ImageViewer::getTransformedSize() const
{
    if(rawImage.isNull()) return QSize();

    // Swap dimensions if the image is turned sideways
    if(std::abs(std::fmod(rotationAngle, 180.0)) > 45.0)
    {
        return QSize(rawImage.height(), rawImage.width());
    }

    return rawImage.size();
}

void ImageViewer::updateImageDisplay()
{
    if(rawImage.isNull()) return;

    QSize transformedSize = getTransformedSize();

    if(fitToWindow)
    {
        QSize viewSize = scrollArea->viewport()->size();
        targetDisplaySize = transformedSize;
        targetDisplaySize.scale(viewSize, Qt::KeepAspectRatio);
        fitAct->setChecked(true);
    }
    else
    {
        targetDisplaySize = transformedSize * zoomFactor;
        fitAct->setChecked(false);
    }

    renderOptimizedImage(Qt::FastTransformation);
    updateWindowTitle();
    smoothRenderTimer->start(200);
}

// Scale raw data down BEFORE painting to the display label
void ImageViewer::renderOptimizedImage(Qt::TransformationMode mode)
{
    if(rawImage.isNull() || !targetDisplaySize.isValid()) return;

    // Build standard transformation matrix
    QTransform transform;
    transform.scale(isFlippedHorizontal ? -1 : 1, isFlippedVertical ? -1 : 1);
    transform.rotate(rotationAngle);

    // Apply geometric matrices and downsample
    QImage modifiedImg = rawImage.transformed(transform, mode);
    QImage scaledImg = modifiedImg.scaled(targetDisplaySize, Qt::IgnoreAspectRatio, mode);

    // Inject Real-time Color Adjustment Filters (Only process if values deviate from default)
    if(brightnessValue != 0 || contrastValue != 0)
    {
        // Enforce 32-bit color mapping formatting for direct fast pixel manipulation
        if(scaledImg.format() != QImage::Format_ARGB32 && scaledImg.format() != QImage::Format_RGB32)
        {
            scaledImg = scaledImg.convertToFormat(QImage::Format_ARGB32);
        }

        // Generate a Contrast Lookup Table (LUT) to reduce Math execution overhead
        // Contrast formula mapping: factor = (259 * (C + 255)) / (255 * (259 - C))
        double factor = (259.0 * (contrastValue + 255.0)) / (255.0 * (259.0 - contrastValue));
        int contrastLUT[256];

        for(int i = 0; i < 256; ++i)
        {
            int val = static_cast<int>(std::round(factor * (i - 128) + 128 + brightnessValue));
            contrastLUT[i] = std::clamp(val, 0, 255);
        }

        // Directly parse memory scanlines for performance
        for(int y = 0; y < scaledImg.height(); ++y)
        {
            QRgb* line = reinterpret_cast<QRgb*>(scaledImg.scanLine(y));
            for(int x = 0; x < scaledImg.width(); ++x)
            {
                int r = contrastLUT[qRed(line[x])];
                int g = contrastLUT[qGreen(line[x])];
                int b = contrastLUT[qBlue(line[x])];
                line[x] = qRgba(r, g, b, qAlpha(line[x]));
            }
        }
    }

    // Paint modified context canvas layers to the view pane    
    imageLabel->setPixmap(QPixmap::fromImage(scaledImg));
    imageLabel->resize(targetDisplaySize);

    // Draw selection box if cropping is active
    if(isCroppingMode && !cropSelectionRect.isNull())
    {
        QPixmap currentPixmap = imageLabel->pixmap();

        if(!currentPixmap.isNull())
        {
            QPainter painter(&currentPixmap);
            QPen pen(Qt::DashLine);
            pen.setWidth(2);
            pen.setColor(Qt::cyan);
            painter.setPen(pen);

            // Map viewport points directly back onto the dynamically scaled pixmap surface
            QPoint labelStart = imageLabel->mapFrom(scrollArea->viewport(), cropSelectionRect.topLeft());
            QPoint labelEnd = imageLabel->mapFrom(scrollArea->viewport(), cropSelectionRect.bottomRight());
            QRect drawRect(labelStart, labelEnd);

            painter.drawRect(drawRect);
            painter.end();
            imageLabel->setPixmap(currentPixmap);
        }
    }
}

void ImageViewer::applyImageCrop()
{
    if(rawImage.isNull() || cropSelectionRect.isEmpty()) return;

    // Gather full pipeline transformations
    QTransform transform;
    transform.scale(isFlippedHorizontal ? -1 : 1, isFlippedVertical ? -1 : 1);
    transform.rotate(rotationAngle);
    QImage workingCopy = rawImage.transformed(transform, Qt::SmoothTransformation);

    // Map coordinates backward from display layout down into raw data pixels
    double scaleX = static_cast<double>(workingCopy.width()) / targetDisplaySize.width();
    double scaleY = static_cast<double>(workingCopy.height()) / targetDisplaySize.height();

    QPoint labelTopLeft = imageLabel->mapFrom(scrollArea->viewport(), cropSelectionRect.topLeft());
    
    int cropX = std::clamp(static_cast<int>(std::round(labelTopLeft.x() * scaleX)), 0, workingCopy.width());
    int cropY = std::clamp(static_cast<int>(std::round(labelTopLeft.y() * scaleY)), 0, workingCopy.height());
    int cropW = std::clamp(static_cast<int>(std::round(cropSelectionRect.width() * scaleX)), 1, workingCopy.width() - cropX);
    int cropH = std::clamp(static_cast<int>(std::round(cropSelectionRect.height() * scaleY)), 1, workingCopy.height() - cropY);

    // Slice the coordinates out and save back to the master container
    rawImage = workingCopy.copy(cropX, cropY, cropW, cropH);

    // Reset global spatial state properties (they are now baked into rawImage)
    rotationAngle = 0.0;
    isFlippedHorizontal = false;
    isFlippedVertical = false;
    isCroppingMode = false;
    cropToggleAct->setChecked(false);
    scrollArea->setCursor(Qt::ArrowCursor);

    updateImageDisplay();
}

void ImageViewer::saveTransformedImage(const QString& path)
{
    // Bake current transforms to match what is visible on screen
    QTransform transform;
    transform.scale(isFlippedHorizontal ? -1 : 1, isFlippedVertical ? -1 : 1);
    transform.rotate(rotationAngle);
    QImage finalImage = rawImage.transformed(transform, Qt::SmoothTransformation);

    // Apply permanent color changes onto the high-resolution master copy before saving
    if(brightnessValue != 0 || contrastValue != 0)
    {
        finalImage = finalImage.convertToFormat(QImage::Format_ARGB32);
        double factor = (259.0 * (contrastValue + 255.0)) / (255.0 * (259.0 - contrastValue));
        int contrastLUT[256];

        for(int i = 0; i < 256; ++i)
        {
            int val = static_cast<int>(std::round(factor * (i - 128) + 128 + brightnessValue));
            contrastLUT[i] = std::clamp(val, 0, 255);
        }

        for(int y = 0; y < finalImage.height(); ++y)
        {
            QRgb* line = reinterpret_cast<QRgb*>(finalImage.scanLine(y));
            for(int x = 0; x < finalImage.width(); ++x)
            {
                line[x] = qRgba(contrastLUT[qRed(line[x])], contrastLUT[qGreen(line[x])], contrastLUT[qBlue(line[x])], qAlpha(line[x]));
            }
        }
    }

    QImageWriter writer(path);
    writer.setQuality(95); // High-quality storage preservation configuration
    
    if(writer.write(finalImage))
    {
        currentFilePath = path;
        updateWindowTitle();
        QMessageBox::information(mainWindow, "Success", "Image saved successfully.");
    }
    else
    {
        QMessageBox::critical(mainWindow, "Error", QString("Failed to save image: %1").arg(writer.errorString()));
    }
}

// Spawns standalone layout adjustment floating module frame window
void ImageViewer::openAdjustmentsPanel()
{
    if(adjustmentsWidget)
    {
        adjustmentsWidget->raise();
        adjustmentsWidget->activateWindow();
        return;
    }

    adjustmentsWidget = new QWidget(mainWindow, Qt::Tool);
    adjustmentsWidget->setWindowTitle("Color Adjustments");
    adjustmentsWidget->setAttribute(Qt::WA_DeleteOnClose);

    // Connect cleanup tracking directly to standard destroy routines
    QObject::connect(adjustmentsWidget, &QWidget::destroyed, [this]()
    {
        adjustmentsWidget = nullptr; 
    });

    QFormLayout* layout = new QFormLayout(adjustmentsWidget);

    QSlider* brightSlider = new QSlider(Qt::Horizontal);
    brightSlider->setRange(-100, 100);
    brightSlider->setValue(brightnessValue);

    QObject::connect(brightSlider, &QSlider::valueChanged, [this](int value)
    {
        brightnessValue = value;
        renderOptimizedImage(Qt::FastTransformation); // Responsive rendering while sliding
    });

    // Trigger rendering calculations when release click actions conclude
    QObject::connect(brightSlider, &QSlider::sliderReleased, [this]()
    {
        smoothRenderTimer->start(100);
    });

    QSlider* contrastSlider = new QSlider(Qt::Horizontal);
    contrastSlider->setRange(-100, 100);
    contrastSlider->setValue(contrastValue);

    QObject::connect(contrastSlider, &QSlider::valueChanged, [this](int value)
    {
        contrastValue = value;
        renderOptimizedImage(Qt::FastTransformation);
    });

    QObject::connect(contrastSlider, &QSlider::sliderReleased, [this]()
    {
        smoothRenderTimer->start(100);
    });

    layout->addRow("Brightness:", brightSlider);
    layout->addRow("Contrast:", contrastSlider);

    adjustmentsWidget->show();
}

void ImageViewer::updateWindowTitle()
{
    QString baseName = "Image Viewer";

    if(currentFilePath.isEmpty())
    {
        mainWindow->setWindowTitle(baseName);
        return;
    }

    QString fileName = QFileInfo(currentFilePath).fileName();

    if(fitToWindow)
    {
        mainWindow->setWindowTitle(QString("%1 - %2 [Fit]").arg(fileName, baseName));
    }
    else
    {
        mainWindow->setWindowTitle(QString("%1 - %2 [%3%]")
                                    .arg(fileName)
                                    .arg(baseName)
                                    .arg(std::round(zoomFactor * 100.0)));
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ImageViewer viewer;

    viewer.show();

    if (app.arguments().size() > 1) viewer.openFile(app.arguments().at(1));

    return app.exec();
}

