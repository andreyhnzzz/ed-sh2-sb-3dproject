#include "PathfindTab.h"
#include "CampusMapView.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>

PathfindTab::PathfindTab(NavigationService& nav, ScenarioManager& scenario, CampusMapView* mapView,
                         const CampusGraph& graph, QWidget* parent)
    : QWidget(parent), nav_(nav), scenario_(scenario), mapView_(mapView), graph_(graph)
{
    auto* layout = new QVBoxLayout(this);
    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel("Origen:"), 0, 0);
    combo_from_ = new QComboBox(this);
    grid->addWidget(combo_from_, 0, 1);
    grid->addWidget(new QLabel("Destino:"), 1, 0);
    combo_to_ = new QComboBox(this);
    grid->addWidget(combo_to_, 1, 1);
    layout->addLayout(grid);

    btn_find_ = new QPushButton("Buscar Camino (DFS)", this);
    layout->addWidget(btn_find_);

    lbl_total_ = new QLabel(this);
    layout->addWidget(lbl_total_);

    layout->addWidget(new QLabel("Ruta:"));
    list_path_ = new QListWidget(this);
    layout->addWidget(list_path_);

    refreshNodeList();
    connect(btn_find_, &QPushButton::clicked, this, &PathfindTab::onFindPath);
}

void PathfindTab::refreshNodeList() {
    combo_from_->clear();
    combo_to_->clear();
    for (auto& id : graph_.nodeIds()) {
        try {
            const Node& n = graph_.getNode(id);
            QString item = QString::fromStdString(id + " — " + n.name);
            QString data = QString::fromStdString(id);
            combo_from_->addItem(item, data);
            combo_to_->addItem(item, data);
        } catch (...) {}
    }
}

void PathfindTab::onFindPath() {
    QString from = combo_from_->currentData().toString();
    QString to = combo_to_->currentData().toString();
    if (from.isEmpty() || to.isEmpty()) return;

    auto result = nav_.findPath(from.toStdString(), to.toStdString(), scenario_.isMobilityReduced());

    list_path_->clear();
    if (!result.found) {
        lbl_total_->setText("No se encontro camino entre los nodos seleccionados.");
        lbl_total_->setStyleSheet("color: red;");
        return;
    }

    for (size_t i = 0; i < result.path.size(); ++i) {
        const auto& id = result.path[i];
        try {
            const Node& n = graph_.getNode(id);
            list_path_->addItem(QString("%1. %2 (%3)")
                                    .arg(i + 1)
                                    .arg(QString::fromStdString(n.name))
                                    .arg(QString::fromStdString(id)));
        } catch (...) {}
    }

    lbl_total_->setText(QString("Camino encontrado | Distancia total: %1 | Pasos: %2")
                            .arg(result.total_weight, 0, 'f', 2)
                            .arg(result.path.size()));
    lbl_total_->setStyleSheet("color: green;");

    if (mapView_) mapView_->highlightPath(result.path);
}
