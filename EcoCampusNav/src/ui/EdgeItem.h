#pragma once
#include <QGraphicsLineItem>
#include <QGraphicsTextItem>
#include "../core/graph/Edge.h"

class EdgeItem : public QGraphicsLineItem {
public:
    EdgeItem(double x1, double y1, double x2, double y2,
             const Edge& edge, QGraphicsItem* parent = nullptr);

    void setBlocked(bool b);
    void setHighlighted(bool h);
    const Edge& edge() const { return edge_; }

private:
    Edge edge_;
    bool blocked_{false};
    bool highlighted_{false};
    QGraphicsTextItem* weight_label_{nullptr};
    void updateAppearance();
};
