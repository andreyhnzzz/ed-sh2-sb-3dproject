#pragma once
#include <QWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include "../services/ComplexityAnalyzer.h"
#include "../services/ScenarioManager.h"
#include "../core/graph/CampusGraph.h"

class ComplexityTab : public QWidget {
    Q_OBJECT
public:
    ComplexityTab(ComplexityAnalyzer& analyzer, ScenarioManager& scenario,
                  const CampusGraph& graph, QWidget* parent = nullptr);

private slots:
    void onAnalyze();

private:
    ComplexityAnalyzer& analyzer_;
    ScenarioManager& scenario_;
    const CampusGraph& graph_;
    QComboBox* combo_start_{nullptr};
    QPushButton* btn_analyze_{nullptr};
    QTableWidget* table_{nullptr};
    QLabel* lbl_info_{nullptr};
};
