#include <QApplication>
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScreen>
#include <QStyle>
#include <vector>

#include <kernel/keycodes.h>
#include "../../desktop/include/gui.h"
#include "../../desktop/include/event.h"
#include "../../desktop/include/kbd.h"
#include "../../desktop/include/keys.h"

#define APPICON_PATH            "/usr/share/gui/icons/keyboard.png"
#define GLOB                    __global_gui_data

struct KeyDefinition
{
    int keyCode;
    QString normalLabel;
    QString shiftedLabel;
};

class VirtualKeyboard : public QWidget
{
private:
    bool m_shiftActive = false;
    bool m_capsActive = false;
    bool m_ctrlActive = false;
    bool m_altActive = false;

    // Dragging state tracking variables
    bool m_isDragging = false;
    QPoint m_dragOffset;

    std::vector<std::pair<QPushButton*, KeyDefinition>> m_keyButtons;

    std::vector<std::vector<KeyDefinition>> m_layoutMatrix =
    {
        { { KEYCODE_BACKTICK, "`", "~" },
          { KEYCODE_1, "1", "!" }, { KEYCODE_2, "2", "@" }, { KEYCODE_3, "3", "#" },
          { KEYCODE_4, "4", "$" }, { KEYCODE_5, "5", "%" }, { KEYCODE_6, "6", "^" },
          { KEYCODE_7, "7", "&&" }, { KEYCODE_8, "8", "*" }, { KEYCODE_9, "9", "(" },
          { KEYCODE_0, "0", ")" }, { KEYCODE_MINUS, "-", "_" }, { KEYCODE_EQUAL, "=", "+" },
          { KEYCODE_BACKSPACE, "Bksp", "Bksp" }
        },
        { { KEYCODE_TAB, "Tab", "Tab" }, { KEYCODE_Q, "q", "Q" }, { KEYCODE_W, "w", "W" },
          { KEYCODE_E, "e", "E" }, { KEYCODE_R, "r", "R" }, { KEYCODE_T, "t", "T" },
          { KEYCODE_Y, "y", "Y" }, { KEYCODE_U, "u", "U" }, { KEYCODE_I, "i", "I" },
          { KEYCODE_O, "o", "O" }, { KEYCODE_P, "p", "P" },
          { KEYCODE_LBRACKET, "[", "{" }, { KEYCODE_RBRACKET, "]", "}" },
          { KEYCODE_BACKSLASH, "\\", "|" },
        },
        { { KEYCODE_CAPS, "Caps", "Caps" }, { KEYCODE_A, "a", "A" },
          { KEYCODE_S, "s", "S" }, { KEYCODE_D, "d", "D" }, { KEYCODE_F, "f", "F" },
          { KEYCODE_G, "g", "G" }, { KEYCODE_H, "h", "H" }, { KEYCODE_J, "j", "J" },
          { KEYCODE_K, "k", "K" }, { KEYCODE_L, "l", "L" },
          { KEYCODE_SEMICOLON, ";", ":" }, { KEYCODE_QUOTE, "'", "\"" },
          { KEYCODE_ENTER, "Enter", "Enter" }
        },
        { { KEYCODE_LSHIFT, "Shift", "Shift" }, { KEYCODE_Z, "z", "Z" },
          { KEYCODE_X, "x", "X" }, { KEYCODE_C, "c", "C" }, { KEYCODE_V, "v", "V" },
          { KEYCODE_B, "b", "B" }, { KEYCODE_N, "n", "N" }, { KEYCODE_M, "m", "M" },
          { KEYCODE_COMMA, ",", "<" }, { KEYCODE_DOT, ".", ">" },
          { KEYCODE_SLASH, "/", "?" }, { KEYCODE_RSHIFT, "Shift", "Shift" },
        },
        {
          { KEYCODE_LCTRL, "Ctrl", "Ctrl" }, { KEYCODE_LALT, "Alt", "Alt" },
          { KEYCODE_SPACE, "Space", "Space" },
          { KEYCODE_RALT, "Alt", "Alt" }, { KEYCODE_RCTRL, "Ctrl", "Ctrl" },
          { KEYCODE_LEFT, "Left", "Left" }, { KEYCODE_UP, "Up", "Up" },
          { KEYCODE_DOWN, "Down", "Down" }, { KEYCODE_RIGHT, "Right", "Right" },
        },
    };

public:
    explicit VirtualKeyboard(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void createHandleBar(QVBoxLayout *masterLayout);
    void buildKeyboardLayout(QVBoxLayout *canvasLayout);
    void handleKeyClick(const KeyDefinition& key, QPushButton *btn);
    void refreshKeyboardLabels();
    void refreshStyle(QWidget *widget);
};

VirtualKeyboard::VirtualKeyboard(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::BypassWindowManagerHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(650, 240);
    setWindowIcon(QIcon(APPICON_PATH));
    setObjectName("MainCanvas");

    setStyleSheet(
        "QWidget#MainCanvas { background-color: #1F1F1F; border: 2px solid #2D2D2D; border-radius: 6px; }"
        "QPushButton { background-color: #2D2D2D; color: #ECF0F1; border: 1px solid #3A3A3A; border-radius: 4px; font-weight: bold; font-size: 12px; }"
        "QPushButton:hover { background-color: #3A3A3A; border-color: #16A085; }"
        "QPushButton:pressed { background-color: #16A085; color: white; }"
        "QPushButton#ToggleActive { background-color: #117A65; border-color: #1ABC9C; color: white; }"

        "QWidget#HandleBar { background-color: #161616; border: none; border-bottom: 1px solid #2D2D2D; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QLabel#HandleTitle { color: #7F8C8D; font-weight: bold; font-size: 11px; background: transparent; border: none; }"
        "QPushButton#CloseBtn { background-color: #C0392B; color: white; border: none; border-radius: 3px; font-weight: bold; }"
        "QPushButton#CloseBtn:hover { background-color: #E74C3C; }"
    );

    auto *masterLayout = new QVBoxLayout(this);
    masterLayout->setContentsMargins(0, 0, 0, 8);
    masterLayout->setSpacing(6);

    createHandleBar(masterLayout);

    auto *keyboardCanvas = new QWidget(this);
    //keyboardCanvas->setStyleSheet("border: none; background: transparent;");

    auto *canvasLayout = new QVBoxLayout(keyboardCanvas);
    canvasLayout->setContentsMargins(8, 0, 8, 0);
    canvasLayout->setSpacing(6);

    buildKeyboardLayout(canvasLayout);
    masterLayout->addWidget(keyboardCanvas);
}

void VirtualKeyboard::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton && event->pos().y() <= 30)
    {
        m_isDragging = true;
        m_dragOffset = event->pos();
        event->accept();
    }
}

void VirtualKeyboard::mouseMoveEvent(QMouseEvent *event)
{
    if(m_isDragging && (event->buttons() & Qt::LeftButton))
    {
        QPoint globalPos = event->globalPosition().toPoint();
        QPoint newPos = globalPos - m_dragOffset;

        if(QScreen *screen = QGuiApplication::primaryScreen())
        {
            QRect scr = screen->geometry();
            newPos.setX(qBound(0, newPos.x(), scr.width() - width()));
            newPos.setY(qBound(0, newPos.y(), scr.height() - height()));
        }

        move(newPos);
        event->accept();
    }
}

void VirtualKeyboard::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_isDragging = false;
        event->accept();
    }
}

void VirtualKeyboard::createHandleBar(QVBoxLayout *masterLayout)
{
    auto *handleBar = new QWidget(this);
    handleBar->setObjectName("HandleBar");
    handleBar->setFixedHeight(30);

    auto *handleLayout = new QHBoxLayout(handleBar);
    handleLayout->setContentsMargins(10, 0, 10, 0);

    auto *titleLabel = new QLabel(":::  On-Screen Keyboard  :::", handleBar);
    titleLabel->setObjectName("HandleTitle");

    auto *closeBtn = new QPushButton("X", handleBar);
    closeBtn->setObjectName("CloseBtn");
    closeBtn->setFixedSize(20, 20);
    closeBtn->setFocusPolicy(Qt::NoFocus);

    QObject::connect(closeBtn, &QPushButton::clicked, []()
    {
        QCoreApplication::quit();
    });

    handleLayout->addWidget(titleLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    handleLayout->addStretch(1);
    handleLayout->addWidget(closeBtn, 0, Qt::AlignRight | Qt::AlignVCenter);

    masterLayout->addWidget(handleBar);
}

void VirtualKeyboard::buildKeyboardLayout(QVBoxLayout *canvasLayout)
{
    for(const auto& rowData : m_layoutMatrix)
    {
        auto *rowWidget = new QWidget(this);
        //rowWidget->setStyleSheet("border: none; background: transparent;");

        auto *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(5);

        for(const auto& keyDef : rowData)
        {
            auto *btn = new QPushButton(keyDef.normalLabel, rowWidget);
            btn->setFocusPolicy(Qt::NoFocus);

            if(keyDef.keyCode == KEYCODE_SPACE) btn->setFixedWidth(160);
            else if(keyDef.keyCode == KEYCODE_TAB) btn->setFixedWidth(45);
            else if(keyDef.keyCode == KEYCODE_CAPS ||
                    keyDef.keyCode == KEYCODE_ENTER) btn->setFixedWidth(65);
            else if(keyDef.keyCode == KEYCODE_LSHIFT ||
                    keyDef.keyCode == KEYCODE_RSHIFT) btn->setFixedWidth(75);
            else if(keyDef.keyCode == KEYCODE_LCTRL ||
                    keyDef.keyCode == KEYCODE_RCTRL ||
                    keyDef.keyCode == KEYCODE_LALT ||
                    keyDef.keyCode == KEYCODE_RALT) btn->setFixedWidth(55);
            else btn->setFixedWidth(40);

            btn->setFixedHeight(34);
            m_keyButtons.push_back({btn, keyDef});

            QObject::connect(btn, &QPushButton::clicked, [this, keyDef, btn]()
            {
                this->handleKeyClick(keyDef, btn);
            });

            rowLayout->addWidget(btn);
        }
        canvasLayout->addWidget(rowWidget);
    }
}

static inline void send_key_event(winid_t winid, char key, char modifiers)
{
    struct event_t ev;

    ev.src = TO_WINID(GLOB.mypid, 0);
    ev.dest = winid;
    ev.valid_reply = 1;
    ev.type = EVENT_KEY_PRESS;
    ev.key.code = key;
    ev.key.modifiers = modifiers;
    write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));

    ev.type = EVENT_KEY_RELEASE;
    write(GLOB.serverfd, (void *)&ev, sizeof(struct event_t));
}

void VirtualKeyboard::handleKeyClick(const KeyDefinition& key, QPushButton *btn)
{
    if(key.keyCode == KEYCODE_LSHIFT || key.keyCode == KEYCODE_RSHIFT)
    {
        m_shiftActive = !m_shiftActive;
        btn->setObjectName(m_shiftActive ? "ToggleActive" : "");
        refreshKeyboardLabels();
        return;
    }

    if(key.keyCode == KEYCODE_CAPS)
    {
        m_capsActive = !m_capsActive;
        btn->setObjectName(m_capsActive ? "ToggleActive" : "");
        refreshKeyboardLabels();
        return;
    }

    if(key.keyCode == KEYCODE_LCTRL || key.keyCode == KEYCODE_RCTRL)
    {
        m_ctrlActive = !m_ctrlActive;
        btn->setObjectName(m_ctrlActive ? "ToggleActive" : "");
        refreshStyle(btn);
        return;
    }

    if(key.keyCode == KEYCODE_LALT || key.keyCode == KEYCODE_RALT)
    {
        m_altActive = !m_altActive;
        btn->setObjectName(m_altActive ? "ToggleActive" : "");
        refreshStyle(btn);
        return;
    }

    char currentModifiers = 0;
    if(m_shiftActive) currentModifiers |= MODIFIER_MASK_SHIFT;
    if(m_ctrlActive)  currentModifiers |= MODIFIER_MASK_CTRL;
    if(m_altActive)   currentModifiers |= MODIFIER_MASK_ALT;
    if(m_capsActive)  currentModifiers |= MODIFIER_MASK_CAPS;

    winid_t winid = get_input_focus();
    send_key_event(winid, key.keyCode, currentModifiers);

    if(m_shiftActive)
    {
        m_shiftActive = false;

        for(auto& pair : m_keyButtons)
        {
            if(pair.second.keyCode == KEYCODE_LSHIFT ||
               pair.second.keyCode == KEYCODE_RSHIFT)
            {
                pair.first->setObjectName("");
                refreshStyle(pair.first);
            }
        }
        refreshKeyboardLabels();
    }
}

void VirtualKeyboard::refreshKeyboardLabels()
{
    bool useUpper = (m_shiftActive != m_capsActive);

    for(auto& pair : m_keyButtons)
    {
        QPushButton *btn = pair.first;
        const KeyDefinition& def = pair.second;

        /*
        if(def.keyCode == KEYCODE_BACKSPACE || def.keyCode == KEYCODE_TAB ||
           def.keyCode == KEYCODE_CAPS || def.keyCode == KEYCODE_ENTER ||
           def.keyCode == KEYCODE_LSHIFT || def.keyCode == KEYCODE_RSHIFT ||
           def.keyCode == KEYCODE_LCTRL || def.keyCode == KEYCODE_RCTRL ||
           def.keyCode == KEYCODE_LALT || def.keyCode == KEYCODE_RALT ||
           def.keyCode == KEYCODE_SPACE)
        {
            continue;
        }
        */

        btn->setText(useUpper ? def.shiftedLabel : def.normalLabel);
        refreshStyle(btn);
    }
}

void VirtualKeyboard::refreshStyle(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    VirtualKeyboard osk;

    osk.show();

    if(QScreen *screen = QGuiApplication::primaryScreen())
    {
        QRect scr = screen->geometry();
        osk.move((scr.width() - osk.width()) / 2, scr.height() - osk.height() - 80);
    }

    return app.exec();
}

