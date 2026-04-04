#pragma once
#include <QWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QLabel>
#include "../services/ScenarioManager.h"

class CampusMapView;

class ScenarioTab : public QWidget {
    Q_OBJECT
public:
    ScenarioTab(ScenarioManager& scenario, CampusMapView* mapView, QWidget* parent = nullptr);

private slots:
    void onApply();

private:
    ScenarioManager& scenario_;
    CampusMapView* mapView_;
    QCheckBox* chk_mr_{nullptr};
    QRadioButton* rb_new_{nullptr};
    QRadioButton* rb_regular_{nullptr};
    QPushButton* btn_apply_{nullptr};
    QLabel* lbl_info_{nullptr};
};
