#pragma once
#include <QGraphicsEllipseItem>
#include <QGraphicsTextItem>
#include <QString>
#include "../core/graph/Node.h"

class NodeItem : public QGraphicsEllipseItem {
public:
    explicit NodeItem(const Node& node, QGraphicsItem* parent = nullptr);

    const std::string& nodeId() const { return node_.id; }
    void setHighlighted(bool h);
    void setVisited(bool v);
    static QColor colorForType(const std::string& type);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;

private:
    Node node_;
    bool highlighted_{false};
    bool visited_{false};
    QGraphicsTextItem* label_{nullptr};

    static constexpr double RADIUS = 12.0;
    void updateAppearance();
};
