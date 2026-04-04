#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include "../services/NavigationService.h"
#include "../services/ScenarioManager.h"
#include "../core/graph/CampusGraph.h"

class CampusMapView;

class PathfindTab : public QWidget {
    Q_OBJECT
public:
    PathfindTab(NavigationService& nav, ScenarioManager& scenario, CampusMapView* mapView,
                const CampusGraph& graph, QWidget* parent = nullptr);
    void refreshNodeList();

private slots:
    void onFindPath();

private:
    NavigationService& nav_;
    ScenarioManager& scenario_;
    CampusMapView* mapView_;
    const CampusGraph& graph_;
    QComboBox* combo_from_{nullptr};
    QComboBox* combo_to_{nullptr};
    QPushButton* btn_find_{nullptr};
    QListWidget* list_path_{nullptr};
    QLabel* lbl_total_{nullptr};
};
