#include "ScenarioTab.h"
#include "CampusMapView.h"
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>

ScenarioTab::ScenarioTab(ScenarioManager& scenario, CampusMapView* mapView, QWidget* parent)
    : QWidget(parent), scenario_(scenario), mapView_(mapView)
{
    auto* layout = new QVBoxLayout(this);

    auto* grpMR = new QGroupBox("Perfil de Accesibilidad");
    auto* mrLayout = new QVBoxLayout(grpMR);
    chk_mr_ = new QCheckBox("Movilidad Reducida (evita escaleras, usa solo elevador)");
    mrLayout->addWidget(chk_mr_);
    layout->addWidget(grpMR);

    auto* grpST = new QGroupBox("Tipo de Estudiante");
    auto* stLayout = new QVBoxLayout(grpST);
    rb_new_ = new QRadioButton("Estudiante Nuevo (rutas mas simples)");
    rb_regular_ = new QRadioButton("Estudiante Regular");
    rb_regular_->setChecked(true);
    stLayout->addWidget(rb_new_);
    stLayout->addWidget(rb_regular_);
    layout->addWidget(grpST);

    btn_apply_ = new QPushButton("Aplicar Escenario", this);
    layout->addWidget(btn_apply_);

    lbl_info_ = new QLabel(this);
    lbl_info_->setWordWrap(true);
    layout->addWidget(lbl_info_);

    layout->addStretch();
    connect(btn_apply_, &QPushButton::clicked, this, &ScenarioTab::onApply);
}

void ScenarioTab::onApply() {
    scenario_.setMobilityReduced(chk_mr_->isChecked());
    scenario_.setStudentType(rb_new_->isChecked() ? StudentType::NEW_STUDENT : StudentType::REGULAR_STUDENT);

    if (mapView_) mapView_->setMobilityMode(chk_mr_->isChecked());

    QString msg = "Escenario aplicado:\n";
    msg += chk_mr_->isChecked() ? "Movilidad reducida ACTIVADA\n" : "Movilidad normal\n";
    msg += rb_new_->isChecked() ? "Estudiante Nuevo" : "Estudiante Regular";
    lbl_info_->setText(msg);
    lbl_info_->setStyleSheet("color: #2980B9; font-style: italic;");
}
