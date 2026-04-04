#pragma once
#include <QGraphicsView>
#include <QGraphicsScene>
#include <unordered_map>
#include <string>
#include <vector>
#include "../core/graph/CampusGraph.h"
#include "NodeItem.h"
#include "EdgeItem.h"

class CampusMapView : public QGraphicsView {
    Q_OBJECT
public:
    explicit CampusMapView(QWidget* parent = nullptr);

    void loadGraph(const CampusGraph& graph);
    void highlightTraversal(const std::vector<std::string>& order);
    void highlightPath(const std::vector<std::string>& path);
    void clearHighlights();
    void setMobilityMode(bool mr);
    void updateEdgeBlockState(const std::string& from, const std::string& to, bool blocked);

signals:
    void nodeClicked(const QString& nodeId);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QGraphicsScene* scene_{nullptr};
    std::unordered_map<std::string, NodeItem*> node_items_;
    std::vector<EdgeItem*> edge_items_;
    bool panning_{false};
    QPoint pan_start_;
    bool mobility_mode_{false};
};
