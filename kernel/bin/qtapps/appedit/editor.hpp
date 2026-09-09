#ifndef APPEDITOR_H
#define APPEDITOR_H

#include <QMainWindow>
#include <QLabel>
#include <QTextEdit>
#include <QFileSystemModel>
#include <QTreeView>
#include <QSplitter>

#include <thread>
#include <mutex>

#include <hunspell.h>

struct HighlightRule
{
    QRegularExpression pattern;
    QTextCharFormat format;
};

struct TabBundle
{
    QTextEdit* mainEditor = nullptr;
    QTextEdit* lineNumberPanel = nullptr;
    QTextEdit* minimapPanel = nullptr;
    QString filePath = "";
    QString activeLangKey = "txt";
};

class Editor : public QMainWindow
{
public:
    Editor(QWidget *parent = nullptr);
    ~Editor();
    void openFile(const QString &fileName);

    QTextEdit* currentTextEdit();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QTabWidget *tabWidget;
    QLabel *statusLabel;
    QLabel *cursorPositionLabel;
    QSplitter *mainSplitter;
    QTimer *autoSaveTimer;
    bool isDarkMode;
    int lastSidebarWidth = 240;

    std::map<QWidget*, TabBundle> tabsMap;

    QMenu *recentFilesMenu;
    QStringList recentFilesList;
    static const int MaxRecentFiles = 5;

    // Asynchronous Hunspell Engine Pointers
    Hunhandle* hunspellEngine;
    std::thread spellCheckThread;
    std::atomic<bool> stopSpellCheck;
    std::atomic<bool> spellCheckPending;
    std::mutex textMutex;
    std::string textToValidate;
    QTextEdit* spellCheckTargetEdit = nullptr;

    // File Browser Sidebar Components
    QTreeView *fileTreeView;
    QFileSystemModel *fileSystemModel;

    // Pre-compiled rule repository cache maps
    // Maps an extension string (e.g., "cpp", "py") to its pre-allocated rules list
    std::map<QString, std::vector<HighlightRule>> languageRulesRepository;
    
    // Core structural formatting properties cached at startup
    QTextCharFormat keywordFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat tagFormat;

    // private functions
    void initializeLanguageRepository();
    QString detectLanguageKey(const QString &filePath);
    void setupUI();
    TabBundle currentBundle();
    void checkBracketMatching(QTextEdit* editor);
    void addNewTab(const QString &title = "Untitled", const QString &filePath = "");
    void closeTab(int index);
    void applySyntaxHighlighting(QTextEdit *textEdit);
    void applyThemeToBundle(const TabBundle& bundle);
    void showCustomContextMenu(QTextEdit* editor, const QPoint &pos);
    void updateLineNumbers(QTextEdit* mainEditor, QTextEdit* lineNumbers);
    void saveFile();
    void saveCurrentFile();
    void exportCurrentToPdf();
    void setupAutoSave();
    void adjustRecentFiles(const QString &filePath);
    void loadRecentFiles();
    void updateRecentFilesMenu();
    void triggerAsyncSpellCheck(QTextEdit* activeEdit);
    void startSpellCheckWorker();
    void highlightMisspellings(QTextEdit* targetEdit, const std::vector<std::pair<int, int>>& ranges);
    void updateStatusBar();
    void showFindReplaceDialog();
    void toggleDarkMode();
    void toggleSidebar();
};

#endif      /* APPEDITOR_H */
