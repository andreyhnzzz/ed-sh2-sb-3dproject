#include "ComplexityTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

ComplexityTab::ComplexityTab(ComplexityAnalyzer& analyzer, ScenarioManager& scenario,
                              const CampusGraph& graph, QWidget* parent)
    : QWidget(parent), analyzer_(analyzer), scenario_(scenario), graph_(graph)
{
    auto* layout = new QVBoxLayout(this);

    auto* grid = new QGridLayout();
    grid->addWidget(new QLabel("Nodo de inicio para análisis:"), 0, 0);
    combo_start_ = new QComboBox(this);
    grid->addWidget(combo_start_, 0, 1);

    grid->addWidget(new QLabel("Nodo destino:"), 1, 0);
    combo_dest_ = new QComboBox(this);
    grid->addWidget(combo_dest_, 1, 1);

    for (auto& id : graph_.nodeIds()) {
        try {
            const auto item = QString::fromStdString(id + " — " + graph_.getNode(id).name);
            const auto data = QString::fromStdString(id);
            combo_start_->addItem(item, data);
            combo_dest_->addItem(item, data);
        } catch (...) {}
    }
    layout->addLayout(grid);

    btn_analyze_ = new QPushButton("Analizar Complejidad", this);
    layout->addWidget(btn_analyze_);

    table_ = new QTableWidget(0, 4, this);
    table_->setHorizontalHeaderLabels({"Algoritmo", "Nodos Visitados", "Tiempo (us)", "O(V+E) Teorico"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_);

    lbl_info_ = new QLabel(this);
    lbl_info_->setWordWrap(true);
    layout->addWidget(lbl_info_);

    lbl_compare_ = new QLabel(this);
    lbl_compare_->setWordWrap(true);
    layout->addWidget(lbl_compare_);

    layout->addWidget(new QLabel(
        "Nota: Ambos algoritmos tienen complejidad O(V+E). DFS usa pila, BFS usa cola."));

    connect(btn_analyze_, &QPushButton::clicked, this, &ComplexityTab::onAnalyze);
}

void ComplexityTab::onAnalyze() {
    QString startId = combo_start_->currentData().toString();
    QString destId = combo_dest_->currentData().toString();
    if (startId.isEmpty() || destId.isEmpty()) return;

    auto stats = analyzer_.analyze(startId.toStdString(), scenario_.isMobilityReduced());

    table_->setRowCount(static_cast<int>(stats.size()));
    for (int i = 0; i < static_cast<int>(stats.size()); ++i) {
        table_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(stats[i].algorithm)));
        table_->setItem(i, 1, new QTableWidgetItem(QString::number(stats[i].nodes_visited)));
        table_->setItem(i, 2, new QTableWidgetItem(QString::number(stats[i].elapsed_us)));
        table_->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(stats[i].theoretical)));
    }

    lbl_info_->setText(QString("Grafo: V=%1 nodos, E=%2 aristas. Análisis completado.")
                           .arg(graph_.nodeCount()).arg(graph_.edgeCount()));

    auto cmp = analyzer_.compareAlgorithms(startId.toStdString(), destId.toStdString(),
                                           scenario_.isMobilityReduced());
    QString visitedSummary;
    if (!cmp.dfs_reaches_destination && !cmp.bfs_reaches_destination) {
        visitedSummary = "Ninguno de los algoritmos alcanzó el destino.";
    } else if (cmp.bfs_nodes_visited > cmp.dfs_nodes_visited) {
        visitedSummary = "BFS visitó más nodos antes de llegar al destino.";
    } else if (cmp.bfs_nodes_visited < cmp.dfs_nodes_visited) {
        visitedSummary = "DFS visitó más nodos antes de llegar al destino.";
    } else {
        visitedSummary = "BFS y DFS visitaron la misma cantidad de nodos antes de llegar al destino.";
    }

    lbl_compare_->setText(QString(
        "Comparación BFS vs DFS\n"
        "DFS -> destino: %1 | nodos: %2 | tiempo: %3 us\n"
        "BFS -> destino: %4 | nodos: %5 | tiempo: %6 us\n"
        "%7")
        .arg(cmp.dfs_reaches_destination ? "sí" : "no")
        .arg(cmp.dfs_nodes_visited)
        .arg(cmp.dfs_elapsed_us)
        .arg(cmp.bfs_reaches_destination ? "sí" : "no")
        .arg(cmp.bfs_nodes_visited)
        .arg(cmp.bfs_elapsed_us)
        .arg(visitedSummary));
}
