#include "ComplexityTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>

ComplexityTab::ComplexityTab(ComplexityAnalyzer& analyzer, ScenarioManager& scenario,
                              const CampusGraph& graph, QWidget* parent)
    : QWidget(parent), analyzer_(analyzer), scenario_(scenario), graph_(graph)
{
    auto* layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel("Nodo de inicio para análisis:"));
    combo_start_ = new QComboBox(this);
    for (auto& id : graph_.nodeIds()) {
        try {
            combo_start_->addItem(QString::fromStdString(id + " — " + graph_.getNode(id).name),
                                   QString::fromStdString(id));
        } catch (...) {}
    }
    layout->addWidget(combo_start_);

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

    layout->addWidget(new QLabel(
        "Nota: Ambos algoritmos tienen complejidad O(V+E). DFS usa pila, BFS usa cola."));

    connect(btn_analyze_, &QPushButton::clicked, this, &ComplexityTab::onAnalyze);
}

void ComplexityTab::onAnalyze() {
    QString startId = combo_start_->currentData().toString();
    if (startId.isEmpty()) return;

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
}
