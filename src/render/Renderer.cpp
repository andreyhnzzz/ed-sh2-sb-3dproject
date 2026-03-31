#include "Renderer.h"
#include "Shader.h"
#include "entity/Player.h"
#include "entity/Door.h"
#include "scene/SceneManager.h"
#include "scene/Room.h"
#include "narrative/Narrator.h"
#include "core/EventBus.h"
#include "input/InputHandler.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <QDebug>

// ---------------------------------------------------------------------------
// Geometry data
// ---------------------------------------------------------------------------
static const float CUBE_VERTS[] = {
    // pos               normal
    // Front
    -0.5f,-0.5f, 0.5f,  0,0,1,
     0.5f,-0.5f, 0.5f,  0,0,1,
     0.5f, 0.5f, 0.5f,  0,0,1,
    -0.5f, 0.5f, 0.5f,  0,0,1,
    // Back
    -0.5f,-0.5f,-0.5f,  0,0,-1,
     0.5f,-0.5f,-0.5f,  0,0,-1,
     0.5f, 0.5f,-0.5f,  0,0,-1,
    -0.5f, 0.5f,-0.5f,  0,0,-1,
    // Left
    -0.5f,-0.5f,-0.5f, -1,0,0,
    -0.5f,-0.5f, 0.5f, -1,0,0,
    -0.5f, 0.5f, 0.5f, -1,0,0,
    -0.5f, 0.5f,-0.5f, -1,0,0,
    // Right
     0.5f,-0.5f,-0.5f,  1,0,0,
     0.5f,-0.5f, 0.5f,  1,0,0,
     0.5f, 0.5f, 0.5f,  1,0,0,
     0.5f, 0.5f,-0.5f,  1,0,0,
    // Top
    -0.5f, 0.5f,-0.5f,  0,1,0,
     0.5f, 0.5f,-0.5f,  0,1,0,
     0.5f, 0.5f, 0.5f,  0,1,0,
    -0.5f, 0.5f, 0.5f,  0,1,0,
    // Bottom
    -0.5f,-0.5f,-0.5f,  0,-1,0,
     0.5f,-0.5f,-0.5f,  0,-1,0,
     0.5f,-0.5f, 0.5f,  0,-1,0,
    -0.5f,-0.5f, 0.5f,  0,-1,0,
};
static const unsigned int CUBE_INDICES[] = {
     0, 1, 2,  2, 3, 0,
     4, 6, 5,  6, 4, 7,
     8, 9,10, 10,11, 8,
    12,14,13, 14,12,15,
    16,17,18, 18,19,16,
    20,22,21, 22,20,23,
};

static const float GROUND_VERTS[] = {
    -100,0,-100,  0,1,0,
     100,0,-100,  0,1,0,
     100,0, 100,  0,1,0,
    -100,0, 100,  0,1,0,
};
static const unsigned int GROUND_INDICES[] = { 0,1,2, 2,3,0 };

// ---------------------------------------------------------------------------
Renderer::~Renderer()
{
    freeMesh(m_groundMesh);
    freeMesh(m_cubeMesh);
}

void Renderer::freeMesh(MeshData& m)
{
    if (m.vao) { glDeleteVertexArrays(1, &m.vao); m.vao = 0; }
    if (m.vbo) { glDeleteBuffers(1, &m.vbo);      m.vbo = 0; }
    if (m.ebo) { glDeleteBuffers(1, &m.ebo);      m.ebo = 0; }
}

static MeshData uploadMesh(QOpenGLFunctions_3_3_Core* gl,
                            const float* verts, GLsizeiptr vSize,
                            const unsigned int* idx, GLsizeiptr iSize,
                            int indexCount)
{
    MeshData md;
    md.indexCount = indexCount;
    gl->glGenVertexArrays(1, &md.vao);
    gl->glGenBuffers(1, &md.vbo);
    gl->glGenBuffers(1, &md.ebo);

    gl->glBindVertexArray(md.vao);

    gl->glBindBuffer(GL_ARRAY_BUFFER, md.vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, vSize, verts, GL_STATIC_DRAW);

    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, md.ebo);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, iSize, idx, GL_STATIC_DRAW);

    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    gl->glEnableVertexAttribArray(1);

    gl->glBindVertexArray(0);
    return md;
}

MeshData Renderer::buildGroundQuad()
{
    return uploadMesh(this, GROUND_VERTS, sizeof(GROUND_VERTS),
                      GROUND_INDICES, sizeof(GROUND_INDICES), 6);
}

MeshData Renderer::buildCube()
{
    return uploadMesh(this, CUBE_VERTS, sizeof(CUBE_VERTS),
                      CUBE_INDICES, sizeof(CUBE_INDICES), 36);
}

void Renderer::initialize(int w, int h)
{
    initializeOpenGLFunctions();
    m_width  = w;
    m_height = h;

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.4f, 0.6f, 0.9f, 1.0f);

    m_shader = std::make_unique<Shader>();
    if (!m_shader->load("assets/shaders/vertex.glsl",
                        "assets/shaders/fragment.glsl")) {
        qWarning() << "Renderer: shader load failed";
    }

    m_groundMesh = buildGroundQuad();
    m_cubeMesh   = buildCube();

    SceneManager::instance().loadFromFile("assets/maps/rooms.json");
    Narrator::instance().loadDialogues("assets/narrative/dialogues.json");

    auto& eb  = EventBus::instance();
    auto& nar = Narrator::instance();
    eb.subscribe("game_start",  [&nar](const std::string& d){ nar.onEvent("game_start", d);  });
    eb.subscribe("door_opened", [&nar](const std::string& d){ nar.onEvent("door_opened", d); });
    eb.subscribe("door_locked", [&nar](const std::string& d){ nar.onEvent("door_locked", d); });
    eb.subscribe("room_changed",[&nar](const std::string& d){ nar.onEvent("room_changed", d);});

    m_player = std::make_unique<Player>();

    Room* room = SceneManager::instance().activeRoom();
    if (room) {
        for (auto& dd : room->doors) {
            m_doors.push_back(std::make_unique<Door>(dd.position, dd.target_room, dd.locked));
            m_doorNames.push_back(dd.id);
        }
    }
}

void Renderer::resize(int w, int h)
{
    m_width  = w;
    m_height = h;
    glViewport(0, 0, w, h);
}

void Renderer::update(float dt, InputHandler* input)
{
    m_player->update(dt,
                     input->forward(), input->back(),
                     input->left(),    input->right());

    glm::vec3 playerPos = m_player->position();

    Room* room = SceneManager::instance().activeRoom();
    if (room) room->vcRoad.update(playerPos, dt);

    m_nearDoorIndex = -1;
    for (int i = 0; i < (int)m_doors.size(); ++i) {
        m_doors[i]->update(dt);
        if (m_doors[i]->isNearPlayer(playerPos))
            m_nearDoorIndex = i;
    }

    input->clearEConsumed();
    if (m_nearDoorIndex >= 0 && input->eJustPressed()) {
        m_doors[m_nearDoorIndex]->tryOpen();
        input->markEConsumed();
    }

    for (auto& door : m_doors) {
        if (door->playerCanEnter(playerPos)) {
            std::string target = door->targetRoom();
            EventBus::instance().publish("room_changed", target);
            SceneManager::instance().loadRoom(target);
        }
    }

    Narrator::instance().update(dt);

    // Update HUD data
    m_hudPlayerPos     = playerPos;
    m_hudNarratorText  = Narrator::instance().currentText();
    m_hudNarratorAlpha = Narrator::instance().currentAlpha();

    m_hudInteractText.clear();
    if (m_nearDoorIndex >= 0) {
        DoorState st = m_doors[m_nearDoorIndex]->state();
        if (st == DoorState::LOCKED)
            m_hudInteractText = "[Bloqueada]";
        else if (st == DoorState::CLOSED || st == DoorState::OPENING)
            m_hudInteractText = "[E] Abrir";
    }
}

void Renderer::drawMesh(const MeshData& mesh, const glm::mat4& model,
                         const glm::vec3& color, const glm::mat4& view,
                         const glm::mat4& proj)
{
    m_shader->use();
    m_shader->setMat4("model",       glm::value_ptr(model));
    m_shader->setMat4("view",        glm::value_ptr(view));
    m_shader->setMat4("projection",  glm::value_ptr(proj));
    m_shader->setVec3("objectColor", color.r, color.g, color.b);
    m_shader->setVec3("lightDir",    1.0f, -2.0f, 1.0f);
    m_shader->setVec3("lightColor",  1.0f, 1.0f, 1.0f);
    m_shader->setFloat("ambientStrength", 0.3f);

    glBindVertexArray(mesh.vao);
    glDrawElements(GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Renderer::renderGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Room* room = SceneManager::instance().activeRoom();
    if (!room || !m_shader) return;

    float aspect = (m_height > 0) ? (float)m_width / (float)m_height : 1.0f;
    glm::mat4 view = room->vcRoad.getViewMatrix();
    glm::mat4 proj = room->vcRoad.getProjectionMatrix(aspect);

    // Ground
    drawMesh(m_groundMesh, glm::mat4(1.0f), glm::vec3(0.2f,0.35f,0.15f), view, proj);

    // Biblioteca
    {
        glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-30,2,10)),
                                 glm::vec3(8,4,6));
        drawMesh(m_cubeMesh, m, glm::vec3(0.2f,0.3f,0.8f), view, proj);
    }
    // Comedor
    {
        glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0,1.5f,15)),
                                 glm::vec3(10,3,8));
        drawMesh(m_cubeMesh, m, glm::vec3(0.9f,0.4f,0.1f), view, proj);
    }
    // Aulas
    {
        glm::mat4 m = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(30,2.5f,5)),
                                 glm::vec3(14,5,10));
        drawMesh(m_cubeMesh, m, glm::vec3(0.5f,0.5f,0.5f), view, proj);
    }

    // Player
    drawMesh(m_cubeMesh, m_player->modelMatrix(), glm::vec3(1.0f,0.9f,0.0f), view, proj);

    // Doors
    for (auto& door : m_doors) {
        drawMesh(m_cubeMesh, door->modelMatrix(), glm::vec3(0.55f,0.27f,0.07f), view, proj);
    }
}
