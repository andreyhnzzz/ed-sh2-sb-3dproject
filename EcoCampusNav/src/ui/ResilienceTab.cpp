#include "ResilienceTab.h"
#include "CampusMapView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QGridLayout>

ResilienceTab::ResilienceTab(ResilienceService& resilience, ScenarioManager& scenario,
                              CampusMapView* mapView, const CampusGraph& graph, QWidget* parent)
    : QWidget(parent), resilience_(resilience), scenario_(scenario), mapView_(mapView), graph_(graph)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* grpBlock = new QGroupBox("Bloquear Arista");
    auto* blockLayout = new QVBoxLayout(grpBlock);
    combo_edges_ = new QComboBox(this);
    refreshEdgeList();
    blockLayout->addWidget(new QLabel("Seleccionar arista:"));
    blockLayout->addWidget(combo_edges_);

    auto* btnRow = new QHBoxLayout();
    btn_block_ = new QPushButton("Bloquear", this);
    btn_unblock_all_ = new QPushButton("Desbloquear Todo", this);
    btnRow->addWidget(btn_block_);
    btnRow->addWidget(btn_unblock_all_);
    blockLayout->addLayout(btnRow);

    blockLayout->addWidget(new QLabel("Aristas bloqueadas:"));
    list_blocked_ = new QListWidget(this);
    blockLayout->addWidget(list_blocked_);
    mainLayout->addWidget(grpBlock);

    auto* grpAlt = new QGroupBox("Buscar Ruta Alternativa");
    auto* altLayout = new QGridLayout(grpAlt);
    altLayout->addWidget(new QLabel("Desde:"), 0, 0);
    combo_alt_from_ = new QComboBox(this);
    altLayout->addWidget(combo_alt_from_, 0, 1);
    altLayout->addWidget(new QLabel("Hasta:"), 1, 0);
    combo_alt_to_ = new QComboBox(this);
    altLayout->addWidget(combo_alt_to_, 1, 1);
    btn_find_alt_ = new QPushButton("Buscar Ruta Alternativa", this);
    altLayout->addWidget(btn_find_alt_, 2, 0, 1, 2);
    lbl_result_ = new QLabel(this);
    lbl_result_->setWordWrap(true);
    altLayout->addWidget(lbl_result_, 3, 0, 1, 2);
    list_alt_path_ = new QListWidget(this);
    altLayout->addWidget(list_alt_path_, 4, 0, 1, 2);
    mainLayout->addWidget(grpAlt);

    for (auto& id : graph_.nodeIds()) {
        try {
            const Node& n = graph_.getNode(id);
            QString item = QString::fromStdString(id + " — " + n.name);
            QString data = QString::fromStdString(id);
            combo_alt_from_->addItem(item, data);
            combo_alt_to_->addItem(item, data);
        } catch (...) {}
    }

    connect(btn_block_, &QPushButton::clicked, this, &ResilienceTab::onBlockEdge);
    connect(btn_unblock_all_, &QPushButton::clicked, this, &ResilienceTab::onUnblockAll);
    connect(btn_find_alt_, &QPushButton::clicked, this, &ResilienceTab::onFindAlternate);
}

void ResilienceTab::refreshEdgeList() {
    combo_edges_->clear();
    for (auto& id : graph_.nodeIds()) {
        for (auto& e : graph_.edgesFrom(id)) {
            if (e.from < e.to) {
                std::string wStr = std::to_string(e.base_weight);
                if (wStr.size() > 4) wStr = wStr.substr(0, 4);
                combo_edges_->addItem(
                    QString::fromStdString(e.from + " <-> " + e.to + " (w=" + wStr + ")"),
                    QString::fromStdString(e.from + "," + e.to));
            }
        }
    }
}

void ResilienceTab::onBlockEdge() {
    QString data = combo_edges_->currentData().toString();
    if (data.isEmpty()) return;
    QStringList parts = data.split(",");
    if (parts.size() != 2) return;

    std::string from = parts[0].toStdString();
    std::string to = parts[1].toStdString();
    resilience_.blockEdge(from, to);

    list_blocked_->clear();
    for (auto& [f, t] : resilience_.getBlockedEdges()) {
        list_blocked_->addItem(QString::fromStdString(f + " <-> " + t));
    }

    if (mapView_) mapView_->updateEdgeBlockState(from, to, true);
}

void ResilienceTab::onUnblockAll() {
    resilience_.unblockAll();
    list_blocked_->clear();
    if (mapView_) {
        for (auto& id : graph_.nodeIds()) {
            for (auto& e : graph_.edgesFrom(id)) {
                if (e.from < e.to)
                    mapView_->updateEdgeBlockState(e.from, e.to, false);
            }
        }
    }
}

void ResilienceTab::onFindAlternate() {
    QString from = combo_alt_from_->currentData().toString();
    QString to = combo_alt_to_->currentData().toString();
    if (from.isEmpty() || to.isEmpty()) return;

    auto result = resilience_.findAlternatePath(from.toStdString(), to.toStdString(),
                                                 scenario_.isMobilityReduced());

    list_alt_path_->clear();
    if (!result.found) {
        lbl_result_->setText("Zona inaccesible: No existe ruta alternativa con los bloqueos actuales.");
        lbl_result_->setStyleSheet("color: red; font-weight: bold;");
        return;
    }

    lbl_result_->setText(QString("Ruta alternativa encontrada | Distancia: %1")
                             .arg(result.total_weight, 0, 'f', 2));
    lbl_result_->setStyleSheet("color: green;");

    for (size_t i = 0; i < result.path.size(); ++i) {
        const auto& id = result.path[i];
        try {
            const Node& n = graph_.getNode(id);
            list_alt_path_->addItem(QString("%1. %2 (%3)")
                                        .arg(i + 1)
                                        .arg(QString::fromStdString(n.name))
                                        .arg(QString::fromStdString(id)));
        } catch (...) {}
    }

    if (mapView_) mapView_->highlightPath(result.path);
}
