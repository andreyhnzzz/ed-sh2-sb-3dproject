#include "NodeItem.h"
#include <QGraphicsSceneHoverEvent>
#include <QToolTip>
#include <QCursor>
#include <QPen>
#include <QBrush>
#include <QString>

QColor NodeItem::colorForType(const std::string& type) {
    if (type == "exterior")    return QColor("#95A5A6");
    if (type == "comedor")     return QColor("#E67E22");
    if (type == "biblioteca")  return QColor("#3498DB");
    if (type == "aulas_n1")    return QColor("#27AE60");
    if (type == "aulas_n2")    return QColor("#1ABC9C");
    if (type == "escalera")    return QColor("#E74C3C");
    if (type == "elevador")    return QColor("#9B59B6");
    if (type == "transporte")  return QColor("#F1C40F");
    return QColor("#BDC3C7");
}

NodeItem::NodeItem(const Node& node, QGraphicsItem* parent)
    : QGraphicsEllipseItem(-RADIUS, -RADIUS, 2*RADIUS, 2*RADIUS, parent)
    , node_(node)
{
    setPos(node.x, node.y);
    setAcceptHoverEvents(true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);

    label_ = new QGraphicsTextItem(QString::fromStdString(node.id), this);
    label_->setPos(RADIUS + 2, -8);
    label_->setDefaultTextColor(Qt::black);
    QFont f;
    f.setPointSize(7);
    label_->setFont(f);

    updateAppearance();
}

void NodeItem::setHighlighted(bool h) {
    highlighted_ = h;
    updateAppearance();
}

void NodeItem::setVisited(bool v) {
    visited_ = v;
    updateAppearance();
}

void NodeItem::updateAppearance() {
    QColor base = colorForType(node_.type);
    if (highlighted_) {
        setPen(QPen(Qt::yellow, 3));
        setBrush(QBrush(base.lighter(130)));
    } else if (visited_) {
        setPen(QPen(Qt::darkGray, 2));
        setBrush(QBrush(base.lighter(110)));
    } else {
        setPen(QPen(Qt::black, 1.5));
        setBrush(QBrush(base));
    }
}

void NodeItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event) {
    QString tip = QString("<b>%1</b><br/>Tipo: %2<br/>z=%3")
                      .arg(QString::fromStdString(node_.name))
                      .arg(QString::fromStdString(node_.type))
                      .arg(node_.z);
    QToolTip::showText(QCursor::pos(), tip);
    QGraphicsEllipseItem::hoverEnterEvent(event);
}

void NodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event) {
    QToolTip::hideText();
    QGraphicsEllipseItem::hoverLeaveEvent(event);
}
