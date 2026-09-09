#ifndef CURSOR_VIEWER_H
#define CURSOR_VIEWER_H

#include <QWidget>
#include <QMainWindow>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QGridLayout>
#include <vector>

class CursorViewer : public QMainWindow
{
public:
    explicit CursorViewer(QWidget *parent = nullptr);
    bool eventFilter(QObject *watched, QEvent *event) override;
    void processCommandLineArgument(const QString &argPath);
    void discoverCursorThemes();

private:
    struct CursorFrame
    {
        QPixmap pixmap;
        quint32 width{0};
        quint32 height{0};
        quint32 xhot{0};
        quint32 yhot{0};
        quint32 delay{0};
    };

    struct TocEntry
    {
        quint32 type; 
        quint32 subtype; 
        quint32 position; 
    };

    void loadThemeDirectory(const QString &path);
    void resetViewerState();
    void parseAvailableSizes();
    void loadXcursorSize(quint32 targetSizeSubtype);
    void startLivePreviewLoop();
    void tickLiveAnimation();
    void setAnimationPlayback(bool active);
    void createMenuBar();
    void openCustomFolderDialog();
    void handleTargetDirectory(const QString &dirPath, const QString &selectFileName = "");

#ifdef __laylaos__
    void setAsSystemTheme();
#endif

    // UI Controls
    QListWidget *m_fileList;
    QComboBox *m_themeComboBox;
    QComboBox *m_sizeComboBox;
    QScrollArea *m_framesScroll;
    QWidget *m_framesWidget;
    QGridLayout *m_framesGrid;
    QLabel *m_testPadLabel;

    // State Tracking
    QStringList m_searchPaths;
    QString m_currentCursorsPath;
    QString m_currentFilePath;
    QString m_resolvedPath;
    std::vector<TocEntry> m_cachedToc;
    std::vector<CursorFrame> m_parsedFrames;
    QTimer *m_animTimer;
    size_t m_currentFrameIndex{0};
    bool m_isMouseInsideSandbox{false};
};

#endif      /* CURSOR_VIEWER_H */
