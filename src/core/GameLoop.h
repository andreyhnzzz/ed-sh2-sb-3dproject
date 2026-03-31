#pragma once
#include <QOpenGLWidget>
#include <QTimer>
#include <chrono>

class Renderer;
class InputHandler;

class GameLoop : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit GameLoop(QWidget* parent = nullptr);
    ~GameLoop() override;

protected:
    void initializeGL()      override;
    void resizeGL(int w, int h) override;
    void paintGL()           override;
    void keyPressEvent(QKeyEvent* event)   override;
    void keyReleaseEvent(QKeyEvent* event) override;

private slots:
    void tick();

private:
    QTimer* m_timer = nullptr;
    std::chrono::steady_clock::time_point m_lastTime;

    Renderer*     m_renderer     = nullptr;
    InputHandler* m_inputHandler = nullptr;
};
