#include "paintcanvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QBrush>
#include <QInputDialog>
#include <QFont>
#include <QLinearGradient>
#include <QPainterPath>
#include <QBitmap>
#include <queue>
#include <cmath>
#include <random>

PaintCanvas::PaintCanvas(QWidget *parent)
    : QWidget(parent), m_activeTool("Free Draw"), m_activeBucketMode("Solid"), 
      m_activeBrushPattern("Solid"), m_primaryColor(Qt::black), m_secondaryColor(Qt::white), 
      m_lineThickness(2), m_lineStyle(Qt::SolidLine), m_airbrushDensity(30), m_alpha(255), 
      m_isDrawing(false), m_isDraggingSelection(false), m_isDirty(false)
{
    setAttribute(Qt::WA_StaticContents);
    setMouseTracking(true);
    m_image = QImage(QSize(800, 600), QImage::Format_ARGB32);
    m_image.fill(Qt::white);
}

void PaintCanvas::createNewCanvas(int width, int height) 
{
    saveCurrentStateToHistory();
    m_isDrawing = false;
    m_isDirty = false;
    m_image = QImage(QSize(width, height), QImage::Format_ARGB32);
    m_image.fill(Qt::white);
    clearActiveSelection();
    resize(m_image.size());
    if(onCanvasResized) onCanvasResized();
    update();
}

void PaintCanvas::resizeCanvasGeometry(int width, int height) 
{
    if(width == m_image.width() && height == m_image.height()) return;
    saveCurrentStateToHistory();
    resizeImage(&m_image, QSize(width, height));
    m_isDirty = true;
    resize(m_image.size());
    if(onCanvasResized) onCanvasResized();
    update();
}

void PaintCanvas::setTool(const QString &toolName) 
{ 
    m_activeTool = toolName;
    clearActiveSelection();
}

void PaintCanvas::clearActiveSelection() 
{
    if(m_isDraggingSelection) 
    {
        m_isDraggingSelection = false;
    }

    m_selectionRect = QRect();
    m_wandRegion = QRegion();
    m_dragSelectionContent = QImage();
    update();
}

void PaintCanvas::setBucketMode(const QString &mode) { m_activeBucketMode = mode; }
void PaintCanvas::setBrushPattern(const QString &pattern) { m_activeBrushPattern = pattern; }

void PaintCanvas::setPrimaryColor(const QColor &color) 
{ 
    m_primaryColor = color;
    m_primaryColor.setAlpha(m_alpha);
    if(onColorsChanged) onColorsChanged();
}

void PaintCanvas::setSecondaryColor(const QColor &color) 
{ 
    m_secondaryColor = color;
    m_secondaryColor.setAlpha(m_alpha);
    if(onColorsChanged) onColorsChanged();
}

void PaintCanvas::setThickness(int thickness) { m_lineThickness = thickness; }
void PaintCanvas::setLineStyle(Qt::PenStyle style) { m_lineStyle = style; }
void PaintCanvas::setAirbrushDensity(int density) { m_airbrushDensity = density; }

void PaintCanvas::setAlpha(int alpha) 
{ 
    m_alpha = alpha; 
    m_primaryColor.setAlpha(m_alpha);
    m_secondaryColor.setAlpha(m_alpha);
    if(onColorsChanged) onColorsChanged();
}

bool PaintCanvas::hasSelection() const 
{
    if(m_activeTool == "Select") return !m_selectionRect.isEmpty();
    if(m_activeTool == "Magic Wand") return !m_wandRegion.isEmpty();
    return false;
}

QPen PaintCanvas::getConfiguredPen() const 
{
    QColor penColor = (m_activeTool == "Eraser") ? Qt::white : m_primaryColor;
    QPen pen(penColor, m_lineThickness, m_lineStyle);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

void PaintCanvas::saveCurrentStateToHistory() 
{
    m_undoStack.append(m_image);
    m_redoStack.clear();
    if(m_undoStack.size() > m_maxHistorySize) m_undoStack.removeFirst();
}

void PaintCanvas::undo() 
{
    if(m_undoStack.isEmpty()) return;
    m_redoStack.append(m_image);
    m_image = m_undoStack.takeLast();
    update();
}

void PaintCanvas::redo() 
{
    if(m_redoStack.isEmpty()) return;
    m_undoStack.append(m_image);
    m_image = m_redoStack.takeLast();
    update();
}

bool PaintCanvas::colorsMatch(QRgb c1, QRgb c2, int tolerance) const 
{
    //if (tolerance == 0) return c1 == c2;
    int dr = std::abs(qRed(c1) - qRed(c2));
    int dg = std::abs(qGreen(c1) - qGreen(c2));
    int db = std::abs(qBlue(c1) - qBlue(c2));
    return (dr <= tolerance && dg <= tolerance && db <= tolerance);
}

void PaintCanvas::floodFill(const QPoint &startPoint, const QColor &fillColor) 
{
    int width = m_image.width();
    int height = m_image.height();
    
    if(startPoint.x() < 0 || startPoint.x() >= width || startPoint.y() < 0 || startPoint.y() >= height) return;

    QRgb targetRgba = m_image.pixel(startPoint);
    QRgb fillRgba = fillColor.rgba();
    
    if(m_activeBucketMode == "Solid" && colorsMatch(targetRgba, fillRgba, 2)) return;

    // Use a fast flat vector bitmask to track visited pixels
    std::vector<bool> visited(width * height, false);
    std::vector<QPoint> filledPixels;
    filledPixels.reserve(width * height / 4);

    std::queue<QPoint> q;
    q.push(startPoint);
    visited[startPoint.y() * width + startPoint.x()] = true;

    int minX = startPoint.x(), maxX = startPoint.x();
    int minY = startPoint.y(), maxY = startPoint.y();

    // Breadth-First-Search (BFS) Pixel Mask Generation
    while(!q.empty()) 
    {
        QPoint pt = q.front();
        q.pop();

        int x = pt.x();
        int y = pt.y();
        
        filledPixels.push_back(pt);

        if(x < minX) minX = x;
        if(x > maxX) maxX = x;
        if(y < minY) minY = y;
        if(y > maxY) maxY = y;

        int dx[] = {1, -1, 0, 0};
        int dy[] = {0, 0, 1, -1};

        for(int i = 0; i < 4; ++i) 
        {
            int nx = x + dx[i];
            int ny = y + dy[i];
            int idx = ny * width + nx;

            if(nx >= 0 && nx < width && ny >= 0 && ny < height && !visited[idx]) 
            {
                if(colorsMatch(m_image.pixel(nx, ny), targetRgba, 12)) 
                {
                    visited[idx] = true;
                    q.push(QPoint(nx, ny));
                }
            }
        }
    }

    if(filledPixels.empty()) return;
    saveCurrentStateToHistory();

    if(m_activeBucketMode == "Solid") 
    {
        for(const QPoint &pt : filledPixels) 
        {
            m_image.setPixel(pt, fillRgba);
        }
    }
    else
    {
        int boundsWidth = maxX - minX + 1;
        int boundsHeight = maxY - minY + 1;

        if(boundsWidth < 1) boundsWidth = 1;
        if(boundsHeight < 1) boundsHeight = 1;

        // Build a temporary color lookup cache to quickly pull gradient values by row index
        QImage gradientBuffer;
        QLinearGradient gradient;

        // Construct a Linear Gradient matching the container size
        if(m_activeBucketMode == "Vertical Gradient") 
        {
            gradientBuffer = QImage(1, boundsHeight, QImage::Format_ARGB32);
            gradient = QLinearGradient(0, 0, 0, boundsHeight);
        } 
        else
        { // Horizontal Gradient
            gradientBuffer = QImage(boundsWidth, 1, QImage::Format_ARGB32);
            gradient = QLinearGradient(0, 0, boundsWidth, 0);
        }

        gradient.setColorAt(0.0, m_primaryColor);
        gradient.setColorAt(1.0, m_secondaryColor);

        QPainter gradPainter(&gradientBuffer);
        gradPainter.fillRect(gradientBuffer.rect(), QBrush(gradient));
        gradPainter.end();

        for(const QPoint &pt : filledPixels)
        {
            if(m_activeBucketMode == "Vertical Gradient") 
            {
                int bufferY = pt.y() - minY;
                if(bufferY >= 0 && bufferY < boundsHeight) 
                {
                    m_image.setPixel(pt, gradientBuffer.pixel(0, bufferY));
                }
            }
            else
            { // Horizontal Gradient
                int bufferX = pt.x() - minX;
                if(bufferX >= 0 && bufferX < boundsWidth) 
                {
                    m_image.setPixel(pt, gradientBuffer.pixel(bufferX, 0));
                }
            }
        }
    }
    
    update();
}

void PaintCanvas::runMagicWand(const QPoint &startPoint) 
{
    int width = m_image.width(); 
    int height = m_image.height();
    if(startPoint.x() < 0 || startPoint.x() >= width || startPoint.y() < 0 || startPoint.y() >= height) return;

    m_wandRegion = QRegion(); // Reset selection path
    QRgb targetColor = m_image.pixel(startPoint);

    // Direct coordinate collections
    std::vector<bool> visited(width * height, false);
    std::queue<QPoint> q;
    
    q.push(startPoint);
    visited[startPoint.y() * width + startPoint.x()] = true;

    // Use a multi-layered vector array map to collect pixel hits sorted by row index
    // This allows us to instantly merge adjacent pixels into clean spans
    std::vector<std::vector<int>> rowsOfMatchedPixels(height);

    // Perform pixel-by-pixel exploration loop
    while(!q.empty()) 
    {
        QPoint pt = q.front(); 
        q.pop(); 
        
        int x = pt.x(); 
        int y = pt.y(); 

        rowsOfMatchedPixels[y].push_back(x);

        // Verify the 4 cardinal orthogonal neighbors
        int dx[] = {1, -1, 0, 0};
        int dy[] = {0, 0, 1, -1};

        for(int i = 0; i < 4; ++i) 
        {
            int nx = x + dx[i];
            int ny = y + dy[i];
            
            if(nx >= 0 && nx < width && ny >= 0 && ny < height) 
            {
                int idx = ny * width + nx;
                if(!visited[idx]) 
                {
                    if(colorsMatch(m_image.pixel(nx, ny), targetColor, 15)) 
                    {
                        visited[idx] = true;
                        q.push(QPoint(nx, ny));
                    }
                }
            }
        }
    }

    // Batch-build the QRegion using horizontal row spans
    QRegion optimizedRegion;
    for(int y = 0; y < height; ++y) 
    {
        auto &cols = rowsOfMatchedPixels[y];
        if(cols.empty()) continue;

        // Sort column coordinates from left to right to build continuous blocks
        std::sort(cols.begin(), cols.end());

        int spanStart = cols[0];
        int previousX = cols[0];

        for(size_t i = 1; i < cols.size(); ++i) 
        {
            int currentX = cols[i];

            // If there's a gap between pixels, close the current span and start a new one
            if(currentX > previousX + 1) 
            {
                optimizedRegion += QRect(spanStart, y, previousX - spanStart + 1, 1);
                spanStart = currentX;
            }
            previousX = currentX;
        }

        // Append the final remaining span block on this line row
        optimizedRegion += QRect(spanStart, y, previousX - spanStart + 1, 1);
    }

    m_wandRegion = optimizedRegion;
    
    update();
}

QPixmap PaintCanvas::copySelection() const 
{
    if(!hasSelection()) return QPixmap();

    if(m_activeTool == "Select") return QPixmap::fromImage(m_image.copy(m_selectionRect));
    else if(m_activeTool == "Magic Wand") 
    {
        QRect bounds = m_wandRegion.boundingRect();
        QImage result(bounds.size(), QImage::Format_ARGB32);
        result.fill(Qt::transparent);

        QPainter painter(&result);
        QRegion localRegion = m_wandRegion.translated(-bounds.topLeft());
        painter.setClipRegion(localRegion);
        painter.drawImage(0, 0, m_image.copy(bounds));
        painter.end();

        return QPixmap::fromImage(result);
    }

    return QPixmap();
}

void PaintCanvas::cropToSelection() 
{
    if(!hasSelection()) return;
    saveCurrentStateToHistory();
    QRect cropRect = (m_activeTool == "Select") ? m_selectionRect : m_wandRegion.boundingRect();
    m_image = m_image.copy(cropRect).convertToFormat(QImage::Format_ARGB32);
    m_selectionRect = QRect();
    m_wandRegion = QRegion();
    resize(m_image.size());
    if(onCanvasResized) onCanvasResized();
    update();
}

void PaintCanvas::applyFilter(FilterType type) 
{
    if(m_image.isNull()) return;
    saveCurrentStateToHistory();

    int width = m_image.width();
    int height = m_image.height();

    if(type == Invert) 
    {
        m_image.invertPixels(QImage::InvertRgb);
    }
    else if(type == Greyscale) 
    {
        for(int y = 0; y < height; ++y) 
        {
            QRgb *line = reinterpret_cast<QRgb*>(m_image.scanLine(y));

            for(int x = 0; x < width; ++x) 
            {
                int r = qRed(line[x]);
                int g = qGreen(line[x]);
                int b = qBlue(line[x]);
                int gray = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
                line[x] = qRgba(gray, gray, gray, qAlpha(line[x]));
            }
        }
    }
    else if(type == Blur) 
    {
        QImage blurred = m_image;
        int kernelSize = 1;

        for(int y = kernelSize; y < height - kernelSize; ++y) 
        {
            QRgb *destLine = reinterpret_cast<QRgb*>(blurred.scanLine(y));

            for(int x = kernelSize; x < width - kernelSize; ++x) 
            {
                int totalR = 0, totalG = 0, totalB = 0, totalA = 0;
                int count = 0;

                for(int ky = -kernelSize; ky <= kernelSize; ++ky) 
                {
                    const QRgb *srcLine = reinterpret_cast<const QRgb*>(m_image.constScanLine(y + ky));

                    for(int kx = -kernelSize; kx <= kernelSize; ++kx) 
                    {
                        QRgb pixel = srcLine[x + kx];
                        totalR += qRed(pixel);
                        totalG += qGreen(pixel);
                        totalB += qBlue(pixel);
                        totalA += qAlpha(pixel);
                        count++;
                    }
                }
                destLine[x] = qRgba(totalR / count, totalG / count, totalB / count, totalA / count);
            }
        }
        m_image = blurred;
    }
    else if(type == Sepia) 
    {
        // Nostalgic Tint Mapping
        for(int y = 0; y < height; ++y) 
        {
            QRgb *line = reinterpret_cast<QRgb*>(m_image.scanLine(y));

            for(int x = 0; x < width; ++x) 
            {
                int r = qRed(line[x]);
                int g = qGreen(line[x]);
                int b = qBlue(line[x]);
                int tr = static_cast<int>(0.393 * r + 0.769 * g + 0.189 * b);
                int tg = static_cast<int>(0.349 * r + 0.686 * g + 0.168 * b);
                int tb = static_cast<int>(0.272 * r + 0.534 * g + 0.131 * b);
                line[x] = qRgba(std::min(tr, 255), std::min(tg, 255), std::min(tb, 255), qAlpha(line[x]));
            }
        }
    }
    else if(type == Brightness) 
    {
        // Direct Gamma-Scalar Increment Lookups (+25 Units Boost)
        for(int y = 0; y < height; ++y) 
        {
            QRgb *line = reinterpret_cast<QRgb*>(m_image.scanLine(y));

            for(int x = 0; x < width; ++x) 
            {
                line[x] = qRgba(std::min(qRed(line[x]) + 25, 255),
                                std::min(qGreen(line[x]) + 25, 255),
                                std::min(qBlue(line[x]) + 25, 255), qAlpha(line[x]));
            }
        }
    }

    update();
}

void PaintCanvas::paintEvent(QPaintEvent * /*event*/) 
{
    QPainter painter(this);
    if(m_isDrawing && (m_activeTool == "Line" || m_activeTool == "Rectangle" ||
                       m_activeTool == "Ellipse" || m_activeTool == "Select")) 
    {
        painter.drawImage(0, 0, m_previewImage);
    }
    else
    {
        painter.drawImage(0, 0, m_image);

        // Tracing dashed boundaries around selection layers
        QPen selectPen(Qt::blue, 1, Qt::DashLine);
        painter.setPen(selectPen);
        painter.setBrush(Qt::NoBrush);

        if(m_activeTool == "Select" && !m_selectionRect.isEmpty()) 
        {
            painter.drawRect(m_selectionRect);
        }
        else if(m_activeTool == "Magic Wand" && !m_wandRegion.isEmpty()) 
        {
            QPainterPath totalOutlinePath;
            
            // This function processes all individual 1x1 pixels, merges adjacent edges, 
            // and generates a vector boundary matching only the outer edge lines
            totalOutlinePath.addRegion(m_wandRegion);
            
            // Draw only the calculated continuous outer outline
            painter.drawPath(totalOutlinePath);
        }
    }
}

void PaintCanvas::mousePressEvent(QMouseEvent *event) 
{
    if(event->button() == Qt::LeftButton) 
    {
        QPoint clickedPoint = event->position().toPoint();

        if(hasSelection()) 
        {
            bool clickInside = false;
            if(m_activeTool == "Select" && m_selectionRect.contains(clickedPoint)) clickInside = true;
            if(m_activeTool == "Magic Wand" && m_wandRegion.contains(clickedPoint)) clickInside = true;

            if(clickInside) 
            {
                // Begin dragging the floating selection block
                saveCurrentStateToHistory();
                m_isDraggingSelection = true;
                m_isDirty = true;
                m_dragStartMousePos = clickedPoint;
                m_imageBeforeDrag = m_image;

                if(m_activeTool == "Select") 
                {
                    m_dragSourceRect = m_selectionRect;
                    m_dragSelectionContent = m_image.copy(m_selectionRect);

                    // Punch a blank white hole where the selection was pulled from
                    QPainter p(&m_image);
                    p.fillRect(m_selectionRect, Qt::white);
                }
                else
                {
                    m_dragSourceRect = m_wandRegion.boundingRect();
                    m_dragSourceWandRegion = m_wandRegion;
                    m_dragSelectionContent = copySelection().toImage();

                    QPainter p(&m_image);
                    p.setClipRegion(m_wandRegion);
                    p.fillRect(m_dragSourceRect, Qt::white);
                }

                update();
                return;
            } 
            else
            {
                clearActiveSelection();
                return;
            }
        }

        if(m_activeTool == "Eyedropper") 
        {
            if(clickedPoint.x() >= 0 && clickedPoint.x() < m_image.width() &&
               clickedPoint.y() >= 0 && clickedPoint.y() < m_image.height()) 
            {
                QColor sampledColor(m_image.pixel(clickedPoint));
                setPrimaryColor(sampledColor);
            }
            return;
        }

        if(m_activeTool == "Magic Wand") 
        {
            runMagicWand(clickedPoint);
            return;
        }

        if(m_activeTool == "Text") 
        {
            bool ok;
            QString text = QInputDialog::getText(this, "Text Tool", "Enter text:", QLineEdit::Normal, "", &ok);
            if(ok && !text.isEmpty()) 
            {
                saveCurrentStateToHistory();
                m_isDirty = true;

                QPainter painter(&m_image);
                painter.setPen(m_primaryColor);

                int fontSize = m_lineThickness * 6;
                if(fontSize < 12) fontSize = 14;
                painter.setFont(QFont("Arial", fontSize));
                painter.drawText(clickedPoint, text);
                update();
            }
            return;
        }

        m_isDrawing = true;
        m_startPoint = clickedPoint;
        m_lastPoint = m_startPoint;

        if(m_activeTool != "Select") 
        {
            saveCurrentStateToHistory();
            m_isDirty = true;
        }

        if(m_activeTool == "Bucket Fill") 
        {
            floodFill(m_startPoint, m_primaryColor);
            m_isDrawing = false;
        }
        else if(m_activeTool == "Line" || m_activeTool == "Rectangle" ||
                m_activeTool == "Ellipse" || m_activeTool == "Select") 
        {
            m_previewImage = m_image;
        }
        else if(m_activeTool == "Free Draw") 
        {
            drawPatternStroke(m_startPoint, m_startPoint);
        }
    }
}

void PaintCanvas::mouseMoveEvent(QMouseEvent *event) 
{
    QPoint currentPoint = event->position().toPoint();

    if(onMouseMoved) onMouseMoved(currentPoint);

    if(m_isDraggingSelection) 
    {
        QPoint delta = currentPoint - m_dragStartMousePos;

        // Restore canvas to before the drag step, maintaining the initial cut hole
        m_image = m_imageBeforeDrag;
        QPainter p(&m_image);

        if(m_activeTool == "Select") 
        {
            p.fillRect(m_dragSourceRect, Qt::white); // Keep hole open
            QRect newTarget = m_dragSourceRect.translated(delta);
            p.drawImage(newTarget.topLeft(), m_dragSelectionContent);
            m_selectionRect = newTarget;
        }
        else
        {
            p.setClipRegion(m_dragSourceWandRegion);
            p.fillRect(m_dragSourceRect, Qt::white);
            p.setClipping(false);
            QPoint newTopLeft = m_dragSourceRect.topLeft() + delta;
            p.drawImage(newTopLeft, m_dragSelectionContent);
            m_wandRegion = m_dragSourceWandRegion.translated(delta);
        }
        update();
        return;
    }

    if(m_activeTool == "Eyedropper" && (event->buttons() & Qt::LeftButton)) 
    {
        if(currentPoint.x() >= 0 && currentPoint.x() < m_image.width() &&
           currentPoint.y() >= 0 && currentPoint.y() < m_image.height()) 
        {
            setPrimaryColor(QColor(m_image.pixel(currentPoint)));
        }
        return;
    }

    if(!(event->buttons() & Qt::LeftButton) || !m_isDrawing) return;

    if(m_activeTool == "Eraser") 
    {
        QPainter painter(&m_image);
        painter.setPen(getConfiguredPen());
        painter.drawLine(m_lastPoint, currentPoint);
        m_lastPoint = currentPoint;
        update();
    }
    else if(m_activeTool == "Free Draw") 
    {
        drawPatternStroke(m_lastPoint, currentPoint);
        m_lastPoint = currentPoint;
    }
    else if(m_activeTool == "Line" || m_activeTool == "Rectangle" ||
            m_activeTool == "Ellipse" || m_activeTool == "Select") 
    {
        if(event->modifiers() & Qt::ShiftModifier) 
        {
            int dx = std::abs(currentPoint.x() - m_startPoint.x());
            int dy = std::abs(currentPoint.y() - m_startPoint.y());

            if(m_activeTool == "Line") 
            {
                if(dx > dy) currentPoint.setY(m_startPoint.y());
                else currentPoint.setX(m_startPoint.x());
            }
            else
            {
                int sideLength = std::max(dx, dy);
                currentPoint.setX(m_startPoint.x() + (sideLength * (currentPoint.x() >= m_startPoint.x() ? 1 : -1)));
                currentPoint.setY(m_startPoint.y() + (sideLength * (currentPoint.y() >= m_startPoint.y() ? 1 : -1)));
            }
        }

        m_previewImage = m_image;
        QPainter painter(&m_previewImage);

        if(m_activeTool == "Select") 
        {
            painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
            painter.setBrush(Qt::NoBrush);
            m_selectionRect = QRect(m_startPoint, currentPoint).normalized(); 
            painter.drawRect(m_selectionRect);
        }
        else
        {
            painter.setPen(getConfiguredPen());
            painter.setBrush(QBrush(m_secondaryColor));
            drawShape(painter, m_startPoint, currentPoint);
        }
        update();
    }
}

void PaintCanvas::drawPatternStroke(const QPoint &from, const QPoint &to) 
{
    QPainter painter(&m_image);
    if(m_activeBrushPattern == "Solid") 
    {
        painter.setPen(getConfiguredPen());
        painter.drawLine(from, to);
    }
    else if(m_activeBrushPattern == "Airbrush") 
    {
        painter.setPen(m_primaryColor);

        static std::mt19937 gen(std::random_device{}());
        int radius = m_lineThickness * 3;

        if(radius < 5) radius = 5;
        
        std::uniform_real_distribution<double> dist_r(0.0, static_cast<double>(radius));
        std::uniform_real_distribution<double> dist_theta(0.0, 2.0 * M_PI);

        int distance = std::hypot(to.x() - from.x(), to.y() - from.y());
        int steps = std::max(1, distance / 2);

        for(int s = 0; s <= steps; ++s) 
        {
            double t = (steps == 0) ? 0.0 : static_cast<double>(s) / steps;
            int cx = from.x() + t * (to.x() - from.x());
            int cy = from.y() + t * (to.y() - from.y());

            for(int i = 0; i < m_airbrushDensity; ++i) 
            {
                painter.drawPoint(cx + dist_r(gen) * std::cos(dist_theta(gen)), cy + dist_r(gen) * std::sin(dist_theta(gen)));
            }
        }
    }
    else if(m_activeBrushPattern == "Calligraphy") 
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_primaryColor);

        int distance = std::hypot(to.x() - from.x(), to.y() - from.y());
        int steps = std::max(1, distance);

        for(int s = 0; s <= steps; ++s) 
        {
            double t = static_cast<double>(s) / steps;

            painter.save();
            painter.translate(from.x() + t * (to.x() - from.x()), from.y() + t * (to.y() - from.y()));
            painter.rotate(45);
            painter.drawRect(QRect(-1, -m_lineThickness, 2, m_lineThickness * 2));
            painter.restore();
        }
    }
    update();
}

void PaintCanvas::mouseReleaseEvent(QMouseEvent *event) 
{
    if(event->button() == Qt::LeftButton) 
    {
        if(m_isDraggingSelection) 
        {
            m_isDraggingSelection = false;
            m_imageBeforeDrag = QImage();
            update();
            return;
        }

        if(m_isDrawing) 
        {
            QPoint currentPoint = event->position().toPoint();
            m_isDrawing = false;

            if(event->modifiers() & Qt::ShiftModifier) 
            {
                int dx = std::abs(currentPoint.x() - m_startPoint.x());
                int dy = std::abs(currentPoint.y() - m_startPoint.y());

                if(m_activeTool == "Line") 
                {
                    if(dx > dy) currentPoint.setY(m_startPoint.y());
                    else currentPoint.setX(m_startPoint.x());
                } 
                else
                {
                    int sideLength = std::max(dx, dy);

                    currentPoint.setX(m_startPoint.x() + (sideLength * (currentPoint.x() >= m_startPoint.x() ? 1 : -1)));
                    currentPoint.setY(m_startPoint.y() + (sideLength * (currentPoint.y() >= m_startPoint.y() ? 1 : -1)));
                }
            }

            if(m_activeTool == "Line" || m_activeTool == "Rectangle" || m_activeTool == "Ellipse") 
            {
                QPainter painter(&m_image); 
                painter.setPen(getConfiguredPen());
                painter.setBrush(QBrush(m_secondaryColor));
                drawShape(painter, m_startPoint, currentPoint);
                update();
            }
            else if(m_activeTool == "Select") 
            {
                m_selectionRect = QRect(m_startPoint, currentPoint).normalized();
            }
        }
    }
}

void PaintCanvas::resizeEvent(QResizeEvent *event) 
{
    if(width() > m_image.width() || height() > m_image.height()) 
    {
        int newWidth = qMax(width(), m_image.width());
        int newHeight = qMax(height(), m_image.height());

        saveCurrentStateToHistory();
        resizeImage(&m_image, QSize(newWidth, newHeight));
        update();
    }

    QWidget::resizeEvent(event);
}

void PaintCanvas::resizeImage(QImage *image, const QSize &newSize) 
{
    if(image->size() == newSize) return;

    QImage newImage(newSize, QImage::Format_ARGB32);
    newImage.fill(Qt::white);
    QPainter painter(&newImage);
    painter.drawImage(QPoint(0, 0), *image);
    *image = newImage;
}

void PaintCanvas::drawShape(QPainter &painter, const QPoint &start, const QPoint &end) 
{
    if(m_activeTool == "Line") painter.drawLine(start, end);
    else if(m_activeTool == "Rectangle") painter.drawRect(QRect(start, end).normalized());
    else if(m_activeTool == "Ellipse") painter.drawEllipse(QRect(start, end).normalized());
}

bool PaintCanvas::openImage(const QString &filePath) 
{
    QImage loadedImage;

    if(!loadedImage.load(filePath)) return false;

    saveCurrentStateToHistory();
    m_image = loadedImage.convertToFormat(QImage::Format_ARGB32);
    m_isDirty = false;
    resize(m_image.size());
    update();
    return true;
}

bool PaintCanvas::saveImage(const QString &filePath) 
{
    bool ok = m_image.save(filePath);
    if(ok) m_isDirty = false;
    return ok;
}

void PaintCanvas::pasteImage(const QPixmap &pixmap) 
{
    if(pixmap.isNull()) return;

    // Save current pixels to history before stamping the new image
    saveCurrentStateToHistory();
    
    QPainter painter(&m_image);
    painter.drawPixmap(0, 0, pixmap);
    update();
}

