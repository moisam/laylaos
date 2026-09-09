#include "settings_base.h"
#include "settings_single_instance_lock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QCheckBox>
#include <QPushButton>
#include <QLineEdit>
#include <QPalette>
#include <QColor>

#define APPICON_PATH            "/usr/share/gui/icons/theme.png"

const char *LOCK_FILE = "/tmp/desktop-settings-theme.lock";

struct
{
    // theme name
    char *name;

    // windows
    uint32_t themecolor[128];

} themes[] =
{
    {
      (char *)"Default",
      {
            0xEFEFEFFF,                           // window bg
            0x737373FF,                           // window title bg top color
            0x5A5A5AFF,                           // window title bg mid1 color
            0x4A4A4AFF,                           // window title bg mid2 color
            0x3A3A3AFF,                           // window title bg bottom color
            0x555555FF,                           // window title bg top color (inactive)
            0x4D4D4DFF,                           // window title bg mid1 color (inactive)
            0x484848FF,                           // window title bg mid2 color (inactive)
            0x404040FF,                           // window title bg bottom color (inactive)
            0xFFFFFFFF, 0x888888FF,               // window text
            0x2A2A2AFF,                           // window border outer color (very dark grey)
            0x4A4A4AFF,                           // window border mid color (medium dark grey)
            0x737373FF,                           // window border inner color (light grey)
            0x9E9E9EFF,                           // window border top 1px highlight
            0x3A3A3AFF,                           // window border inactive outer color (very dark grey)
            0x454545FF,                           // window border inactive mid color (medium dark grey)
            0x505050FF,                           // window border inactive inner color (light grey)
            0x656565FF,                           // window border top 1px highlight (inactive)

            0x5A5A5AFF,                           // controlbox button background
            0x7D7D7DFF,                           // controlbox button background (hover)
            0xEBF0F5FF,                           // controlbox button text
            0x4D4D4DFF,                           // controlbox button inactive background
            0x585858FF,                           // controlbox button inactive background (hover)
            0x8A9095FF,                           // controlbox button inactive text

            0x5A5A5AFF,                           // controlbox button disabled background
            0x3A3A3AFF,                           // controlbox button disabled text
            0x7A7A7AFF,                           // controlbox button disabled text shadow
            0x4D4D4DFF,                           // controlbox button disabled inactive background
            0x353535FF,                           // controlbox button disabled inactive text
            0x606060FF,                           // controlbox button disabled inactive text shadow

            0x7F7F7FFF,                           // controlbox button top & left borders
            0x3A3A3AFF,                           // controlbox button bottom & right borders
            0x9E9E9EFF,                           // controlbox button hover top & left borders
            0x4A4A4AFF,                           // controlbox button hover bottom & right borders

            0xEFEFEFFF, 0x222226FF, 0x222226FF,   // buttons
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xE0DFE3FF, 0x222226FF, 0x222226FF,
            0xEFEFEFFF, 0xBABDC4FF, 0x222226FF,
            0xEFEFEFFF, 0x222226FF,               // status bars
            0xEFEFEFFF, 0x222226FF,               // scroll bars
            0xFFFFFFFF, 0x000000FF,               // textboxes
            0xFFFFFFFF, 0x000000FF,               // inputboxes
            0x16A085FF, 0xFFFFFFFF,
            0xEFEFEFFF, 0xBABDC4FF,
            0x16A085FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
    {
      (char *)"Dark Blue",
      {
            0xEFEFEFFF,                           // window bg
            0x3A5F85FF,                           // window title bg top color
            0x24405EFF,                           // window title bg mid1 color
            0x162D45FF,                           // window title bg mid2 color
            0x0E1E30FF,                           // window title bg bottom color
            0x2E3E52FF,                           // window title bg top color (inactive)
            0x242F3DFF,                           // window title bg mid1 color (inactive)
            0x1D2633FF,                           // window title bg mid2 color (inactive)
            0x161D26FF,                           // window title bg bottom color (inactive)
            0xFFFFFFFF, 0x888888FF,               // window text
            0x09131FFF,                           // window border outer color (very dark grey)
            0x1D3550FF,                           // window border mid color (medium dark grey)
            0x36587EFF,                           // window border inner color (light grey)
            0x5C84ACFF,                           // window border top 1px highlight
            0x121A24FF,                           // window border inactive outer color (very dark grey)
            0x222C38FF,                           // window border inactive mid color (medium dark grey)
            0x323F52FF,                           // window border inactive inner color (light grey)
            0x3D526BFF,                           // window border top 1px highlight (inactive)

            0x284666FF,                           // controlbox button background
            0x385E87FF,                           // controlbox button background (hover)
            0x0E1E30FF,                           // controlbox button text
            0x242F3DFF,                           // controlbox button inactive background
            0x2C394AFF,                           // controlbox button inactive background (hover)
            0x7D8C9EFF,                           // controlbox button inactive text

            0x24405EFF,                           // controlbox button disabled background
            0x0E1E30FF,                           // controlbox button disabled text
            0x4D7299FF,                           // controlbox button disabled text shadow
            0x242F3dFF,                           // controlbox button disabled inactive background
            0x7D8C9EFF,                           // controlbox button disabled inactive text
            0x161D26FF,                           // controlbox button disabled inactive text shadow

            0x466A91FF,                           // controlbox button top & left borders
            0x11253AFF,                           // controlbox button bottom & right borders
            0x6088B3FF,                           // controlbox button hover top & left borders
            0x18324EFF,                           // controlbox button hover bottom & right borders

            0xEFEFEFFF, 0x222226FF, 0x222226FF,   // buttons
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xE0DFE3FF, 0x222226FF, 0x222226FF,
            0xEFEFEFFF, 0xBABDC4FF, 0x222226FF,
            0xEFEFEFFF, 0x222226FF,               // status bars
            0xEFEFEFFF, 0x222226FF,               // scroll bars
            0xFFFFFFFF, 0x000000FF,               // textboxes
            0xFFFFFFFF, 0x000000FF,               // inputboxes
            0x1D3550FF, 0xFFFFFFFF,
            0xEFEFEFFF, 0xBABDC4FF,
            0x337CC4FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
    {
      (char *)"Olive Green",
      {
            0xEFEFEFFF,                           // window bg
            0x666E54FF,                           // window title bg top color
            0x505740FF,                           // window title bg mid1 color
            0x404632FF,                           // window title bg mid2 color
            0x323725FF,                           // window title bg bottom color
            0x505446FF,                           // window title bg top color (inactive)
            0x43473BFF,                           // window title bg mid1 color (inactive)
            0x3A3D33FF,                           // window title bg mid2 color (inactive)
            0x31332BFF,                           // window title bg bottom color (inactive)
            0xFFFFFFFF, 0x888888FF,               // window text
            0x222619FF,                           // window border outer color (very dark grey)
            0x484E39FF,                           // window border mid color (medium dark grey)
            0x6A7358FF,                           // window border inner color (light grey)
            0x8E9979FF,                           // window border top 1px highlight
            0x262921FF,                           // window border inactive outer color (very dark grey)
            0x3D4036FF,                           // window border inactive mid color (medium dark grey)
            0x505448FF,                           // window border inactive inner color (light grey)
            0x636958FF,                           // window border top 1px highlight (inactive)

            0x525A42FF,                           // controlbox button background
            0x677054FF,                           // controlbox button background (hover)
            0xF5F6F2FF,                           // controlbox button text
            0x43473BFF,                           // controlbox button inactive background
            0x4E5245FF,                           // controlbox button inactive background (hover)
            0x7F8573FF,                           // controlbox button inactive text

            0x505740FF,                           // controlbox button disabled background
            0x282C1EFF,                           // controlbox button disabled text
            0x75805EFF,                           // controlbox button disabled text shadow
            0x43473BFF,                           // controlbox button disabled inactive background
            0x7F8573FF,                           // controlbox button disabled inactive text
            0x31332BFF,                           // controlbox button disabled inactive text shadow

            0x737C60FF,                           // controlbox button top & left borders
            0x363B29FF,                           // controlbox button bottom & right borders
            0x919C7AFF,                           // controlbox button hover top & left borders
            0x434934FF,                           // controlbox button hover bottom & right borders

            0xEFEFEFFF, 0x222226FF, 0x222226FF,   // buttons
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xE0DFE3FF, 0x222226FF, 0x222226FF,
            0xEFEFEFFF, 0xBABDC4FF, 0x222226FF,
            0xEFEFEFFF, 0x222226FF,               // status bars
            0xEFEFEFFF, 0x222226FF,               // scroll bars
            0xFFFFFFFF, 0x000000FF,               // textboxes
            0xFFFFFFFF, 0x000000FF,               // inputboxes
            0x484E39FF, 0xFFFFFFFF,
            0xEFEFEFFF, 0xBABDC4FF,
            0x337CC4FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
    {
      (char *)"Orange",
      {
            0xEFEFEFFF,                           // window bg
            0xE07234FF,                           // window title bg top color
            0xCD5619FF,                           // window title bg mid1 color
            0xB8450AFF,                           // window title bg mid2 color
            0x963400FF,                           // window title bg bottom color
            0x9E6546FF,                           // window title bg top color (inactive)
            0x8F5637FF,                           // window title bg mid1 color (inactive)
            0x7D4B2EFF,                           // window title bg mid2 color (inactive)
            0x6B3E25FF,                           // window title bg bottom color (inactive)
            0xFFFFFFFF, 0x888888FF,               // window text
            0x6B2500FF,                           // window border outer color (very dark grey)
            0xB8450AFF,                           // window border mid color (medium dark grey)
            0xE58047FF,                           // window border inner color (light grey)
            0xFA9A61FF,                           // window border top 1px highlight
            0x54321EFF,                           // window border inactive outer color (very dark grey)
            0x825134FF,                           // window border inactive mid color (medium dark grey)
            0x9E6848FF,                           // window border inactive inner color (light grey)
            0xB37B5BFF,                           // window border top 1px highlight (inactive)

            0xC25117FF,                           // controlbox button background
            0xE06524FF,                           // controlbox button background (hover)
            0xFFF0E6FF,                           // controlbox button text
            0x8F5637FF,                           // controlbox button inactive background
            0x9E6140FF,                           // controlbox button inactive background (hover)
            0xC7A28DFF,                           // controlbox button inactive text

            0xCD5619FF,                           // controlbox button disabled background
            0x7A2A00FF,                           // controlbox button disabled text
            0xD97E4DFF,                           // controlbox button disabled text shadow
            0x8F5637FF,                           // controlbox button disabled inactive background
            0xC7A28DFF,                           // controlbox button disabled inactive text
            0x54321EFF,                           // controlbox button disabled inactive text shadow

            0xE3793DFF,                           // controlbox button top & left borders
            0x9E3A04FF,                           // controlbox button bottom & right borders
            0xFC9E6AFF,                           // controlbox button hover top & left borders
            0xB54407FF,                           // controlbox button hover bottom & right borders

            0xEFEFEFFF, 0x222226FF, 0x222226FF,   // buttons
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xE0DFE3FF, 0x222226FF, 0x222226FF,
            0xEFEFEFFF, 0xBABDC4FF, 0x222226FF,
            0xEFEFEFFF, 0x222226FF,               // status bars
            0xEFEFEFFF, 0x222226FF,               // scroll bars
            0xFFFFFFFF, 0x000000FF,               // textboxes
            0xFFFFFFFF, 0x000000FF,               // inputboxes
            0xB8450AFF, 0xFFFFFFFF,
            0xEFEFEFFF, 0xBABDC4FF,
            0xFF9933FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
    {
      (char *)"Dark mode",
      {
            0x1F1F1FFF,                           // window bg
            0x737373FF,                           // window title bg top color
            0x5A5A5AFF,                           // window title bg mid1 color
            0x4A4A4AFF,                           // window title bg mid2 color
            0x3A3A3AFF,                           // window title bg bottom color
            0x555555FF,                           // window title bg top color (inactive)
            0x4D4D4DFF,                           // window title bg mid1 color (inactive)
            0x484848FF,                           // window title bg mid2 color (inactive)
            0x404040FF,                           // window title bg bottom color (inactive)
            0xFFFFFFFF, 0x888888FF,               // window text
            0x2A2A2AFF,                           // window border outer color (very dark grey)
            0x4A4A4AFF,                           // window border mid color (medium dark grey)
            0x737373FF,                           // window border inner color (light grey)
            0x9E9E9EFF,                           // window border top 1px highlight
            0x3A3A3AFF,                           // window border inactive outer color (very dark grey)
            0x454545FF,                           // window border inactive mid color (medium dark grey)
            0x505050FF,                           // window border inactive inner color (light grey)
            0x656565FF,                           // window border top 1px highlight (inactive)

            0x5A5A5AFF,                           // controlbox button background
            0x7D7D7DFF,                           // controlbox button background (hover)
            0xEBF0F5FF,                           // controlbox button text
            0x4D4D4DFF,                           // controlbox button inactive background
            0x585858FF,                           // controlbox button inactive background (hover)
            0x8A9095FF,                           // controlbox button inactive text

            0x5A5A5AFF,                           // controlbox button disabled background
            0x3A3A3AFF,                           // controlbox button disabled text
            0x7A7A7AFF,                           // controlbox button disabled text shadow
            0x4D4D4DFF,                           // controlbox button disabled inactive background
            0x353535FF,                           // controlbox button disabled inactive text
            0x606060FF,                           // controlbox button disabled inactive text shadow

            0x7F7F7FFF,                           // controlbox button top & left borders
            0x3A3A3AFF,                           // controlbox button bottom & right borders
            0x9E9E9EFF,                           // controlbox button hover top & left borders
            0x4A4A4AFF,                           // controlbox button hover bottom & right borders

            0x161616FF, 0xECF0F1FF, 0x222226FF,   // buttons
            0x16A085FF, 0xFFFFFFFF, 0x16A085FF,
            0xB4B4B8FF, 0x222226FF, 0x222226FF,
            0xE0DFE3FF, 0x222226FF, 0x222226FF,
            0x161616FF, 0xBABDC4FF, 0x222226FF,
            0x161616FF, 0x222226FF,               // status bars
            0x161616FF, 0x222226FF,               // scroll bars
            0x161616FF, 0xECF0F1FF,               // textboxes
            0x161616FF, 0xECF0F1FF,               // inputboxes
            0x16A085FF, 0xFFFFFFFF,
            0x161616FF, 0xBABDC4FF,
            0x16A085FF, 0x333333FF, 0xDDDDDDFF,   // toggle buttons
      },
    },
};

size_t theme_count = sizeof(themes) / sizeof(themes[0]);


class ThemesWindow : public SettingsBaseWindow
{
public:
    explicit ThemesWindow(QWidget *parent = nullptr);
    void updateLivePreviewCanvas();
    void sendThemeRequest();
    QString colorInHex(int activeTheme, int colorIndex);

protected:
    void changeEvent(QEvent *event) override;

private:
    QListWidget *m_themesList = nullptr;
    QFrame *m_mockWindowCanvas = nullptr;
    QLabel *m_mockTitleLabel = nullptr;
    QPushButton *m_mockButton = nullptr;
    QLineEdit *m_mockLineEdit = nullptr;
    QCheckBox *m_mockCheckBox = nullptr;
};

ThemesWindow::ThemesWindow(QWidget *parent)
    : SettingsBaseWindow("System Theme", parent)
{
    setFixedSize(530, 300);
    setWindowIcon(QIcon(APPICON_PATH));

    auto *masterLayout = new QHBoxLayout(this);
    masterLayout->setContentsMargins(15, 15, 15, 15);
    masterLayout->setSpacing(15);

    auto *leftContainer = new QWidget(this);

    auto *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    auto *listLabel = new QLabel("System Theme:", leftContainer);
    listLabel->setStyleSheet("font-weight: bold; color: #0D6C60;");
    leftLayout->addWidget(listLabel);

    m_themesList = new QListWidget(leftContainer);
    for(size_t i = 0; i < theme_count; i++) m_themesList->addItem(themes[i].name);
    m_themesList->setCurrentRow(0);
    /*
    m_themesList->setStyleSheet(
        "QListWidget { border: 1px solid #3A3A3A; border-radius: 4px; padding: 4px; }"
    );
    */
    leftLayout->addWidget(m_themesList, 1);

    auto *applyBtn = new QPushButton("Apply Selected Theme", leftContainer);
    applyBtn->setFixedSize(200, 32);
    leftLayout->addWidget(applyBtn);

    masterLayout->addWidget(leftContainer, 2);

    auto *rightContainer = new QWidget(this);

    auto *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto *previewLabel = new QLabel("Theme Preview:", rightContainer);
    previewLabel->setStyleSheet("font-weight: bold; color: #7F8C8D;");
    rightLayout->addWidget(previewLabel);

    m_mockWindowCanvas = new QFrame(rightContainer);
    m_mockWindowCanvas->setFixedSize(280, 220);

    auto *mockWindowLayout = new QVBoxLayout(m_mockWindowCanvas);
    mockWindowLayout->setContentsMargins(10, 10, 10, 10);
    mockWindowLayout->setSpacing(8);

    m_mockTitleLabel = new QLabel("Sample Application", m_mockWindowCanvas);
    mockWindowLayout->addWidget(m_mockTitleLabel);

    m_mockLineEdit = new QLineEdit("Text input field...", m_mockWindowCanvas);
    m_mockLineEdit->setReadOnly(true);
    mockWindowLayout->addWidget(m_mockLineEdit);

    m_mockCheckBox = new QCheckBox("Active Checkbox option", m_mockWindowCanvas);
    m_mockCheckBox->setChecked(true);
    mockWindowLayout->addWidget(m_mockCheckBox);

    mockWindowLayout->addStretch(1);

    m_mockButton = new QPushButton("Action Button", m_mockWindowCanvas);
    mockWindowLayout->addWidget(m_mockButton);

    rightLayout->addWidget(m_mockWindowCanvas);
    masterLayout->addWidget(rightContainer, 3);

    updateLivePreviewCanvas();

    QObject::connect(m_themesList, &QListWidget::currentRowChanged, [this](int row)
    {
        this->updateLivePreviewCanvas();
    });

    QObject::connect(applyBtn, &QPushButton::clicked, [this]()
    {
        this->sendThemeRequest();
    });
}

void ThemesWindow::sendThemeRequest()
{
    int activeAccentIndex = m_themesList->currentRow();

    if(activeAccentIndex < 0 || activeAccentIndex >= (int)theme_count) return;

    memcpy(GLOB.themecolor, themes[activeAccentIndex].themecolor, 
                                THEME_COLOR_LAST * sizeof(uint32_t));

    // tell the server about the new theme so it can be broadcast to everybody
    send_color_theme_to_server();
}

QString ThemesWindow::colorInHex(int activeTheme, int colorIndex)
{
    uint32_t col = themes[activeTheme].themecolor[colorIndex];
    uint32_t red = (col >> 24) & 0xFF;
    uint32_t green = (col >> 16) & 0xFF;
    uint32_t blue = (col >> 8) & 0xFF;
    QColor color = QColor(red, green, blue);

    return color.name(QColor::HexRgb);
}

void ThemesWindow::updateLivePreviewCanvas()
{
    int activeAccentIndex = m_themesList->currentRow();

    if(activeAccentIndex < 0 || activeAccentIndex >= (int)theme_count) return;

    QString bgHex       = colorInHex(activeAccentIndex, THEME_COLOR_WINDOW_BGCOLOR);
    QString widgetBgHex = colorInHex(activeAccentIndex, THEME_COLOR_INPUTBOX_BGCOLOR);
    QString textHex     = colorInHex(activeAccentIndex, THEME_COLOR_INPUTBOX_TEXTCOLOR);
    QString accentHex   = colorInHex(activeAccentIndex, THEME_COLOR_INPUTBOX_SELECT_BGCOLOR);
    QString pushbuttonTextHex = colorInHex(activeAccentIndex, THEME_COLOR_BUTTON_TEXTCOLOR);
    QString borderHex   = accentHex;

    m_mockWindowCanvas->setStyleSheet(
        QString("QFrame { background-color: %1; border: 2px solid %2; border-radius: 6px; }").arg(bgHex, accentHex)
    );
    m_mockTitleLabel->setStyleSheet(
        QString("QLabel { color: %1; font-weight: bold; font-size: 11px; background: transparent; border: none; }").arg(accentHex)
    );
    m_mockLineEdit->setStyleSheet(
        QString("QLineEdit { background-color: %1; color: %2; border: 1px solid %3; border-radius: 3px; padding: 3px; }").arg(widgetBgHex, textHex, borderHex)
    );
    m_mockCheckBox->setStyleSheet(
        QString("QCheckBox { color: %1; background: transparent; border: none; }"
                "QCheckBox::indicator:checked { background-color: %2; border: 1px solid %3; border-radius: 2px; }").arg(textHex, accentHex, borderHex)
    );
    /*
    m_mockButton->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; border: 1px solid %3; border-radius: 4px; padding: 4px; font-weight: bold; }"
                "QPushButton:hover { background-color: %3; }").arg(widgetBgHex, pushbuttonTextHex, accentHex)
    );
    */

    m_mockWindowCanvas->update();
}

void ThemesWindow::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange)
    {
        this->updateLivePreviewCanvas();
    }

    SettingsBaseWindow::changeEvent(event);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Check for a running instance
    if(!SingleInstanceLock::grabSingleInstanceLock(QString(LOCK_FILE)))
    {
        return 0;
    }

    ThemesWindow themesWindow;
    themesWindow.show();

    // Ensure the lock file is released when the window closes
    QObject::connect(&app, &QApplication::aboutToQuit, []()
    {
        SingleInstanceLock::releaseSingleInstanceLock(QString(LOCK_FILE));
    });

    return app.exec();
}

