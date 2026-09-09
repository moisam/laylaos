#include <QApplication>
#include <QWidget>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QLineEdit>
#include <QString>
#include <QKeyEvent>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QIcon>
#include <QStyleOptionButton>
#include <QJSEngine>
#include <cmath>
#include <algorithm>

#define APPICON_PATH            "/usr/share/gui/icons/calculator.png"

class EventButton;
class EventListWidget;

class Calculator : public QWidget
{
public:
    enum class CalcMode { Standard, Scientific, Programmer };
    enum class NumBase { Hex = 16, Dec = 10, Oct = 8, Bin = 2 };

    explicit Calculator(QWidget *parent = nullptr);
    ~Calculator() = default;

    void dispatchInput(const QString &input);
    void dispatchHistoryDoubleClicked(const QString &itemText);
    void dispatchClearHistory();
    void setCalculatorMode(CalcMode mode);
    void setNumericalBase(NumBase base);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void initUi();
    void updateActiveLayout();
    void calculateResult();
    void applyAdvancedOp(const QString &op);
    void applyBitwiseOp(const QString &op);
    void applyScientificOp(const QString &op);
    void addToHistory(const QString &expr, const QString &res);
    void updateBaseLabels(long long value);
    void clearGrid();
    void updateMenuIndicators();

    CalcMode m_currentMode{CalcMode::Standard};
    NumBase m_currentBase{NumBase::Dec};

    QLineEdit *m_display{nullptr};
    QListWidget *m_historyList{nullptr};
    QGridLayout *m_buttonGrid{nullptr};

    // Sub-labels for Programmer Mode base quick-views
    QWidget *m_baseLabelContainer{nullptr};
    QLabel *m_hexLabel{nullptr};
    QLabel *m_decLabel{nullptr};
    QLabel *m_octLabel{nullptr};
    QLabel *m_binLabel{nullptr};

    // Menu Pointers for Dynamic Adjustments
    QMenu *m_baseMenu{nullptr};
    QAction *m_stdAction{nullptr};
    QAction *m_sciAction{nullptr};
    QAction *m_progAction{nullptr};
    
    QAction *m_hexAction{nullptr};
    QAction *m_decAction{nullptr};
    QAction *m_octAction{nullptr};
    QAction *m_binAction{nullptr};

    QString m_expression{""};
};

// Custom button that forwards clicks to the parent container
class EventButton : public QWidget
{
public:
    EventButton(const QString &text, Calculator *parent, bool enabled = true);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    Calculator *m_parentCalc;
    bool m_enabled;
};

// Custom ListWidget that traps double-clicks
class EventListWidget : public QListWidget
{
public:
    explicit EventListWidget(Calculator *parent);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    Calculator *m_parentCalc;
};

class EventClearButton : public QWidget
{
public:
    explicit EventClearButton(const QString &text, Calculator *parent);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    Calculator *m_parentCalc;
};

// ==========================================
// EVENT BUTTON
// ==========================================
EventButton::EventButton(const QString &text, Calculator *parent, bool enabled) 
    : QWidget(parent), m_text(text), m_parentCalc(parent), m_enabled(enabled)
{
    setFixedSize(65, 50);
}

void EventButton::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
    {
        m_parentCalc->dispatchInput(m_text);
    }
}

void EventButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QStyleOptionButton option;
    option.initFrom(this);
    option.text = m_text;
    option.rect = rect();

    if(m_enabled)
    {
        option.state |= QStyle::State_Enabled;
    }
    else
    {
        option.state &= ~QStyle::State_Enabled;
    }

    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
}

// ==========================================
// EVENT LISTWIDGET
// ==========================================
EventListWidget::EventListWidget(Calculator *parent) 
    : QListWidget(parent), m_parentCalc(parent) {}

void EventListWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    QListWidget::mouseDoubleClickEvent(event);
    if(auto *currentItem = this->currentItem())
    {
        m_parentCalc->dispatchHistoryDoubleClicked(currentItem->text());
    }
}

// ==========================================
// EVENT CLEAR BUTTON
// ==========================================
EventClearButton::EventClearButton(const QString &text, Calculator *parent)
    : QWidget(parent), m_text(text), m_parentCalc(parent)
{
    // Make the button span the width of the history container panel
    setMinimumHeight(30); 
}

void EventClearButton::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::MouseButton::LeftButton)
    {
        m_parentCalc->dispatchClearHistory();
    }
}

void EventClearButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    QStyleOptionButton option;
    option.initFrom(this);
    option.text = m_text;
    option.rect = rect();
    option.state |= QStyle::State_Enabled;
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
}

// ==========================================
// MAIN CALCULATOR ENGINE
// ==========================================
Calculator::Calculator(QWidget *parent) : QWidget(parent)
{
    setWindowTitle("Calculator");
    setMinimumSize(580, 480);
    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setWindowIcon(QIcon(APPICON_PATH));
    initUi();
}

void Calculator::initUi()
{
    auto *mainVerticalStack = new QVBoxLayout(this);
    mainVerticalStack->setContentsMargins(0, 0, 0, 0);
    mainVerticalStack->setSpacing(0);

    auto *menuBar = new QMenuBar(this);
    auto *modeMenu = menuBar->addMenu("View Modes");

    m_stdAction  = modeMenu->addAction("Standard Mode", [this]()
    {
        setCalculatorMode(CalcMode::Standard);
    });

    m_sciAction  = modeMenu->addAction("Scientific Mode", [this]()
    {
        setCalculatorMode(CalcMode::Scientific);
    });

    m_progAction = modeMenu->addAction("Programmer Mode", [this]()
    {
        setCalculatorMode(CalcMode::Programmer);
    });

    m_stdAction->setCheckable(true);
    m_sciAction->setCheckable(true);
    m_progAction->setCheckable(true);

    m_baseMenu = menuBar->addMenu("Numeric Base");
    m_hexAction = m_baseMenu->addAction("Hexadecimal", [this]() { setNumericalBase(NumBase::Hex); });
    m_decAction = m_baseMenu->addAction("Decimal", [this]() { setNumericalBase(NumBase::Dec); });
    m_octAction = m_baseMenu->addAction("Octal", [this]() { setNumericalBase(NumBase::Oct); });
    m_binAction = m_baseMenu->addAction("Binary", [this]() { setNumericalBase(NumBase::Bin); });

    m_hexAction->setCheckable(true);
    m_decAction->setCheckable(true);
    m_octAction->setCheckable(true);
    m_binAction->setCheckable(true);

    mainVerticalStack->addWidget(menuBar);

    auto *workspaceLayout = new QHBoxLayout();
    workspaceLayout->setContentsMargins(10, 10, 10, 10);
    workspaceLayout->setSpacing(15);

    // --- LEFT PANEL: Calculator GUI ---
    auto *leftContainer = new QVBoxLayout();
    leftContainer->setSpacing(8);

    m_display = new QLineEdit(this);
    m_display->setFont(QFont("Arial", 26));
    m_display->setAlignment(Qt::AlignmentFlag::AlignRight);
    m_display->setReadOnly(true);
    m_display->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    m_display->setText("0");
    leftContainer->addWidget(m_display);

    // Programmer dynamic Radix display fields container
    m_baseLabelContainer = new QWidget(this);
    auto *baseLabelLayout = new QVBoxLayout(m_baseLabelContainer);
    baseLabelLayout->setContentsMargins(5, 0, 5, 0);
    baseLabelLayout->setSpacing(2);

    m_hexLabel = new QLabel("HEX: 0", this);
    m_decLabel = new QLabel("DEC: 0", this);
    m_octLabel = new QLabel("OCT: 0", this);
    m_binLabel = new QLabel("BIN: 0", this);

    baseLabelLayout->addWidget(m_hexLabel);
    baseLabelLayout->addWidget(m_decLabel);
    baseLabelLayout->addWidget(m_octLabel);
    baseLabelLayout->addWidget(m_binLabel);
    m_baseLabelContainer->setVisible(false); // Default hidden standard view
    leftContainer->addWidget(m_baseLabelContainer);

    m_buttonGrid = new QGridLayout();
    m_buttonGrid->setSpacing(5);
    leftContainer->addLayout(m_buttonGrid);
    workspaceLayout->addLayout(leftContainer, 0);

    // --- RIGHT PANEL: Event List History Panel ---
    auto *rightContainer = new QVBoxLayout();
    rightContainer->setSpacing(8);

    auto *historyTitle = new QLabel("History log", this);
    historyTitle->setFont(QFont("Arial", 11, QFont::Weight::Bold));
    rightContainer->addWidget(historyTitle);

    m_historyList = new EventListWidget(this);
    m_historyList->setFont(QFont("Courier New", 11));
    m_historyList->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    rightContainer->addWidget(m_historyList);

    auto *clearBtn = new EventClearButton("Clear History", this);
    rightContainer->addWidget(clearBtn);
    workspaceLayout->addLayout(rightContainer, 1);

    mainVerticalStack->addLayout(workspaceLayout);

    updateActiveLayout();
    updateMenuIndicators();
}

void Calculator::clearGrid()
{
    QLayoutItem *item;
    while((item = m_buttonGrid->takeAt(0)) != nullptr)
    {
        if(item->widget())
        {
            item->widget()->deleteLater();
        }
        delete item;
    }
}

void Calculator::setCalculatorMode(CalcMode mode)
{
    QString currentDisplayValue = m_display->text();
    bool hadError = (currentDisplayValue == "Error");
    int oldBase = (m_currentMode == CalcMode::Programmer) ? static_cast<int>(m_currentBase) : 10;

    m_currentMode = mode;
    m_baseLabelContainer->setVisible(mode == CalcMode::Programmer);

    if(m_currentMode == CalcMode::Standard)
    {
        setWindowTitle("Calculator");
    }
    else if(m_currentMode == CalcMode::Scientific)
    {
        setWindowTitle("Calculator - Scientific Mode");
    }
    else if(m_currentMode == CalcMode::Programmer)
    {
        setWindowTitle("Calculator - Programmer Mode");
    }

    if(hadError || currentDisplayValue.isEmpty())
    {
        m_expression.clear();
        m_display->setText("0");
        updateBaseLabels(0);
    }
    else
    {
        if(m_currentMode == CalcMode::Programmer)
        {
            // Converting from Standard/Scientific float or a different Programmer base
            bool ok;
            long long intVal = 0;
            
            if(oldBase == 10)
            {
                // If coming from a float mode, cast the double to an integer
                intVal = static_cast<long long>(currentDisplayValue.toDouble(&ok));
            }
            else
            {
                intVal = currentDisplayValue.toLongLong(&ok, oldBase);
            }
            
            if(ok)
            {
                // Format the string matching the active target radix base
                m_expression = QString::number(intVal, static_cast<int>(m_currentBase)).toUpper();
                m_display->setText(m_expression);
                updateBaseLabels(intVal);
            }
            else
            {
                m_expression.clear();
                m_display->setText("0");
                updateBaseLabels(0);
            }
        }
        else
        {
            // Converting into Standard or Scientific decimal mode
            if(oldBase != 10)
            {
                // Convert raw integer tokens out of Programmer base into standard base-10
                bool ok;
                long long intVal = currentDisplayValue.toLongLong(&ok, oldBase);
                m_expression = ok ? QString::number(intVal) : "0";
            }
            else
            {
                m_expression = currentDisplayValue;
            }
            m_display->setText(m_expression);
        }
    }

    updateActiveLayout();
    updateMenuIndicators();
}

void Calculator::setNumericalBase(NumBase base)
{
    if(m_currentMode != CalcMode::Programmer) return;
    
    // Convert current value to new base
    bool ok;
    long long currentVal = m_display->text().toLongLong(&ok, static_cast<int>(m_currentBase));
    m_currentBase = base;
    
    if(ok)
    {
        m_display->setText(QString::number(currentVal, static_cast<int>(m_currentBase)).toUpper());
    }

    updateActiveLayout();
    updateMenuIndicators();
}

void Calculator::updateMenuIndicators()
{
    if(m_baseMenu)
    {
        m_baseMenu->menuAction()->setVisible(m_currentMode == CalcMode::Programmer);
    }

    if(m_stdAction)  m_stdAction->setChecked(m_currentMode == CalcMode::Standard);
    if(m_sciAction)  m_sciAction->setChecked(m_currentMode == CalcMode::Scientific);
    if(m_progAction) m_progAction->setChecked(m_currentMode == CalcMode::Programmer);

    if(m_hexAction) m_hexAction->setChecked(m_currentBase == NumBase::Hex);
    if(m_decAction) m_decAction->setChecked(m_currentBase == NumBase::Dec);
    if(m_octAction) m_octAction->setChecked(m_currentBase == NumBase::Oct);
    if(m_binAction) m_binAction->setChecked(m_currentBase == NumBase::Bin);
}

void Calculator::updateActiveLayout()
{
    clearGrid();

    struct GridDef { QString text; int r; int c; bool enabled{true}; };
    std::vector<GridDef> layoutButtons;

    if(m_currentMode == CalcMode::Standard)
    {
        layoutButtons =
        {
            {"%", 0, 0},   {"1/x", 0, 1}, {"x²", 0, 2},  {"sqrt", 0, 3},
            {"C", 1, 0},   {"(", 1, 1},   {")", 1, 2},   {"/", 1, 3},
            {"7", 2, 0},   {"8", 2, 1},   {"9", 2, 2},   {"*", 2, 3},
            {"4", 3, 0},   {"5", 3, 1},   {"6", 3, 2},   {"-", 3, 3},
            {"1", 4, 0},   {"2", 4, 1},   {"3", 4, 2},   {"+", 4, 3},
            {"±", 5, 0},   {"0", 5, 1},   {".", 5, 2},   {"=", 5, 3}
        };
    } 
    else if(m_currentMode == CalcMode::Scientific)
    {
        layoutButtons =
        {
            {"sin", 0, 0}, {"cos", 0, 1}, {"tan", 0, 2}, {"log", 0, 3}, {"ln", 0, 4},
            {"%", 1, 0},   {"1/x", 1, 1}, {"x²", 1, 2},  {"sqrt", 1, 3}, {"^", 1, 4},
            {"C", 2, 0},   {"(", 2, 1},   {")", 2, 2},   {"/", 2, 3},    {"π", 2, 4},
            {"7", 3, 0},   {"8", 3, 1},   {"9", 3, 2},   {"*", 3, 3},    {"e", 3, 4},
            {"4", 4, 0},   {"5", 4, 1},   {"6", 4, 2},   {"-", 4, 3},    {"", 4, 4},
            {"1", 5, 0},   {"2", 5, 1},   {"3", 5, 2},   {"+", 5, 3},    {"", 5, 4},
            {"±", 6, 0},   {"0", 6, 1},   {".", 6, 2},   {"=", 6, 3},    {"", 6, 4}
        };
    } 
    else if(m_currentMode == CalcMode::Programmer)
    {
        int baseVal = static_cast<int>(m_currentBase);
        
        // Conditional button mapping configurations to disable numbers outside active radix range
        layoutButtons =
        {
            {"AND", 0, 0},  {"OR", 0, 1},  {"XOR", 0, 2},  {"NOT", 0, 3},  {"Lsh", 0, 4},  {"Rsh", 0, 5},
            {"A", 1, 0, true}, {"B", 1, 1, true}, {"C", 1, 2, true}, {"D", 1, 3, true}, {"E", 1, 4, true}, {"F", 1, 5, true},
            {"7", 2, 0, baseVal>=10}, {"8", 2, 1, baseVal==16}, {"9", 2, 2, baseVal==16}, {"/", 2, 3}, {"(", 2, 4}, {")", 2, 5},
            {"4", 3, 0, baseVal>=8},  {"5", 3, 1, baseVal>=8},  {"6", 3, 2, baseVal>=8},  {"*", 3, 3}, {"", 3, 4},  {"", 3, 5},
            {"1", 4, 0},              {"2", 4, 1, baseVal>=8},  {"3", 4, 2, baseVal>=8},  {"-", 4, 3}, {"", 4, 4},  {"", 4, 5},
            {"0", 5, 0},              {"C", 5, 1},              {"=", 5, 2},              {"+", 5, 3}, {"", 5, 4},  {"", 5, 5}
        };
    }

    for(const auto& btnDef : layoutButtons)
    {
        if(btnDef.text.isEmpty()) continue;
        auto *btn = new EventButton(btnDef.text, this, btnDef.enabled);
        m_buttonGrid->addWidget(btn, btnDef.r, btnDef.c);
    }
}

void Calculator::keyPressEvent(QKeyEvent *event)
{
    QString keyText = event->text().toUpper();
    // Filter input paths dynamically to ensure illegal characters cannot slip through matching bases
    QString validationString = "0123456789+-/.()^%";

    if(m_currentMode == CalcMode::Programmer)
    {
        // Checking for hexadecimal characters A-F
        if(keyText == "A" || keyText == "B" || keyText == "C" || 
           keyText == "D" || keyText == "E" || keyText == "F")
        {
            if(m_currentBase != NumBase::Hex)
            {
                setNumericalBase(NumBase::Hex);
            }
        }

        if(m_currentBase == NumBase::Hex) validationString = "0123456789ABCDEF+-/()";
        else if(m_currentBase == NumBase::Oct) validationString = "01234567+-/()";
        else if(m_currentBase == NumBase::Bin) validationString = "01+-*/()";
    }

    if(!keyText.isEmpty() && validationString.contains(keyText))
    {
        dispatchInput(keyText);
        return;
    }

    if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) dispatchInput("=");
    else if(event->key() == Qt::Key_Escape || event->key() == Qt::Key_Delete) dispatchInput("C");
    else if(event->key() == Qt::Key_Backspace)
    {
        if(!m_expression.isEmpty())
        {
            m_expression.chop(1);
            m_display->setText(m_expression.isEmpty() ? "0" : m_expression);
        }
    }
    else QWidget::keyPressEvent(event);
}

void Calculator::dispatchInput(const QString &input)
{
    if(m_currentMode == CalcMode::Programmer)
    {
        // Checking for hexadecimal characters A-F
        if(input == "A" || input == "B" || input == "C" || 
           input == "D" || input == "E" || input == "F")
        {
            if(m_currentBase != NumBase::Hex)
            {
                setNumericalBase(NumBase::Hex);
            }
        }
        // Checking for decimal exclusive keys while sitting in Binary
        else if(input == "2" || input == "3" || input == "4" || input == "5" || 
                input == "6" || input == "7" || input == "8" || input == "9")
        {
            if(m_currentBase == NumBase::Bin)
            {
                setNumericalBase(NumBase::Dec);
            }
        }

        if(input == "AND" || input == "OR" || input == "XOR" || 
           input == "NOT" || input == "Lsh" || input == "Rsh")
        {
            applyBitwiseOp(input);
            return;
        }
    }
    else if(m_currentMode == CalcMode::Scientific)
    {
        if(input == "sin" || input == "cos" || input == "tan" ||
           input == "log" || input == "ln" || input == "π" || input == "e")
        {
            applyScientificOp(input);
            return;
        }
    }

    if(input == "%" || input == "1/x" || input == "x²" || input == "sqrt" || input == "±")
    {
        applyAdvancedOp(input);
        return;
    }

    if(input == "C")
    {
        m_expression.clear();
        m_display->setText("0");
        updateBaseLabels(0);
        return;
    }
    else if(input == "=")
    {
        calculateResult();
        return;
    }

    // INPUT VALIDATION ENGINE
    QString operators = "+-*/";

    if(!m_expression.isEmpty())
    {
        QChar lastChar = m_expression.at(m_expression.length() - 1);

        // Handle consecutive math operators (e.g., replace '+' with '*' if clicked successively)
        if(operators.contains(input) && operators.contains(lastChar))
        {
            m_expression.chop(1); // Drop the old operator
            m_expression.append(input); // Splice in the new operator
            m_display->setText(m_expression);
            return;
        }

        // Handle decimal input validation
        if(input == ".")
        {
            if(lastChar == '.')
            {
                return; // Block duplicated consecutive decimals
            }

            if(operators.contains(lastChar))
            {
                // If typed right after an operator (e.g. "6+."), prepend an implicit zero
                m_expression.append("0.");
                m_display->setText(m_expression);
                return;
            }

            // Scan backward to see if the CURRENT number already has a decimal
            bool foundDecimalInCurrentNumber = false;

            for(int i = m_expression.length() - 1; i >= 0; --i)
            {
                QChar c = m_expression.at(i);
                
                // If we hit any operator or bracket, we've moved past the current number segment
                if(operators.contains(c) || c == '(' || c == ')' || c == ' ')
                {
                    break; 
                }
                
                // If we encounter a decimal point first, block the input
                if(c == '.')
                {
                    foundDecimalInCurrentNumber = true;
                    break;
                }
            }

            if(foundDecimalInCurrentNumber)
            {
                return; // Silently reject the input to prevent things like "65.3.6"
            }
        }

        // Fix Bracket Syntax: Inject an explicit '*' operator if '(' follows a digit or ')'
        if(input == "(" && (lastChar.isLetterOrNumber() || lastChar == ')'))
        {
            m_expression.append("*");
        }
    }
    else
    {
        // Prevent starting an empty expression with a trailing operator
        // (except subtraction for negative numbers)
        if(QString("+*/.").contains(input))
        {
            return;
        }

        // If the expression is empty and the user clicks ".", turn it into "0."
        if(input == ".")
        {
            m_expression.append("0.");
            m_display->setText(m_expression);
            return;
        }
    }

    // Prevent leading zeroes
    if(m_expression == "0" && input == "0")
    {
        return;
    }

    // Replace lonely initial zero
    if(m_expression == "0" && input != ".")
    {
        m_expression.clear();
    }

    if(m_display->text() == "Error")
    {
        m_display->clear();
    }

    m_expression.append(input);
    m_display->setText(m_expression);

    if(m_currentMode == CalcMode::Programmer)
    {
        bool ok;
        long long val = m_expression.toLongLong(&ok, static_cast<int>(m_currentBase));
        if(ok) updateBaseLabels(val);
    }
}

void Calculator::applyScientificOp(const QString &op)
{
    if(op == "π")
    {
        m_expression.append(QString::number(M_PI));
        m_display->setText(m_expression);
        return;
    }

    if(op == "e")
    {
        m_expression.append(QString::number(M_E));
        m_display->setText(m_expression);
        return;
    }

    calculateResult();

    bool ok;
    double val = m_display->text().toDouble(&ok);
    if(!ok) return;

    double res = 0.0;
    if(op == "sin") res = std::sin(val);
    else if(op == "cos") res = std::cos(val);
    else if(op == "tan") res = std::tan(val);
    else if(op == "log") res = std::log10(val);
    else if(op == "ln") res = std::log(val);

    m_expression = QString::number(res);
    m_display->setText(m_expression);
    addToHistory(op + "(" + QString::number(val) + ")", m_expression);
}

void Calculator::applyBitwiseOp(const QString &op)
{
    if(op == "NOT")
    {
        bool ok;
        long long val = m_display->text().toLongLong(&ok, static_cast<int>(m_currentBase));
        if(ok)
        {
            long long res = ~val;
            m_expression = QString::number(res, static_cast<int>(m_currentBase)).toUpper();
            m_display->setText(m_expression);
            updateBaseLabels(res);
            addToHistory("NOT(" + QString::number(val, static_cast<int>(m_currentBase)) + ")", m_expression);
        }

        return;
    }

    // Binary operators require mapping operational tokens into intermediate evaluations
    QString translateOp = op;
    if(op == "AND") translateOp = "&";
    else if(op == "OR") translateOp = "|";
    else if(op == "XOR") translateOp = "^";
    else if(op == "Lsh") translateOp = "<<";
    else if(op == "Rsh") translateOp = ">>";
    m_expression.append(" " + translateOp + " ");
    m_display->setText(m_expression);
}

void Calculator::applyAdvancedOp(const QString &op)
{
    QString originalExpr = m_expression.isEmpty() ? m_display->text() : m_expression;

    // Evaluate continuous operations first if they exist (e.g. "5 + 5" becomes "10" before doing "sqrt")
    if(!m_expression.isEmpty() && m_display->text() != "Error" && 
       (m_expression.contains("+") || m_expression.contains("-") || 
        m_expression.contains("*") || m_expression.contains("/")))
    {
        calculateResult();
    }

    bool ok;
    double currentVal = m_display->text().toDouble(&ok);

    if(!ok || m_display->text() == "Error")
    {
        m_display->setText("Error");
        m_expression.clear();
        return;
    }

    double result = 0.0;
    QString operationFormat = "";

    if(op == "%")
    { 
        result = currentVal / 100.0; 
        operationFormat = QString("(%1)%").arg(originalExpr); 
    } 
    else if(op == "1/x")
    {
        if(currentVal == 0.0) { m_display->setText("Error"); m_expression.clear(); return; }
        result = 1.0 / currentVal;
        operationFormat = QString("1/(%1)").arg(originalExpr);
    } 
    else if(op == "x²")
    { 
        result = currentVal * currentVal; 
        operationFormat = QString("(%1)²").arg(originalExpr); 
    } 
    else if(op == "sqrt")
    {
        if(currentVal < 0.0) { m_display->setText("Error"); m_expression.clear(); return; }
        result = std::sqrt(currentVal);
        operationFormat = QString("sqrt(%1)").arg(originalExpr);
    } 
    else if(op == "±")
    {
        result = currentVal * -1.0; 
        operationFormat = QString("negate(%1)").arg(originalExpr); 
    }

    m_expression = QString::number(result);
    m_display->setText(m_expression);

    addToHistory(operationFormat, m_expression);
}

void Calculator::calculateResult()
{
    if(m_expression.isEmpty()) return;

    if(m_currentMode == CalcMode::Programmer)
    {
        // Because QJSEngine only parses base-10 strings natively,
        // we parse Programmer tokens contextually inside a Javascript evaluator context
        QString evalReadyExpr = m_expression;

        // Clean up spacing to keep operators separated from letters
        evalReadyExpr.replace("+", " + ");
        evalReadyExpr.replace("-", " - ");
        evalReadyExpr.replace("*", " * ");
        evalReadyExpr.replace("/", " / ");
        evalReadyExpr.replace("(", " ( ");
        evalReadyExpr.replace(")", " ) ");

        // Split the calculation line into individual semantic tokens
        QStringList tokens = evalReadyExpr.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        // Loop through and prepend JavaScript radix prefixes to the isolated numbers
        for(int i = 0; i < tokens.size(); ++i)
        {
            QString &token = tokens[i];
            
            // Skip structural operators
            if(token == "+" || token == "-" || token == "*" || token == "/" || 
               token == "(" || token == ")" || token == "&" || token == "|" || 
               token == "^" || token == "<<" || token == ">>")
            {
                continue;
            }
            
            // Format numbers to their specific JavaScript engine requirements
            if(m_currentBase == NumBase::Hex)
            {
                token = "0x" + token;
            }
            else if(m_currentBase == NumBase::Oct)
            {
                token = "0o" + token;
            }
            else if(m_currentBase == NumBase::Bin)
            {
                token = "0b" + token;
            }
        }

        evalReadyExpr = tokens.join(" ");

        QJSEngine engine;
        QJSValue result = engine.evaluate(evalReadyExpr);
        
        if(!result.isError())
        {
            long long finalInt = result.toVariant().toLongLong();
            QString rawRes = QString::number(finalInt, static_cast<int>(m_currentBase)).toUpper();
            
            addToHistory(m_expression, rawRes);
            m_expression = rawRes;
            m_display->setText(m_expression);
            updateBaseLabels(finalInt);
        }
        else
        {
            m_display->setText("Error");
            m_expression.clear();
        }
    }
    else
    {
        // Standard and Scientific decimal execution track
        // Adjust exponential operator format for JavaScript interpretation
        QString safeExpr = m_expression;
        safeExpr.replace("^", "**");

        QJSEngine engine;
        QJSValue result = engine.evaluate(safeExpr);

        if(!result.isError() && result.isNumber())
        {
            double numResult = result.toNumber();
            QString outputText = QString::number(numResult);
            addToHistory(m_expression, outputText);
            m_expression = outputText;
            m_display->setText(m_expression);
        }
        else
        {
            m_display->setText("Error");
            m_expression.clear();
        }
    }
}

void Calculator::updateBaseLabels(long long value)
{
    m_hexLabel->setText("HEX: 0x" + QString::number(value, 16).toUpper());
    m_decLabel->setText("DEC: " + QString::number(value, 10));
    m_octLabel->setText("OCT: 0" + QString::number(value, 8));
    m_binLabel->setText("BIN: b" + QString::number(value, 2));
}

void Calculator::addToHistory(const QString &expr, const QString &res)
{
    m_historyList->addItem(QString("%1 = %2").arg(expr, res));
    m_historyList->scrollToBottom();
}

void Calculator::dispatchHistoryDoubleClicked(const QString &itemText)
{
    QStringList parts = itemText.split("=");
    if(parts.size() == 2)
    {
        m_expression = parts.at(1).trimmed();
        m_display->setText(m_expression);

        if(m_currentMode == CalcMode::Programmer)
        {
            bool ok;
            long long val = m_expression.toLongLong(&ok, static_cast<int>(m_currentBase));
            if(ok) updateBaseLabels(val);
        }
    }
}

void Calculator::dispatchClearHistory()
{
    if(m_historyList)
    {
        m_historyList->clear();
    }
}


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Calculator calc;
    calc.show();

    return app.exec();
}

