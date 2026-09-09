#include <QApplication>
#include <QFileDialog>
#include <QDir>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCalendarWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDate>
#include <QTime>
#include <QPainter>
#include <QColor>
#include <QRect>
#include <QComboBox>
#include <QStackedWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QDialog>
#include <QTimeEdit>
#include <QFormLayout>
#include <QMessageBox>
#include <QMetaObject>
#include <QKeyEvent>

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <functional>
#include <algorithm> 

#define APPICON_PATH            "/usr/share/gui/icons/calendar.png"

// Include nlohmann/json header
#include "json.hpp" 
using json = nlohmann::json;

const std::string SAVE_FILE_PATH = "calendar_data.json";

enum class Recurrence { None, Daily, Weekly, Monthly };

// Event Object
struct Event
{
    std::string title;
    std::string type; 
    std::string time; // "HH:MM"
    Recurrence recurrence = Recurrence::None;
    bool alerted = false; 
    std::string colorHex = "#3498db"; // Default category color tag (Sky Blue)

    json toJson() const
    {
        return json{
            {"title", title}, 
            {"type", type}, 
            {"time", time}, 
            {"alerted", alerted},
            {"recurrence", static_cast<int>(recurrence)},
            {"colorHex", colorHex}
        };
    }

    static Event fromJson(const json& j)
    {
        Event ev;
        ev.title = j.value("title", "Untitled Event");
        ev.type = j.value("type", "General");
        ev.time = j.value("time", "12:00");
        ev.alerted = j.value("alerted", false);
        ev.recurrence = static_cast<Recurrence>(j.value("recurrence", 0));
        ev.colorHex = j.value("colorHex", "#3498db");
        return ev;
    }

    QString toDisplayString() const
    {
        QString recText = "";
        if(recurrence == Recurrence::Daily) recText = " [Daily]";
        if(recurrence == Recurrence::Weekly) recText = " [Weekly]";
        if(recurrence == Recurrence::Monthly) recText = " [Monthly]";

        return QString("[%1] %2 - %3%4")
            .arg(QString::fromStdString(type))
            .arg(QString::fromStdString(time))
            .arg(QString::fromStdString(title))
            .arg(recText);
    }
};

// Calendar State container
struct CalendarState
{
    std::map<std::string, std::vector<Event>> events;
    QDate selectedDate = QDate::currentDate();
    mutable std::mutex dataMutex; 

    void loadFromFile()
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::ifstream file(SAVE_FILE_PATH);
        if(!file.is_open()) return;

        try
        {
            json j;
            file >> j;
            events.clear();
            for(auto& [key, eventListJson] : j.items())
            {
                std::vector<Event> parsedEvents;
                for(const auto& item : eventListJson)
                {
                    parsedEvents.push_back(Event::fromJson(item));
                }
                events[key] = parsedEvents;
            }
        }
        catch (...)
        {
            std::cerr << "Error parsing JSON payload." << std::endl;
        }
    }

    void saveToFile() const
    { 
        std::lock_guard<std::mutex> lock(dataMutex); 
        std::ofstream file(SAVE_FILE_PATH);
        if(!file.is_open()) return;
        json j = json::object();
        for(const auto& [dateKey, eventList] : events)
        {
            json listJson = json::array();
            for(const auto& ev : eventList)
            {
                listJson.push_back(ev.toJson());
            }
            j[dateKey] = listJson;
        }
        file << j.dump(4);
    }

    // Resolves and returns all structural and recurring events active on a specific date
    std::vector<Event> getEventsForDate(QDate date) const
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::vector<Event> activeEvents;
        std::string targetKey = date.toString("yyyy-MM-dd").toStdString();

        // Gather one-off standalone events mapped specifically to this date
        if(events.find(targetKey) != events.end())
        {
            for(const auto& ev : events.at(targetKey))
            {
                activeEvents.push_back(ev);
            }
        }

        // Scan database to match ongoing recurring configurations
        for(const auto& [dateKey, evList] : events)
        {
            QDate originDate = QDate::fromString(QString::fromStdString(dateKey), "yyyy-MM-dd");
            
            // Skip future-defined intervals if our grid rendering date precedes it
            // ALSO skip if it's the exact origin date (handled above)
            if(date <= originDate) continue; 

            for(const auto& ev : evList)
            {
                if(ev.recurrence == Recurrence::Daily)
                {
                    activeEvents.push_back(ev);
                }
                else if(ev.recurrence == Recurrence::Weekly && originDate.dayOfWeek() == date.dayOfWeek())
                {
                    activeEvents.push_back(ev);
                }
                else if(ev.recurrence == Recurrence::Monthly && originDate.day() == date.day())
                {
                    activeEvents.push_back(ev);
                }
            }
        }

        // Ensure chronological listing
        std::sort(activeEvents.begin(), activeEvents.end(), [](const Event& a, const Event& b)
        {
            return a.time < b.time;
        });
        return activeEvents;
    }

    bool hasEvents(const std::string& dateKey) const
    {
        QDate targetDate = QDate::fromString(QString::fromStdString(dateKey), "yyyy-MM-dd");
        return !getEventsForDate(targetDate).empty();
    }
};

// Custom Month View
class CustomCalendarWidget : public QCalendarWidget
{
private:
    const CalendarState& m_state;

public:
    explicit CustomCalendarWidget(const CalendarState& state, QWidget *parent = nullptr) 
        : QCalendarWidget(parent), m_state(state) {}

    void forceRefresh()
    {
        this->updateCells();
    }

protected:
    void paintCell(QPainter *painter, const QRect &rect, QDate date) const override
    {
        QCalendarWidget::paintCell(painter, rect, date);
        
        // Fetch all resolved real-time and recurring items for this specific date square
        std::vector<Event> dayEvents = m_state.getEventsForDate(date);
        
        if(!dayEvents.empty())
        {
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);
            
            int dotRadius = 3;
            int spacing = 3;
            int totalDots = static_cast<int>(dayEvents.size());
            if(totalDots > 4) totalDots = 4; // Max visible tracking dots per cell

            // Compute centered baseline coordinates along the bottom section of the day grid
            int totalWidth = (totalDots * (dotRadius * 2)) + ((totalDots - 1) * spacing);
            int startX = rect.center().x() - (totalWidth / 2);
            int posY = rect.bottom() - (dotRadius + 4);

            for(int i = 0; i < totalDots; ++i)
            {
                QColor tagColor(QString::fromStdString(dayEvents[i].colorHex));
                painter->setBrush(tagColor);
                
                int posX = startX + (i * (dotRadius * 2 + spacing)) + dotRadius;
                painter->drawEllipse(QPoint(posX, posY), dotRadius, dotRadius);
            }
            painter->restore();
        }
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CalendarState state;
    state.loadFromFile();

    // Reset alert states on startup so today's upcoming items can still fire
    {
        std::lock_guard<std::mutex> lock(state.dataMutex);
        std::string todayKey = QDate::currentDate().toString("yyyy-MM-dd").toStdString();
        if(state.events.find(todayKey) != state.events.end())
        {
            for(auto& ev : state.events[todayKey])
            {
                ev.alerted = false;
            }
        }
    }

    QWidget mainWindow;
    mainWindow.setWindowTitle("Calendar");
    mainWindow.resize(600, 400);
    mainWindow.setWindowIcon(QIcon(APPICON_PATH));

    QHBoxLayout *mainLayout = new QHBoxLayout(&mainWindow);
    QVBoxLayout *leftLayout = new QVBoxLayout();
    QVBoxLayout *rightLayout = new QVBoxLayout();

    QHBoxLayout *navControlsLayout = new QHBoxLayout();
    QComboBox *viewSelector = new QComboBox(&mainWindow);
    viewSelector->addItems({"Month View", "Week View", "Year View"});

    QPushButton *prevButton = new QPushButton("< Prev", &mainWindow);
    QPushButton *nextButton = new QPushButton("Next >", &mainWindow);

    prevButton->setMaximumWidth(60);
    nextButton->setMaximumWidth(60);

    navControlsLayout->addWidget(viewSelector, 2);
    navControlsLayout->addWidget(prevButton, 1);
    navControlsLayout->addWidget(nextButton, 1);
    leftLayout->addLayout(navControlsLayout);

    QStackedWidget *viewStack = new QStackedWidget(&mainWindow);
    leftLayout->addWidget(viewStack, 1);

    QLabel *selectedDateLabel = new QLabel(&mainWindow);
    QLineEdit *searchBar = new QLineEdit(&mainWindow);
    QListWidget *eventList = new QListWidget(&mainWindow);
    QPushButton *addButton = new QPushButton("Create Event...", &mainWindow);
    QPushButton *deleteButton = new QPushButton("Cancel Selected Event", &mainWindow);
    QPushButton *exportButton = new QPushButton("Export Agenda to CSV...", &mainWindow);

    searchBar->setPlaceholderText("Search events (title or category)...");

    rightLayout->addWidget(selectedDateLabel);
    rightLayout->addWidget(searchBar);
    rightLayout->addWidget(eventList);
    rightLayout->addWidget(addButton);
    rightLayout->addWidget(deleteButton);
    rightLayout->addWidget(exportButton);

    mainLayout->addLayout(leftLayout, 2);
    mainLayout->addLayout(rightLayout, 1);

    // Initialize calendar layouts
    // Month view
    CustomCalendarWidget *monthCalendar = new CustomCalendarWidget(state, viewStack);
    viewStack->addWidget(monthCalendar);

    // Week view
    QWidget *weekRootWidget = new QWidget(viewStack);
    QVBoxLayout *weekRootLayout = new QVBoxLayout(weekRootWidget);
    weekRootLayout->setContentsMargins(0,0,0,0);
    weekRootLayout->setSpacing(0);

    QLabel *weekHeaderBar = new QLabel("Week Details", weekRootWidget);
    weekHeaderBar->setAlignment(Qt::AlignCenter);
    weekHeaderBar->setStyleSheet("background-color: #3498db; color: white; font-weight: bold; font-size: 12px; padding: 6px;");
    weekRootLayout->addWidget(weekHeaderBar);

    QWidget *weekGridContainer = new QWidget(weekRootWidget);
    QHBoxLayout *weekLayout = new QHBoxLayout(weekGridContainer);
    std::vector<QPushButton*> weekDaysButtons;

    for(int i = 0; i < 7; ++i)
    {
        QPushButton *dayBtn = new QPushButton(weekGridContainer);
        dayBtn->setCheckable(true);
        dayBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        weekLayout->addWidget(dayBtn);
        weekDaysButtons.push_back(dayBtn);
    }

    weekRootLayout->addWidget(weekGridContainer, 1);
    viewStack->addWidget(weekRootWidget);

    // Year view
    QWidget *yearRootWidget = new QWidget(viewStack);
    QVBoxLayout *yearRootLayout = new QVBoxLayout(yearRootWidget);
    yearRootLayout->setContentsMargins(0,0,0,0);
    yearRootLayout->setSpacing(0);

    QLabel *yearHeaderBar = new QLabel("Year Details", yearRootWidget);
    yearHeaderBar->setAlignment(Qt::AlignCenter);
    yearHeaderBar->setStyleSheet("background-color: #3498db; color: white; font-weight: bold; font-size: 12px; padding: 6px;");
    yearRootLayout->addWidget(yearHeaderBar);

    QScrollArea *yearScroll = new QScrollArea(yearRootWidget);
    QWidget *yearContainer = new QWidget(yearScroll);
    QGridLayout *yearGridLayout = new QGridLayout(yearContainer);
    std::vector<QPushButton*> yearMonthButtons;
    const QStringList monthNames = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for(int i = 0; i < 12; ++i)
    {
        QPushButton *monthBtn = new QPushButton(monthNames[i], yearContainer);
        monthBtn->setMinimumSize(80, 80);
        yearGridLayout->addWidget(monthBtn, i / 4, i % 4);
        yearMonthButtons.push_back(monthBtn);
    }
    yearScroll->setWidget(yearContainer);
    yearScroll->setWidgetResizable(true);
    yearRootLayout->addWidget(yearScroll, 1);
    viewStack->addWidget(yearRootWidget);

    // Event Filter to capture keyboard interactions (DEL and ENTER keys)
    class KeyPressFilter : public QObject
    {
    private:
        std::function<void()> m_onDelete;
        std::function<void()> m_onEnter;
    public:
        KeyPressFilter(std::function<void()> onDelete, std::function<void()> onEnter, QObject* parent = nullptr)
            : QObject(parent), m_onDelete(onDelete), m_onEnter(onEnter) {}
    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            if(event->type() == QEvent::KeyPress)
            {
                QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
                if(keyEvent->key() == Qt::Key_Delete)
                {
                    m_onDelete();
                    return true;
                }
                else if(keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
                {
                    m_onEnter();
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }
    };

    // Event Filter to capture Mouse Double-Clicks on Week Buttons
    class MouseDoubleClickFilter : public QObject
    {
    private:
        std::function<void()> m_onDoubleClick;
    public:
        MouseDoubleClickFilter(std::function<void()> onDoubleClick, QObject* parent = nullptr)
            : QObject(parent), m_onDoubleClick(onDoubleClick) {}
    protected:
        bool eventFilter(QObject *obj, QEvent *event) override
        {
            if(event->type() == QEvent::MouseButtonDblClick)
            {
                m_onDoubleClick();
                return true;
            }
            return QObject::eventFilter(obj, event);
        }
    };

    // Interface refresh pipeline
    std::function<void()> updateUI;

    auto refreshViews = [&]()
    {
        monthCalendar->setSelectedDate(state.selectedDate);

        // Recalculate Monday based on the updated state.selectedDate value
        int dayOfWeek = state.selectedDate.dayOfWeek();
        QDate mondayOfWeek = state.selectedDate.addDays(1 - dayOfWeek);
        
        for(int i = 0; i < 7; ++i)
        {
            QDate currentDay = mondayOfWeek.addDays(i);
            std::string key = currentDay.toString("yyyy-MM-dd").toStdString();
            QString text = currentDay.toString("ddd\nd\nMMM");

            // Query matching to fetch recurring entries
            size_t count = state.getEventsForDate(currentDay).size();
            if(count > 0)
            {
                text += QString("\n• (%1)").arg(count);
            }
            weekDaysButtons[i]->setText(text);
            weekDaysButtons[i]->setChecked(currentDay == state.selectedDate);
        }

        // Year View: Check event matching counts across months
        for(int i = 0; i < 12; ++i)
        {
            int year = state.selectedDate.year();
            int month = i + 1;
            int eventCount = 0;

            // Loop through all days of this month to compute event counts
            QDate checkDate(year, month, 1);
            while(checkDate.isValid() && checkDate.month() == month)
            {
                eventCount += state.getEventsForDate(checkDate).size();
                checkDate = checkDate.addDays(1);
            }
            
            if(eventCount > 0)
            {
                yearMonthButtons[i]->setText(QString("%1\n(%2)").arg(monthNames[i]).arg(eventCount));
                yearMonthButtons[i]->setStyleSheet("background-color: #3498db; color: white; border-radius: 4px; font-weight: bold;");
            }
            else
            {
                yearMonthButtons[i]->setText(monthNames[i]);
                yearMonthButtons[i]->setStyleSheet("background-color: none; color: black; border: 1px solid #ccc; border-radius: 4px;");
            }
        }

        // Synchronize header bars
        weekHeaderBar->setText(QString("Week of: %1 - %2")
            .arg(mondayOfWeek.toString("MMMM d, yyyy"))
            .arg(mondayOfWeek.addDays(6).toString("MMMM d, yyyy")));
            
        yearHeaderBar->setText(QString("%1").arg(state.selectedDate.year()));
        monthCalendar->update();
    };

    auto updateEventList = [&]()
    {
        QString searchQuery = searchBar->text().trimmed().toLower();
        eventList->clear();
        deleteButton->setEnabled(false);

        auto populateItem = [](QListWidget* listWidget, const Event& ev, const QString& prefix = "")
        {
            QListWidgetItem* item = new QListWidgetItem(listWidget);
            
            // Generate visual color block tags alongside string contents
            QString displayText = prefix + ev.toDisplayString();
            item->setText(displayText);
            
            // Inject color category indicators using background color shading
            QColor tagColor(QString::fromStdString(ev.colorHex));
            QPixmap pix(12, 12);
            pix.fill(tagColor);
            item->setIcon(QIcon(pix));
            
            listWidget->addItem(item);
        };

        if(searchQuery.isEmpty())
        {
            std::string dateKey = state.selectedDate.toString("yyyy-MM-dd").toStdString();
            selectedDateLabel->setText("Agenda for:\n" + state.selectedDate.toString("MMMM d, yyyy"));

            std::vector<Event> dayEvents = state.getEventsForDate(state.selectedDate);
            for (const auto& ev : dayEvents) {
                populateItem(eventList, ev);
            }
        }
        else
        {
            selectedDateLabel->setText(QString("Search results for: '%1'").arg(searchBar->text()));
            std::lock_guard<std::mutex> lock(state.dataMutex);
            
            for(const auto& [dateStr, list] : state.events)
            {
                QDate eventDate = QDate::fromString(QString::fromStdString(dateStr), "yyyy-MM-dd");
                for(const auto& ev : list)
                {
                    QString title = QString::fromStdString(ev.title).toLower();
                    QString type = QString::fromStdString(ev.type).toLower();
                    if(title.contains(searchQuery) || type.contains(searchQuery))
                    {
                        QString prefix = eventDate.toString("yyyy-MM-dd") + " | ";
                        populateItem(eventList, ev, prefix);
                    }
                }
            }

            if(eventList->count() == 0)
            {
                eventList->addItem("No matching events found.");
            }
        }
    };

    updateUI = [&]()
    {
        refreshViews();
        updateEventList();
    };

    updateUI();

    // Event Editor/Creator
    auto openEventEditor = [&](bool isEditing, Event* targetEvent = nullptr)
    {
        QDialog dialog(&mainWindow);
        dialog.setWindowTitle(isEditing ? "Edit Event" : "Add New Event");
        QFormLayout form(&dialog);

        QLineEdit *titleInput = new QLineEdit(&dialog);
        QComboBox *typeInput = new QComboBox(&dialog);
        typeInput->addItems({"Meeting", "Doctor", "Personal", "Task", "Holiday"});
        QTimeEdit *timeInput = new QTimeEdit(&dialog);
        timeInput->setDisplayFormat("HH:mm");
        QComboBox *recurrenceInput = new QComboBox(&dialog);
        recurrenceInput->addItems({"None", "Daily", "Weekly", "Monthly"});

        // Color Category Selection Dropdown
        QComboBox *colorInput = new QComboBox(&dialog);
        colorInput->addItem("Blue (Default)", "#3498db");
        colorInput->addItem("Green (Work)", "#2ecc71");
        colorInput->addItem("Red (Urgent)", "#e74c3c");
        colorInput->addItem("Yellow (Task)", "#f1c40f");
        colorInput->addItem("Purple (Personal)", "#9b59b6");

        if(isEditing && targetEvent)
        {
            titleInput->setText(QString::fromStdString(targetEvent->title));
            typeInput->setCurrentText(QString::fromStdString(targetEvent->type));
            timeInput->setTime(QTime::fromString(QString::fromStdString(targetEvent->time), "HH:mm"));
            recurrenceInput->setCurrentIndex(static_cast<int>(targetEvent->recurrence));

            int colorIdx = colorInput->findData(QString::fromStdString(targetEvent->colorHex));
            if(colorIdx >= 0) colorInput->setCurrentIndex(colorIdx);
        }
        else
        {
            timeInput->setTime(QTime::currentTime());
        }

        form.addRow("Event Title:", titleInput);
        form.addRow("Category Type:", typeInput);
        form.addRow("Start Time:", timeInput);
        form.addRow("Recurrence Pattern:", recurrenceInput);
        form.addRow("Color Tag Category:", colorInput);

        QPushButton *saveButton = new QPushButton("Save Changes", &dialog);
        form.addRow(saveButton);

        QObject::connect(saveButton, &QPushButton::clicked, &dialog, &QDialog::accept);

        if(dialog.exec() == QDialog::Accepted)
        {
            QString title = titleInput->text().trimmed();

            if(!title.isEmpty())
            {
                std::string selectedColor = colorInput->currentData().toString().toStdString();

                if(isEditing && targetEvent)
                {
                    // Scope block to release the lock immediately before calling other functions
                    {
                        std::lock_guard<std::mutex> lock(state.dataMutex);
                        for(auto& [dateKey, list] : state.events)
                        {
                            for(auto& ev : list)
                            {
                                if(ev.title == targetEvent->title && ev.time == targetEvent->time && ev.type == targetEvent->type)
                                {
                                    ev.title = title.toStdString();
                                    ev.type = typeInput->currentText().toStdString();
                                    ev.time = timeInput->time().toString("HH:mm").toStdString();
                                    ev.recurrence = static_cast<Recurrence>(recurrenceInput->currentIndex());
                                    ev.colorHex = selectedColor;
                                    ev.alerted = false;
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    Event newEvent
                    {
                        title.toStdString(),
                        typeInput->currentText().toStdString(),
                        timeInput->time().toString("HH:mm").toStdString(),
                        static_cast<Recurrence>(recurrenceInput->currentIndex()),
                        false,
                        selectedColor,
                    };

                    std::string dateKey = state.selectedDate.toString("yyyy-MM-dd").toStdString();
                    {
                        std::lock_guard<std::mutex> lock(state.dataMutex);
                        state.events[dateKey].push_back(newEvent);
                    }
                }
                
                // Sort chronology inside a distinct local lock window
                {
                    std::lock_guard<std::mutex> lock(state.dataMutex);
                    for(auto& [dateKey, list] : state.events)
                    {
                        std::sort(list.begin(), list.end(), [](const Event& a, const Event& b) { return a.time < b.time; });
                    }
                }
                
                state.saveToFile();
                updateUI();
            }
        }
    };

    // Standard connections
    QObject::connect(viewSelector, &QComboBox::currentIndexChanged, viewStack, &QStackedWidget::setCurrentIndex);

    // Real-time Search: Re-filter event list on every character typed
    QObject::connect(searchBar, &QLineEdit::textChanged, [&]()
    {
        updateEventList();
    });

    QObject::connect(monthCalendar, &QCalendarWidget::selectionChanged, [&]()
    {
        state.selectedDate = monthCalendar->selectedDate();
        updateUI();
    });

    // Month View: Double-clicking a grid day opens the event editor
    QObject::connect(monthCalendar, &QCalendarWidget::activated, [&]()
    {
        openEventEditor(false); 
    });

    for(int i = 0; i < 7; ++i)
    {
        // Single click updates active selection
        QObject::connect(weekDaysButtons[i], &QPushButton::clicked, [=, &state, &updateUI]()
        {
            int currentDayOfWeek = state.selectedDate.dayOfWeek();
            QDate currentMonday = state.selectedDate.addDays(1 - currentDayOfWeek);
            state.selectedDate = currentMonday.addDays(i);
            updateUI();
        });

        // Double-clicking updates selection AND pops open the Event Editor
        MouseDoubleClickFilter *weekDblClickFilter = new MouseDoubleClickFilter([=, &state, &updateUI]()
        {
            int currentDayOfWeek = state.selectedDate.dayOfWeek();
            QDate currentMonday = state.selectedDate.addDays(1 - currentDayOfWeek);
            state.selectedDate = currentMonday.addDays(i);
            updateUI();
            
            // Trigger the centralized event creator workflow
            openEventEditor(false);
        }, &mainWindow);
        
        weekDaysButtons[i]->installEventFilter(weekDblClickFilter);
    }

    for(int i = 0; i < 12; ++i)
    {
        QObject::connect(yearMonthButtons[i], &QPushButton::clicked, [=, &state, &updateUI]()
        {
            state.selectedDate = QDate(state.selectedDate.year(), i + 1, 1);
            viewSelector->setCurrentIndex(0);updateUI();
        });
    }

    // Add Event
    QObject::connect(addButton, &QPushButton::clicked, [&]() { openEventEditor(false); });

    // Export Event
    QObject::connect(exportButton, &QPushButton::clicked, [=, &state, &mainWindow]()
    {
        QString selectedFilePath = QFileDialog::getSaveFileName(
            &mainWindow,
            "Export Schedule Data",
            QDir::homePath() + "/calendar_export.csv",
            "Comma Separated Values (*.csv);;Plain Text Files (*.txt)"
        );

        if(!selectedFilePath.isEmpty())
        {
            std::ofstream exportFile(selectedFilePath.toStdString());
            
            if(!exportFile.is_open())
            {
                QMessageBox::critical(&mainWindow, "Export Failure", "Could not open or write to target directory location.");
                return;
            }

            // Write structured spreadsheet configuration columns metadata rows first
            exportFile << "Date,Time,Category,Event Title,Recurrence Rule\n";

            std::lock_guard<std::mutex> lock(state.dataMutex);
            size_t totalsExportedCount = 0;

            for(const auto& [dateKeyStr, recordsList] : state.events)
            {
                for(const auto& ev : recordsList)
                {
                    // Sanitise string data lines against accidental embedded commas or quotes
                    std::string protectedTitle = ev.title;
                    if(protectedTitle.find(',') != std::string::npos || protectedTitle.find('"') != std::string::npos)
                    {
                        // Enclose text inside double quotes to protect field parsing boundaries
                        protectedTitle = "\"" + protectedTitle + "\"";
                    }

                    std::string ruleText = "None";
                    if(ev.recurrence == Recurrence::Daily) ruleText = "Daily";
                    if(ev.recurrence == Recurrence::Weekly) ruleText = "Weekly";
                    if(ev.recurrence == Recurrence::Monthly) ruleText = "Monthly";

                    // Output records sequentially
                    exportFile << dateKeyStr << ","
                               << ev.time << ","
                               << ev.type << ","
                               << protectedTitle << ","
                               << ruleText << "\n";
                               
                    totalsExportedCount++;
                }
            }

            exportFile.close(); // Flush memory streams and lock down file storage

            QMessageBox::information(
                &mainWindow, 
                "Export Complete", 
                QString("Successfully compiled spreadsheet matrix records!\n\nTotal rows exported: %1")
                    .arg(totalsExportedCount)
            );
        }
    });

    // Delete Event
    QObject::connect(deleteButton, &QPushButton::clicked, [&]()
    {
        int currentRow = eventList->currentRow();
        if(currentRow >= 0)
        {
            QString selectedText = eventList->currentItem()->text();
            
            // Skip deletion if user mistakenly highlights the "No items found" notification
            if(selectedText.startsWith("No matching")) return;

            std::string targetDateKey = "";
            std::string cleanTitle = "";
            std::string cleanType = "";
            std::string cleanTime = "";

            // Parse text data fields depending on whether we are in search mode or regular view
            if(selectedText.contains(" | "))
            {
                QStringList parts = selectedText.split(" | ");
                targetDateKey = parts[0].trimmed().toStdString();
                
                QString details = parts[1]; // e.g. "[Meeting] 14:00 - Team Sync"
                int typeEnd = details.indexOf(']');
                if(typeEnd != -1)
                {
                    cleanType = details.mid(1, typeEnd - 1).toStdString();
                    cleanTime = details.mid(typeEnd + 2, 5).toStdString();
                    
                    // Accommodate extra string padding depending on recurrence bracket tags
                    int hyphenIdx = details.indexOf('-', typeEnd);
                    if(hyphenIdx != -1)
                    {
                        cleanTitle = details.mid(hyphenIdx + 2).toStdString();
                        // Strip out trailing recurrence suffix text from title if present
                        size_t bracketPos = cleanTitle.find(" [");
                        if (bracketPos != std::string::npos) {
                            cleanTitle = cleanTitle.substr(0, bracketPos);
                        }
                    }
                }
            }
            else
            {
                std::vector<Event> dayEvents = state.getEventsForDate(state.selectedDate);
                if(currentRow < static_cast<int>(dayEvents.size()))
                {
                    Event target = dayEvents[currentRow];
                    cleanTitle = target.title;
                    cleanType = target.type;
                    cleanTime = target.time;
                }
            }

            std::string keyToErase = "";

            // Lock, alter data structures, and unlock before returning control
            // back down to file writing and UI threads
            {
                std::lock_guard<std::mutex> lock(state.dataMutex);
                bool foundAndErased = false;
                
                for(auto& [dateKey, list] : state.events)
                {
                    if(!targetDateKey.empty() && dateKey != targetDateKey) continue;

                    for(auto listIt = list.begin(); listIt != list.end(); ++listIt)
                    {
                        if(listIt->title == cleanTitle && listIt->time == cleanTime && listIt->type == cleanType)
                        {
                            list.erase(listIt);
                            foundAndErased = true;
                            if(list.empty())
                            {
                                keyToErase = dateKey;
                            }
                            break;
                        }
                    }
                    if(foundAndErased) break;
                }

                if(!keyToErase.empty())
                {
                    state.events.erase(keyToErase);
                }
            }

            state.saveToFile();
            updateUI();

            // Force the main calendar widget to redraw all visible cells
            monthCalendar->forceRefresh(); 
        }
    });

    // --- PREV / NEXT NAVIGATION HANDLERS ---
    QObject::connect(prevButton, &QPushButton::clicked, [&]()
    {
        int index = viewSelector->currentIndex();
        if(index == 0)
        { // Month View
            state.selectedDate = state.selectedDate.addMonths(-1);
        }
        else if(index == 1)
        { // Week View
            state.selectedDate = state.selectedDate.addDays(-7);
        }
        else if(index == 2)
        { // Year View
            state.selectedDate = state.selectedDate.addYears(-1);
        }
        updateUI();
    });

    QObject::connect(nextButton, &QPushButton::clicked, [&]()
    {
        int index = viewSelector->currentIndex();
        if(index == 0)
        {
            state.selectedDate = state.selectedDate.addMonths(1);
        }
        else if(index == 1)
        {
            state.selectedDate = state.selectedDate.addDays(7);
        }
        else if(index == 2)
        {
            state.selectedDate = state.selectedDate.addYears(1);
        }
        updateUI();
    });

    // Ensure button starts greyed out on initialization
    deleteButton->setEnabled(false);

    // Track active item selections on row modifications
    QObject::connect(eventList, &QListWidget::currentRowChanged, [=](int currentRow)
    {
        deleteButton->setEnabled(currentRow >= 0);
    });

    // Helper function to safely locate and open an event reference anywhere in the interface
    auto handleEventEditingSelection = [&](int currentRow)
    {
        if(currentRow < 0) return;
        
        QString selectedText = eventList->item(currentRow)->text();
        if(selectedText.startsWith("No matching")) return;

        std::string targetDateKey = "";
        std::string cleanTitle = "";
        std::string cleanType = "";
        std::string cleanTime = "";

        // Check if we are interacting with a search listing vs standard agenda row
        if(selectedText.contains(" | "))
        {
            QStringList parts = selectedText.split(" | ");
            targetDateKey = parts[0].trimmed().toStdString();
            
            QString details = parts[1]; // e.g. "[Meeting] 14:00 - Team Sync"
            int typeEnd = details.indexOf(']');
            if(typeEnd != -1)
            {
                cleanType = details.mid(1, typeEnd - 1).toStdString();
                cleanTime = details.mid(typeEnd + 2, 5).toStdString();
                
                int hyphenIdx = details.indexOf('-', typeEnd);
                if(hyphenIdx != -1)
                {
                    cleanTitle = details.mid(hyphenIdx + 2).toStdString();
                    size_t bracketPos = cleanTitle.find(" [");
                    if(bracketPos != std::string::npos)
                    {
                        cleanTitle = cleanTitle.substr(0, bracketPos);
                    }
                }
            }
        }
        else
        {
            // Standard agenda view row mapping 
            std::vector<Event> dayEvents = state.getEventsForDate(state.selectedDate);
            if(currentRow < static_cast<int>(dayEvents.size()))
            {
                Event target = dayEvents[currentRow];
                cleanTitle = target.title;
                cleanType = target.type;
                cleanTime = target.time;
            }
        }

        // Search the master repository map to get a stable pointer reference to the actual item
        Event *eventRefToModify = nullptr;
        {
            std::lock_guard<std::mutex> lock(state.dataMutex);
            for(auto& [dateKey, list] : state.events)
            {
                if(!targetDateKey.empty() && dateKey != targetDateKey) continue;
                for(auto& ev : list)
                {
                    if(ev.title == cleanTitle && ev.time == cleanTime && ev.type == cleanType)
                    {
                        eventRefToModify = &ev;
                        break;
                    }
                }
                if(eventRefToModify) break;
            }
        }

        // If found, forward the real storage reference pointer into the editor dialog box
        if(eventRefToModify)
        {
            openEventEditor(true, eventRefToModify);
        }
    };

    // Trigger Edit on Double-Click Interaction
    QObject::connect(eventList, &QListWidget::itemDoubleClicked, [&]()
    {
        handleEventEditingSelection(eventList->currentRow());
    });

    // Instantiate and register our filter directly onto the QListWidget viewport instance
    KeyPressFilter *listKeyFilter = new KeyPressFilter(
        [&]() { deleteButton->click(); }, // Action on Delete
        [&]() {                           // Action on Enter
            handleEventEditingSelection(eventList->currentRow());
        }, 
        &mainWindow
    );

    eventList->installEventFilter(listKeyFilter);

    // --- BACKGROUND NOTIFICATION ENGINE ---
    std::atomic runNotificationThread{true};

    std::thread notificationWorker([&state, &runNotificationThread, &mainWindow]()
    {
        while(runNotificationThread)
        {
            // Check time intervals every 5 seconds
            std::this_thread::sleep_for(std::chrono::seconds(5));
            QDate today = QDate::currentDate();
            QTime now = QTime::currentTime();
            std::string dateKey = today.toString("yyyy-MM-dd").toStdString();
            std::string timeKey = now.toString("HH:mm").toStdString();
            std::lock_guard<std::mutex> lock(state.dataMutex);

            if(state.events.find(dateKey) != state.events.end())
            {
                for(auto& ev : state.events[dateKey])
                {
                    // Match the hour and minute while ensuring the alert hasn't already fired
                    if(ev.time == timeKey && !ev.alerted)
                    {
                        ev.alerted = true; // Mark as fired

                        // Copy fields for the lambda capture closure
                        QString title = QString::fromStdString(ev.title);
                        QString type = QString::fromStdString(ev.type);
                        QString timeStr = QString::fromStdString(ev.time);

                        // Push execution onto the main GUI thread to open a message box
                        QMetaObject::invokeMethod(&mainWindow, [&mainWindow, title, type, timeStr]()
                        {
                            QMessageBox::information(&mainWindow,
                                QString("Event Alert: %1").arg(type),
                                QString("It is now %1!\n\nEvent: %2").arg(timeStr).arg(title));
                        }, Qt::QueuedConnection);
                    }
                }
            }
        }
    });

    mainWindow.show();

    int execResult = app.exec();

    // Clean up worker threads upon application exit
    runNotificationThread = false;
    if(notificationWorker.joinable())
    {
        notificationWorker.join();
    }

    return execResult;
}

