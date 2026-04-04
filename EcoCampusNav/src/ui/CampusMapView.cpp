#include "CampusMapView.h"
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QGraphicsEllipseItem>

CampusMapView::CampusMapView(QWidget* parent)
    : QGraphicsView(parent)
{
    scene_ = new QGraphicsScene(this);
    setScene(scene_);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag);
    setTransformationAnchor(AnchorUnderMouse);
    setBackgroundBrush(QBrush(QColor("#F0F4F8")));
}

void CampusMapView::loadGraph(const CampusGraph& graph) {
    scene_->clear();
    node_items_.clear();
    edge_items_.clear();

    for (auto& id : graph.nodeIds()) {
        const Node& n = graph.getNode(id);
        for (auto& e : graph.edgesFrom(id)) {
            if (e.from < e.to) {
                const Node& target = graph.getNode(e.to);
                auto* item = new EdgeItem(n.x, n.y, target.x, target.y, e);
                scene_->addItem(item);
                edge_items_.push_back(item);
            }
        }
    }

    for (auto& id : graph.nodeIds()) {
        const Node& n = graph.getNode(id);
        auto* item = new NodeItem(n);
        scene_->addItem(item);
        node_items_[id] = item;
    }

    fitInView(scene_->itemsBoundingRect(), Qt::KeepAspectRatio);
}

void CampusMapView::highlightTraversal(const std::vector<std::string>& order) {
    clearHighlights();
    for (auto& id : order) {
        if (node_items_.count(id))
            node_items_[id]->setVisited(true);
    }
}

void CampusMapView::highlightPath(const std::vector<std::string>& path) {
    clearHighlights();
    for (auto& id : path) {
        if (node_items_.count(id))
            node_items_[id]->setHighlighted(true);
    }
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        for (auto* ei : edge_items_) {
            auto& e = ei->edge();
            bool match = (e.from == path[i] && e.to == path[i+1]) ||
                         (e.from == path[i+1] && e.to == path[i]);
            if (match) ei->setHighlighted(true);
        }
    }
}

void CampusMapView::clearHighlights() {
    for (auto& [id, item] : node_items_) {
        item->setHighlighted(false);
        item->setVisited(false);
    }
    for (auto* ei : edge_items_) {
        ei->setHighlighted(false);
    }
}

void CampusMapView::setMobilityMode(bool mr) {
    mobility_mode_ = mr;
    for (auto* ei : edge_items_) {
        if (mr && ei->edge().blocked_for_mr)
            ei->setBlocked(true);
        else
            ei->setBlocked(ei->edge().currently_blocked);
    }
}

void CampusMapView::updateEdgeBlockState(const std::string& from, const std::string& to, bool blocked) {
    for (auto* ei : edge_items_) {
        auto& e = ei->edge();
        if ((e.from == from && e.to == to) || (e.from == to && e.to == from))
            ei->setBlocked(blocked);
    }
}

void CampusMapView::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    scale(factor, factor);
}

void CampusMapView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        pan_start_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        auto items = scene_->items(mapToScene(event->pos()));
        for (auto* item : items) {
            if (auto* ni = dynamic_cast<NodeItem*>(item)) {
                emit nodeClicked(QString::fromStdString(ni->nodeId()));
                break;
            }
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void CampusMapView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        panning_ = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void CampusMapView::mouseMoveEvent(QMouseEvent* event) {
    if (panning_) {
        QPoint delta = event->pos() - pan_start_;
        pan_start_ = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}
