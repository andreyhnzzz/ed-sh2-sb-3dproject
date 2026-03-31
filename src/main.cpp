#include <QApplication>
#include <QMainWindow>
#include "core/GameLoop.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("Eco-Campus — UTN San Carlos");
    window.setFixedSize(1024, 768);

    GameLoop* gameLoop = new GameLoop(&window);
    window.setCentralWidget(gameLoop);

    window.show();
    gameLoop->setFocus();

    return app.exec();
}
