#ifndef HISTORY_GRAPH_H
#define HISTORY_GRAPH_H

#include <QWidget>
#include <QPainter>
#include <vector>

class HistoryGraph : public QWidget
{
public:
    explicit HistoryGraph(QWidget* parent = nullptr);
    
    void addMultiSamples(const std::vector<double>& values);
    void addSingleSample(double value);
    void setLabels(const std::vector<std::string>& labels);

    void setLineColor(const QColor& color);
    void setLabelText(const std::string& label);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<std::vector<double>> multiPointsHistory;
    std::vector<std::string> coreLabels;
    std::vector<QColor> lineColors;

    QColor chartColor = Qt::cyan;
    std::string axisLabel = "Metric";
};

#endif      /* HISTORY_GRAPH_H */
