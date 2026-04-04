#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QSplitter>
#include <memory>
#include "../core/graph/CampusGraph.h"
#include "../services/NavigationService.h"
#include "../services/ScenarioManager.h"
#include "../services/ComplexityAnalyzer.h"
#include "../services/ResilienceService.h"
#include "CampusMapView.h"
#include "DfsTab.h"
#include "BfsTab.h"
#include "ConnectivityTab.h"
#include "PathfindTab.h"
#include "ScenarioTab.h"
#include "ComplexityTab.h"
#include "ResilienceTab.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(CampusGraph graph, QWidget* parent = nullptr);

private slots:
    void onNodeClicked(const QString& nodeId);
    void onTabChanged(int index);

private:
    CampusGraph graph_;
    NavigationService nav_service_;
    ScenarioManager scenario_manager_;
    ComplexityAnalyzer complexity_analyzer_;
    ResilienceService resilience_service_;

    QSplitter* splitter_{nullptr};
    CampusMapView* map_view_{nullptr};
    QTabWidget* tab_widget_{nullptr};

    DfsTab* dfs_tab_{nullptr};
    BfsTab* bfs_tab_{nullptr};
    ConnectivityTab* conn_tab_{nullptr};
    PathfindTab* path_tab_{nullptr};
    ScenarioTab* scenario_tab_{nullptr};
    ComplexityTab* complexity_tab_{nullptr};
    ResilienceTab* resilience_tab_{nullptr};

    void setupUi();
    void setupStatusBar();
};
