#ifndef PAINTMAINWINDOW_H
#define PAINTMAINWINDOW_H

#include <QMainWindow>
#include "paintcanvas.h"

class QComboBox;
class QAction;
class QLabel;
class QGraphicsView;
class QGraphicsScene;
class QSlider;

class PaintMainWindow : public QMainWindow 
{
public:
    PaintMainWindow(QWidget *parent = nullptr);
    bool eventFilter(QObject *watched, QEvent *event) override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    bool maybeSave();
    void changeTool(QAction *senderAction);
    void chooseFrontColor();
    void chooseBackColor();
    void fileNew();
    void fileOpen();
    void fileSave();
    void fileSaveAs();
    void fileResizeCanvas();
    void editCopy();
    void editPaste();
    void editUndo();
    void editRedo();
    void editCrop();
    void helpAbout();
    void helpShortcuts();
    void updateStatusBarCoords(const QPoint &pos);
    void zoomIn();
    void zoomOut();
    void zoomReset();
    void syncSceneBounds();
    void updateVisibilityStates();
    void refreshColorDisplayPanels();
    
    void filterInvert();
    void filterGreyscale();
    void filterBlur();
    void filterSepia();
    void filterBrightness();

    void initMenus();
    void initToolbars();
    void initStatusBar();
    void initPaletteSidebar();
    void setupZoomingView();
    void updateZoomLabel();

    QIcon createVectorIcon(const QString &toolType);

    PaintCanvas *m_canvas;
    QGraphicsView *m_view;
    QGraphicsScene *m_scene;
    
    QString m_currentFilePath;
    QList<QAction*> m_toolActions;
    QLabel *m_coordsLabel;
    QLabel *m_zoomLabel;
    double m_zoomLevel;

    QComboBox *m_styleCombo;
    QComboBox *m_patternCombo;
    QComboBox *m_bucketModeCombo;

    QAction *m_styleAction;
    QAction *m_patternAction;
    QAction *m_bucketModeAction;

    QLabel *m_sprayLabelWidget;
    QSlider *m_spraySliderWidget;
    QAction *m_sprayLabelAction;
    QAction *m_spraySliderAction;

    QLabel *m_frontColorPreview;
    QLabel *m_backColorPreview;
};

#endif // PAINTMAINWINDOW_H
