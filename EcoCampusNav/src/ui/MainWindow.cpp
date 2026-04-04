#include "MainWindow.h"
#include <QStatusBar>
#include <QVBoxLayout>
#include <QLabel>

MainWindow::MainWindow(CampusGraph graph, QWidget* parent)
    : QMainWindow(parent)
    , graph_(std::move(graph))
    , nav_service_(graph_)
    , scenario_manager_()
    , complexity_analyzer_(graph_)
    , resilience_service_(graph_)
{
    setWindowTitle("EcoCampusNav — Sistema de Navegación del Campus");
    resize(1400, 800);
    setupUi();
    setupStatusBar();
    map_view_->loadGraph(graph_);
}

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    splitter_ = new QSplitter(Qt::Horizontal, central);
    mainLayout->addWidget(splitter_);

    map_view_ = new CampusMapView(splitter_);
    splitter_->addWidget(map_view_);

    tab_widget_ = new QTabWidget(splitter_);
    splitter_->addWidget(tab_widget_);
    splitter_->setStretchFactor(0, 6);
    splitter_->setStretchFactor(1, 4);

    dfs_tab_ = new DfsTab(nav_service_, scenario_manager_, map_view_, graph_, tab_widget_);
    tab_widget_->addTab(dfs_tab_, "DFS");

    bfs_tab_ = new BfsTab(nav_service_, scenario_manager_, map_view_, graph_, tab_widget_);
    tab_widget_->addTab(bfs_tab_, "BFS");

    conn_tab_ = new ConnectivityTab(nav_service_, graph_, tab_widget_);
    tab_widget_->addTab(conn_tab_, "Conectividad");

    path_tab_ = new PathfindTab(nav_service_, scenario_manager_, map_view_, graph_, tab_widget_);
    tab_widget_->addTab(path_tab_, "Camino");

    scenario_tab_ = new ScenarioTab(scenario_manager_, map_view_, tab_widget_);
    tab_widget_->addTab(scenario_tab_, "Escenarios");

    complexity_tab_ = new ComplexityTab(complexity_analyzer_, scenario_manager_, graph_, tab_widget_);
    tab_widget_->addTab(complexity_tab_, "Complejidad");

    resilience_tab_ = new ResilienceTab(resilience_service_, scenario_manager_, map_view_, graph_, tab_widget_);
    tab_widget_->addTab(resilience_tab_, "Fallos");

    connect(map_view_, &CampusMapView::nodeClicked, this, &MainWindow::onNodeClicked);
    connect(tab_widget_, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
}

void MainWindow::setupStatusBar() {
    statusBar()->showMessage(QString("EcoCampusNav | V=%1 nodos, E=%2 aristas | Listo")
                                 .arg(graph_.nodeCount())
                                 .arg(graph_.edgeCount()));
}

void MainWindow::onNodeClicked(const QString& nodeId) {
    try {
        const Node& n = graph_.getNode(nodeId.toStdString());
        statusBar()->showMessage(QString("Nodo seleccionado: %1 — %2 (tipo: %3, z=%4)")
                                     .arg(nodeId)
                                     .arg(QString::fromStdString(n.name))
                                     .arg(QString::fromStdString(n.type))
                                     .arg(n.z));
    } catch (...) {}
}

void MainWindow::onTabChanged(int /*index*/) {
    map_view_->clearHighlights();
}
