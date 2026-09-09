#include "HistoryGraph.h"
#include <QPainterPath>

HistoryGraph::HistoryGraph(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, false);

    // Initialize color palette
    lineColors = { Qt::white, Qt::green, Qt::cyan, Qt::magenta, Qt::yellow, 
                  QColor(255,165,0), QColor(138,43,226), QColor(0,255,127), 
                  QColor(255,20,147), QColor(30,144,255) };
}

void HistoryGraph::addMultiSamples(const std::vector<double>& values) 
{
    if(multiPointsHistory.size() < values.size()) 
    {
        multiPointsHistory.resize(values.size());
    }

    for(size_t i = 0; i < values.size(); ++i) 
    {
        multiPointsHistory[i].push_back(values[i]);
        if(multiPointsHistory[i].size() > 100) multiPointsHistory[i].erase(multiPointsHistory[i].begin());
    }

    update();
}

void HistoryGraph::addSingleSample(double value) 
{
    addMultiSamples(std::vector<double>{value});
}

void HistoryGraph::setLabels(const std::vector<std::string>& labels)
{
    coreLabels = labels;
}

void HistoryGraph::setLineColor(const QColor& color)
{
    chartColor = color; 
    if(!lineColors.empty()) lineColors[0] = color; // Sync first index for single line
}

void HistoryGraph::setLabelText(const std::string& label)
{
    axisLabel = label; 
    setLabels({label});
}

void HistoryGraph::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(25, 25, 25));

    // Draw reference grid mesh lines
    painter.setPen(QColor(60, 60, 60, 100));
    for(int i = 1; i < 4; ++i) 
    {
        int y = (height() / 4) * i;
        painter.drawLine(0, y, width(), y);
    }

    if(multiPointsHistory.empty() || multiPointsHistory[0].size() < 2) 
    {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "Awaiting Data...");
        return;
    }

    double xStep = static_cast<double>(width()) / 99.0;
    int legendX = 10;
    int legendY = 20;

    // Draw lines and legends
    for(size_t core = 0; core < multiPointsHistory.size(); ++core) 
    {
        QColor color = lineColors[core % lineColors.size()];
        QPainterPath linePath;

        for(size_t i = 0; i < multiPointsHistory[core].size(); ++i) 
        {
            double x = i * xStep;
            double y = height() - ((multiPointsHistory[core][i] / 100.0) * height());
            if(i == 0) linePath.moveTo(x, y);
            else linePath.lineTo(x, y);
        }

        painter.setPen(QPen(color, (core == 0) ? 3 : 1.5)); // Make aggregate CPU line bolder
        painter.drawPath(linePath);

        // Draw structural Legend item block
        std::string labelText = (core < coreLabels.size()) ? coreLabels[core] : "CPU " + std::to_string(core);
        if(!multiPointsHistory[core].empty()) 
        {
            labelText += ": " + std::to_string(static_cast<int>(multiPointsHistory[core].back())) + "%";
        }

        painter.setPen(color);
        painter.drawText(legendX, legendY, QString::fromStdString(labelText));
        legendX += 110;
        if(legendX > width() - 100) 
        { // Wrap row if columns overflow window width
            legendX = 10;
            legendY += 20;
        }
    }
}

