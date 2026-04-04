#pragma once
#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include "../services/NavigationService.h"
#include "../core/graph/CampusGraph.h"

class ConnectivityTab : public QWidget {
    Q_OBJECT
public:
    ConnectivityTab(NavigationService& nav, const CampusGraph& graph, QWidget* parent = nullptr);

private slots:
    void onCheckConnectivity();

private:
    NavigationService& nav_;
    const CampusGraph& graph_;
    QPushButton* btn_check_{nullptr};
    QLabel* lbl_result_{nullptr};
    QListWidget* list_components_{nullptr};
};
