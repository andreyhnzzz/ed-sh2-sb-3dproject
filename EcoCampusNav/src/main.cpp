#include <QApplication>
#include <QMessageBox>
#include <filesystem>
#include <iostream>
#include "ui/MainWindow.h"
#include "repositories/JsonGraphRepository.h"

namespace fs = std::filesystem;

static std::string findCampusJson() {
    std::vector<std::string> candidates = {
        "campus.json",
        "../campus.json",
        "../../campus.json",
        "../EcoCampusNav/campus.json"
    };

    if (!QCoreApplication::applicationDirPath().isEmpty()) {
        std::string exeDir = QCoreApplication::applicationDirPath().toStdString();
        candidates.push_back(exeDir + "/campus.json");
        candidates.push_back(exeDir + "/../campus.json");
        candidates.push_back(exeDir + "/../../campus.json");
        candidates.push_back(exeDir + "/../EcoCampusNav/campus.json");
    }

    for (auto& c : candidates) {
        if (fs::exists(c)) return c;
    }
    return "";
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("EcoCampusNav");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Universidad");

    std::string path = findCampusJson();
    if (path.empty()) {
        QMessageBox::critical(nullptr, "Error",
            "No se encontro campus.json.\nColoque el archivo en el directorio de trabajo.");
        return 1;
    }

    CampusGraph graph;
    try {
        graph = JsonGraphRepository::loadFromFile(path);
    } catch (const std::exception& ex) {
        QMessageBox::critical(nullptr, "Error al cargar datos",
            QString("Error: %1").arg(ex.what()));
        return 1;
    }

    MainWindow window(std::move(graph));
    window.show();
    return app.exec();
}
