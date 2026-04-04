#include "EdgeItem.h"
#include <QPen>
#include <QBrush>
#include <QString>
#include <QFont>

EdgeItem::EdgeItem(double x1, double y1, double x2, double y2,
                   const Edge& edge, QGraphicsItem* parent)
    : QGraphicsLineItem(x1, y1, x2, y2, parent)
    , edge_(edge)
{
    setZValue(-1);

    double mx = (x1 + x2) / 2.0;
    double my = (y1 + y2) / 2.0;
    weight_label_ = new QGraphicsTextItem(this);
    weight_label_->setPos(mx, my);
    QFont f;
    f.setPointSize(6);
    weight_label_->setFont(f);
    weight_label_->setDefaultTextColor(QColor("#555555"));
    weight_label_->setPlainText(QString::number(edge_.base_weight, 'f', 1));

    updateAppearance();
}

void EdgeItem::setBlocked(bool b) {
    blocked_ = b;
    updateAppearance();
}

void EdgeItem::setHighlighted(bool h) {
    highlighted_ = h;
    updateAppearance();
}

void EdgeItem::updateAppearance() {
    if (highlighted_) {
        setPen(QPen(QColor("#F39C12"), 3));
    } else if (blocked_) {
        QPen p(QColor("#E74C3C"), 2, Qt::DashLine);
        setPen(p);
    } else if (edge_.blocked_for_mr) {
        QPen p(QColor("#E74C3C"), 1.5, Qt::DotLine);
        setPen(p);
    } else {
        setPen(QPen(QColor("#7F8C8D"), 1.5));
    }
}
