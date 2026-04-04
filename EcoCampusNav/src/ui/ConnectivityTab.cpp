#include "ConnectivityTab.h"
#include <QVBoxLayout>
#include <QLabel>

ConnectivityTab::ConnectivityTab(NavigationService& nav, const CampusGraph& graph, QWidget* parent)
    : QWidget(parent), nav_(nav), graph_(graph)
{
    auto* layout = new QVBoxLayout(this);
    btn_check_ = new QPushButton("Verificar Conexidad", this);
    layout->addWidget(btn_check_);
    lbl_result_ = new QLabel(this);
    lbl_result_->setWordWrap(true);
    layout->addWidget(lbl_result_);
    layout->addWidget(new QLabel("Componentes:"));
    list_components_ = new QListWidget(this);
    layout->addWidget(list_components_);

    connect(btn_check_, &QPushButton::clicked, this, &ConnectivityTab::onCheckConnectivity);
}

void ConnectivityTab::onCheckConnectivity() {
    bool connected = nav_.checkConnectivity();
    auto components = nav_.getComponents();

    if (connected) {
        lbl_result_->setText("El grafo ES conexo. Todos los nodos son accesibles desde cualquier punto.");
        lbl_result_->setStyleSheet("color: green; font-weight: bold;");
    } else {
        lbl_result_->setText(QString("El grafo NO es conexo. Hay %1 componentes.")
                                 .arg(components.size()));
        lbl_result_->setStyleSheet("color: red; font-weight: bold;");
    }

    list_components_->clear();
    for (size_t ci = 0; ci < components.size(); ++ci) {
        const auto& comp = components[ci];
        QString names;
        for (auto& id : comp) {
            try {
                names += QString::fromStdString(graph_.getNode(id).name) + ", ";
            } catch (...) {}
        }
        if (names.endsWith(", ")) names.chop(2);
        list_components_->addItem(QString("Componente %1 (%2 nodos): %3")
                                      .arg(ci + 1).arg(comp.size()).arg(names));
    }
}
