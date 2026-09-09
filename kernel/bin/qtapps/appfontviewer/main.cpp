#include <QApplication>
#include <QMainWindow>
#include <QSplitter>
#include <QListWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QFontDatabase>
#include <QCommandLineParser>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QCheckBox>
#include <QFileInfo>
#include <QScreen>
#include <QStatusBar>

#define APPICON_PATH            "/usr/share/gui/icons/fontviewer.png"

class FontViewerWindow : public QMainWindow
{
public:
    FontViewerWindow(const QString& explicitFontPath = QString());

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void loadSystemFonts();
    void loadExplicitFont(const QString& path);
    void updatePreview();
    void disableControls();
    void showError(const QString& err);

    QListWidget* m_fontList;
    QLineEdit* m_customTextInput;
    QPushButton* m_resetButton;
    QTextEdit* m_previewEdit;
    QSlider* m_sizeSlider;
    QLabel* m_sizeValueLabel;
    QCheckBox* m_boldCheck;
    QCheckBox* m_italicCheck;
    QString m_defaultSampleText;
};

FontViewerWindow::FontViewerWindow(const QString& explicitFontPath)
{
    setWindowTitle("Font Viewer");
    setWindowIcon(QIcon(APPICON_PATH));

    QScreen *primaryScreen = QGuiApplication::primaryScreen();

    if(primaryScreen)
    {
        QRect screenGeometry = primaryScreen->availableGeometry();
        int screenWidth = screenGeometry.width();
        int screenHeight = screenGeometry.height();

        // Propose our desired desktop application dimensions
        int proposedWidth = 600;
        int proposedHeight = 400;

        // Enforce a ceiling limit matching hardware screen limits 
        int finalWidth = std::min(proposedWidth, screenWidth - 30);
        int finalHeight = std::min(proposedHeight, screenHeight - 30);

        resize(finalWidth, finalHeight);
    }
    else
    {
        // Fallback hard limit envelope if screen devices fail to report properties 
        resize(600, 400);
    }

    m_defaultSampleText = "The quick brown fox jumps over the lazy dog. 1234567890!";

    // Core central layout widget
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // Top Control Toolbar Panel
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    mainLayout->addLayout(toolbarLayout);

    // Size control elements
    QLabel* sizeLabel = new QLabel("Font Size:", this);
    toolbarLayout->addWidget(sizeLabel);

    m_sizeSlider = new QSlider(Qt::Horizontal, this);
    m_sizeSlider->setRange(8, 72);
    m_sizeSlider->setValue(16);
    toolbarLayout->addWidget(m_sizeSlider);

    m_sizeValueLabel = new QLabel("16 pt", this);
    m_sizeValueLabel->setMinimumWidth(40);
    toolbarLayout->addWidget(m_sizeValueLabel);

    toolbarLayout->addSpacing(20);

    // Formatting toggle widgets
    m_boldCheck = new QCheckBox("Bold", this);
    toolbarLayout->addWidget(m_boldCheck);

    m_italicCheck = new QCheckBox("Italic", this);
    toolbarLayout->addWidget(m_italicCheck);

    toolbarLayout->addStretch();

    // Main content display splitter
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    mainLayout->addWidget(splitter);

    // Left layout pane (Font Selection List)
    QWidget* leftContainer = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 5, 0, 0);

    QLabel* listLabel = new QLabel("Available Fonts:", leftContainer);
    leftLayout->addWidget(listLabel);

    m_fontList = new QListWidget(leftContainer);
    leftLayout->addWidget(m_fontList);
    splitter->addWidget(leftContainer);

    // Right layout pane (Interactive Custom Inputs + Display Panel)
    QWidget* rightContainer = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 5, 0, 0);

    QLabel* inputLabel = new QLabel("Type text to preview:", rightContainer);
    rightLayout->addWidget(inputLabel);

    // Horizontal inline configuration panel tracking text inputs + structural resets
    QHBoxLayout* inputRowLayout = new QHBoxLayout();

    m_customTextInput = new QLineEdit(rightContainer);
    m_customTextInput->setPlaceholderText("Enter text to test font rendering...");
    m_customTextInput->setText(m_defaultSampleText);
    inputRowLayout->addWidget(m_customTextInput);

    m_resetButton = new QPushButton("Reset default", rightContainer);
    inputRowLayout->addWidget(m_resetButton);

    rightLayout->addLayout(inputRowLayout);
    rightLayout->addSpacing(5);

    QLabel* sampleLabel = new QLabel("Font preview:", rightContainer);
    rightLayout->addWidget(sampleLabel);

    m_previewEdit = new QTextEdit(rightContainer);
    m_previewEdit->setReadOnly(true);
    rightLayout->addWidget(m_previewEdit);
    splitter->addWidget(rightContainer);

    splitter->setSizes(QList<int>({280, 770}));

    // Bind event intercept filters
    m_sizeSlider->installEventFilter(this);
    m_customTextInput->installEventFilter(this);
    m_resetButton->installEventFilter(this);

    connect(m_fontList, &QListWidget::itemSelectionChanged, this, [this]() { updatePreview(); });
    connect(m_boldCheck, &QCheckBox::clicked, this, [this]() { updatePreview(); });
    connect(m_italicCheck, &QCheckBox::clicked, this, [this]() { updatePreview(); });

    statusBar()->setSizeGripEnabled(true);

    if(!explicitFontPath.isEmpty())
    {
        loadExplicitFont(explicitFontPath);
    }
    else
    {
        loadSystemFonts();
    }
}

bool FontViewerWindow::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == m_resetButton && event->type() == QEvent::MouseButtonRelease)
    {
        m_customTextInput->setText(m_defaultSampleText);
        updatePreview();
        return true;
    }
    else if(watched == m_customTextInput &&
               (event->type() == QEvent::KeyRelease || event->type() == QEvent::InputMethod))
    {
        updatePreview();
    }
    else if(watched == m_sizeSlider &&
               (event->type() == QEvent::MouseMove ||
                event->type() == QEvent::MouseButtonRelease ||
                event->type() == QEvent::KeyRelease))
    {
        m_sizeValueLabel->setText(QString("%1 pt").arg(m_sizeSlider->value()));
        updatePreview();
    }

    return QMainWindow::eventFilter(watched, event);
}

void FontViewerWindow::loadSystemFonts()
{
    statusBar()->setStyleSheet("QStatusBar { background-color: #E3F2FD; color: #0D47A1; font-weight: bold; }");
    statusBar()->showMessage("Viewing standard system fonts");

    QStringList fontFamilies = QFontDatabase::families();
    m_fontList->addItems(fontFamilies);

    if(m_fontList->count() > 0)
    {
        m_fontList->setCurrentRow(0);
        m_fontList->setFocus(Qt::OtherFocusReason);
        updatePreview();
    }
}

void FontViewerWindow::disableControls()
{
    m_sizeSlider->setEnabled(false);
    m_customTextInput->setEnabled(false);
    m_resetButton->setEnabled(false);
    m_boldCheck->setEnabled(false);
    m_italicCheck->setEnabled(false);
}

void FontViewerWindow::showError(const QString& err)
{
    statusBar()->showMessage(err);
    m_previewEdit->setText(err);
}

void FontViewerWindow::loadExplicitFont(const QString& path)
{
    QFileInfo fileInfo(path);
    if(!fileInfo.exists())
    {
        statusBar()->setStyleSheet("QStatusBar { background-color: #FFEBEE; color: #B71C1C; font-weight: bold; }");
        showError(QString("Error: File not found: %1").arg(path));
        disableControls();
        return;
    }

    int fontId = QFontDatabase::addApplicationFont(path);
    if(fontId == -1)
    {
        statusBar()->setStyleSheet("QStatusBar { background-color: #FFEBEE; color: #B71C1C; font-weight: bold; }");
        showError(QString("Error: Failed to load font file: %1").arg(path));
        disableControls();
        return;
    }

    QStringList loadedFamilies = QFontDatabase::applicationFontFamilies(fontId);
    if(loadedFamilies.isEmpty())
    {
        statusBar()->setStyleSheet("QStatusBar { background-color: #FFF3E0; color: #E65100; font-weight: bold; }");
        showError(QString("Error: Font has zero rendering families: %1").arg(path));
        disableControls();
        return;
    }

    statusBar()->setStyleSheet("QStatusBar { background-color: #E8F5E9; color: #1B5E20; font-weight: bold; }");
    statusBar()->showMessage(QString("Loaded custom font file -> %1").arg(fileInfo.fileName()));

    m_fontList->addItems(loadedFamilies);
    m_fontList->setCurrentRow(0);
    m_fontList->setFocus(Qt::OtherFocusReason);
    updatePreview();
}

void FontViewerWindow::updatePreview()
{
    QListWidgetItem* currentItem = m_fontList->currentItem();
    if(!currentItem) return;

    QString selectedFamily = currentItem->text();
    int targetSize = m_sizeSlider->value();

    QString displayText = m_customTextInput->text();
    if(displayText.isEmpty())
    {
        displayText = "-- Empty text - Type above to preview --";
    }

    QFont previewFont(selectedFamily, targetSize);
    previewFont.setBold(m_boldCheck->isChecked());
    previewFont.setItalic(m_italicCheck->isChecked());

    m_previewEdit->setFont(previewFont);
    m_previewEdit->setText(displayText);
}

int main(int argc, char* argv[])
{
#ifdef __laylaos__
    qputenv("FONTCONFIG_PATH", "/usr/share/fonts");
    qputenv("FONTCONFIG_FILE", "/etc/fonts/fonts.conf");
#endif

    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("Font Viewer");
    QCoreApplication::setApplicationVersion("1.4");

    QCommandLineParser parser;
    parser.setApplicationDescription("Font view tool.");
    parser.addHelpOption();
    parser.addPositionalArgument("fontfile", "Path validation input font file.", "[fontfile]");
    parser.process(app);

    const QStringList positionalArgs = parser.positionalArguments();
    QString targetFontPath = positionalArgs.isEmpty() ? QString() : positionalArgs.first();
    FontViewerWindow window(targetFontPath);

    window.show();

    return app.exec();
}

