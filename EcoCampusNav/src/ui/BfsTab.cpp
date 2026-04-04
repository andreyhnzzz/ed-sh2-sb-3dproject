#include "BfsTab.h"
#include "CampusMapView.h"
#include <QVBoxLayout>
#include <QLabel>

BfsTab::BfsTab(NavigationService& nav, ScenarioManager& scenario, CampusMapView* mapView,
               const CampusGraph& graph, QWidget* parent)
    : QWidget(parent), nav_(nav), scenario_(scenario), mapView_(mapView), graph_(graph)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Nodo de inicio:"));
    combo_start_ = new QComboBox(this);
    layout->addWidget(combo_start_);

    btn_run_ = new QPushButton("Ejecutar BFS", this);
    layout->addWidget(btn_run_);

    lbl_stats_ = new QLabel(this);
    layout->addWidget(lbl_stats_);

    layout->addWidget(new QLabel("Orden de visita:"));
    list_result_ = new QListWidget(this);
    layout->addWidget(list_result_);

    refreshNodeList();
    connect(btn_run_, &QPushButton::clicked, this, &BfsTab::onRunBfs);
}

void BfsTab::refreshNodeList() {
    combo_start_->clear();
    for (auto& id : graph_.nodeIds()) {
        try {
            const Node& n = graph_.getNode(id);
            combo_start_->addItem(QString::fromStdString(id + " — " + n.name),
                                   QString::fromStdString(id));
        } catch (...) {}
    }
}

void BfsTab::onRunBfs() {
    QString startId = combo_start_->currentData().toString();
    if (startId.isEmpty()) return;

    auto result = nav_.runBfs(startId.toStdString(), scenario_.isMobilityReduced());

    list_result_->clear();
    for (size_t i = 0; i < result.visit_order.size(); ++i) {
        const std::string& id = result.visit_order[i];
        double dist = result.accumulated_dist.count(id) ? result.accumulated_dist.at(id) : 0.0;
        try {
            const Node& n = graph_.getNode(id);
            list_result_->addItem(QString("%1. %2 (%3) — dist: %4")
                                      .arg(i + 1)
                                      .arg(QString::fromStdString(n.name))
                                      .arg(QString::fromStdString(id))
                                      .arg(dist, 0, 'f', 2));
        } catch (...) {}
    }

    lbl_stats_->setText(QString("Nodos visitados: %1 | Tiempo: %2 μs")
                            .arg(result.nodes_visited)
                            .arg(result.elapsed_us));

    if (mapView_) mapView_->highlightTraversal(result.visit_order);
}
