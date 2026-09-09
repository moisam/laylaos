#ifndef PAINTCANVAS_H
#define PAINTCANVAS_H

#include <QWidget>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QColor>
#include <QPen>
#include <QList>
#include <QRegion>
#include <functional>

class PaintCanvas : public QWidget 
{
public:
    enum FilterType { Invert, Greyscale, Blur, Sepia, Brightness };

    explicit PaintCanvas(QWidget *parent = nullptr);

    void setTool(const QString &toolName);
    void setBucketMode(const QString &mode);
    void setBrushPattern(const QString &pattern);
    void setPrimaryColor(const QColor &color);
    void setSecondaryColor(const QColor &color);
    void setThickness(int thickness);
    void setLineStyle(Qt::PenStyle style);
    void setAirbrushDensity(int density);
    void setAlpha(int alpha);

    void createNewCanvas(int width, int height);
    void resizeCanvasGeometry(int width, int height);
    bool openImage(const QString &filePath);
    bool saveImage(const QString &filePath);
    
    QPixmap copySelection() const;
    void pasteImage(const QPixmap &pixmap);
    void cropToSelection();
    bool hasSelection() const;
    void clearActiveSelection();

    void undo();
    void redo();
    
    void applyFilter(FilterType type);
    QSize imageSize() const { return m_image.size(); }

    QColor primaryColor() const { return m_primaryColor; }
    QColor secondaryColor() const { return m_secondaryColor; }
    QString activeTool() const { return m_activeTool; }
    QString activeBrushPattern() const { return m_activeBrushPattern; }

    bool isDirty() const { return m_isDirty; }
    void setDirty(bool dirty) { m_isDirty = dirty; }

    std::function<void(const QPoint&)> onMouseMoved = nullptr;
    std::function<void()> onCanvasResized = nullptr;
    std::function<void()> onColorsChanged = nullptr;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drawShape(QPainter &painter, const QPoint &start, const QPoint &end);
    void drawPatternStroke(const QPoint &from, const QPoint &to);
    void floodFill(const QPoint &startPoint, const QColor &fillColor);
    void runMagicWand(const QPoint &startPoint);
    void resizeImage(QImage *image, const QSize &newSize);
    QPen getConfiguredPen() const;
    void saveCurrentStateToHistory();
    bool colorsMatch(QRgb c1, QRgb c2, int tolerance = 15) const;

    QImage m_image;
    QImage m_previewImage;
    
    QString m_activeTool;
    QString m_activeBucketMode;
    QString m_activeBrushPattern;
    QColor m_primaryColor;
    QColor m_secondaryColor;
    int m_lineThickness;
    Qt::PenStyle m_lineStyle;
    int m_airbrushDensity;
    int m_alpha;

    bool m_isDrawing;
    QPoint m_startPoint;
    QPoint m_lastPoint;
    bool m_isDirty;
    
    QRect m_selectionRect;
    QRegion m_wandRegion;

    bool m_isDraggingSelection;
    QPoint m_dragStartMousePos;
    QImage m_dragSelectionContent;  // Holds the cutout image being moved
    QRect m_dragSourceRect;         // Original bounds of selection
    QRegion m_dragSourceWandRegion; // Original wand region mask
    QImage m_imageBeforeDrag;       // Preserves canvas background under selection

    QList<QImage> m_undoStack;
    QList<QImage> m_redoStack;
    const int m_maxHistorySize = 30;
};

#endif // PAINTCANVAS_H
