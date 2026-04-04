#pragma once
#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include "../services/NavigationService.h"
#include "../services/ScenarioManager.h"

class CampusMapView;

class BfsTab : public QWidget {
    Q_OBJECT
public:
    BfsTab(NavigationService& nav, ScenarioManager& scenario, CampusMapView* mapView,
           const CampusGraph& graph, QWidget* parent = nullptr);

    void refreshNodeList();

private slots:
    void onRunBfs();

private:
    NavigationService& nav_;
    ScenarioManager& scenario_;
    CampusMapView* mapView_;
    const CampusGraph& graph_;

    QComboBox* combo_start_{nullptr};
    QPushButton* btn_run_{nullptr};
    QListWidget* list_result_{nullptr};
    QLabel* lbl_stats_{nullptr};
};
