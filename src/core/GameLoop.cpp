#include "GameLoop.h"
#include "render/Renderer.h"
#include "input/InputHandler.h"
#include "core/EventBus.h"

#include <QKeyEvent>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QOpenGLContext>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLFunctions_3_3_Core>
#include <QDebug>
#include <QString>

GameLoop::GameLoop(QWidget* parent)
    : QOpenGLWidget(parent)
{
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    setFormat(fmt);

    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(1024, 768);
}

GameLoop::~GameLoop()
{
    makeCurrent();
    delete m_renderer;
    doneCurrent();
}

void GameLoop::initializeGL()
{
    m_inputHandler = new InputHandler(this);
    m_renderer     = new Renderer();
    m_renderer->initialize(width(), height());

    auto* ctx = QOpenGLContext::currentContext();
    auto* f = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
    if (f) {
        qDebug() << "OpenGL Version:"
                 << reinterpret_cast<const char*>(f->glGetString(GL_VERSION));
    }
    qDebug() << "EcoCampus v0.1 running";

    EventBus::instance().publish("game_start");

    m_lastTime = std::chrono::steady_clock::now();

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &GameLoop::tick);
    m_timer->start(16);
}

void GameLoop::resizeGL(int w, int h)
{
    if (m_renderer) m_renderer->resize(w, h);
}

void GameLoop::paintGL()
{
    if (!m_renderer) return;

    // 1. OpenGL 3D scene
    m_renderer->renderGL();

    // 2. QPainter HUD overlay
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont monoFont("Courier New", 11);
    monoFont.setStyleHint(QFont::Monospace);

    QFont serifFont("Georgia", 13);
    serifFont.setStyleHint(QFont::Serif);

    auto drawShadowText = [&](const QFont& font, int x, int y,
                               const QString& text,
                               const QColor& color = Qt::white,
                               int shadowAlpha = 180) {
        painter.setFont(font);
        painter.setPen(QColor(0, 0, 0, shadowAlpha));
        painter.drawText(x + 1, y + 1, text);
        painter.setPen(color);
        painter.drawText(x, y, text);
    };

    // Version — top left
    drawShadowText(monoFont, 10, 20, "Eco-Campus v0.1");

    // Player position — bottom right
    glm::vec3 pos = m_renderer->hudPlayerPos();
    QString posStr = QString("X: %1  Z: %2")
                         .arg(static_cast<double>(pos.x), 0, 'f', 1)
                         .arg(static_cast<double>(pos.z), 0, 'f', 1);
    {
        QFontMetrics fm(monoFont);
        int posW = fm.horizontalAdvance(posStr);
        drawShadowText(monoFont, width() - posW - 10, height() - 10, posStr);
    }

    // Interaction text — center bottom
    std::string interact = m_renderer->hudInteractText();
    if (!interact.empty()) {
        QString iStr = QString::fromStdString(interact);
        QFontMetrics fm(monoFont);
        int iW = fm.horizontalAdvance(iStr);
        drawShadowText(monoFont, (width() - iW) / 2, height() - 30, iStr,
                       QColor(255, 220, 100));
    }

    // Narrator subtitle — lower area
    std::string narText  = m_renderer->hudNarratorText();
    float       narAlpha = m_renderer->hudNarratorAlpha();
    if (!narText.empty() && narAlpha > 0.0f) {
        int alpha = static_cast<int>(narAlpha * 255.0f);
        QString nStr = QString::fromStdString(narText);
        QFontMetrics sfm(serifFont);
        int nW = sfm.horizontalAdvance(nStr);
        int nX = (width() - nW) / 2;
        int nY = height() - 70;

        painter.setFont(serifFont);
        painter.setPen(QColor(0, 0, 0, static_cast<int>(alpha * 0.7f)));
        painter.drawText(nX + 1, nY + 1, nStr);
        painter.setPen(QColor(255, 255, 255, alpha));
        painter.drawText(nX, nY, nStr);
    }

    painter.end();
}

void GameLoop::tick()
{
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_lastTime).count();
    m_lastTime = now;
    if (dt > 0.1f) dt = 0.1f;

    if (m_renderer && m_inputHandler) {
        m_renderer->update(dt, m_inputHandler);
    }

    update();
}

void GameLoop::keyPressEvent(QKeyEvent* event)
{
    if (m_inputHandler) m_inputHandler->keyPressed(event->key());
    QOpenGLWidget::keyPressEvent(event);
}

void GameLoop::keyReleaseEvent(QKeyEvent* event)
{
    if (m_inputHandler) m_inputHandler->keyReleased(event->key());
    QOpenGLWidget::keyReleaseEvent(event);
}
