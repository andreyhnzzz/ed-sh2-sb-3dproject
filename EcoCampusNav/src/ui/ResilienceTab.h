#pragma once
#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include "../services/ResilienceService.h"
#include "../services/ScenarioManager.h"
#include "../core/graph/CampusGraph.h"

class CampusMapView;

class ResilienceTab : public QWidget {
    Q_OBJECT
public:
    ResilienceTab(ResilienceService& resilience, ScenarioManager& scenario,
                  CampusMapView* mapView, const CampusGraph& graph, QWidget* parent = nullptr);
    void refreshEdgeList();

private slots:
    void onBlockEdge();
    void onUnblockAll();
    void onFindAlternate();

private:
    ResilienceService& resilience_;
    ScenarioManager& scenario_;
    CampusMapView* mapView_;
    const CampusGraph& graph_;

    QComboBox* combo_edges_{nullptr};
    QPushButton* btn_block_{nullptr};
    QPushButton* btn_unblock_all_{nullptr};
    QListWidget* list_blocked_{nullptr};

    QComboBox* combo_alt_from_{nullptr};
    QComboBox* combo_alt_to_{nullptr};
    QPushButton* btn_find_alt_{nullptr};
    QLabel* lbl_result_{nullptr};
    QListWidget* list_alt_path_{nullptr};
};
