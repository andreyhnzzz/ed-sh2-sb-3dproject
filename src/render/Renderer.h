#pragma once
#include <QOpenGLFunctions_3_3_Core>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

class Shader;
class InputHandler;
class Player;
class Door;

struct MeshData {
    unsigned int vao = 0, vbo = 0, ebo = 0;
    int indexCount = 0;
};

class Renderer : protected QOpenGLFunctions_3_3_Core
{
public:
    Renderer();
    ~Renderer();
    Renderer(Renderer&&) noexcept;
    Renderer& operator=(Renderer&&) noexcept;
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void initialize(int w, int h);
    void resize(int w, int h);
    void update(float dt, InputHandler* input);
    void renderGL();   // OpenGL draw calls only

    // HUD data for QPainter overlay in GameLoop
    glm::vec3   hudPlayerPos()     const { return m_hudPlayerPos;     }
    std::string hudInteractText()  const { return m_hudInteractText;  }
    std::string hudNarratorText()  const { return m_hudNarratorText;  }
    float       hudNarratorAlpha() const { return m_hudNarratorAlpha; }

private:
    int m_width  = 1024;
    int m_height = 768;

    std::unique_ptr<Shader> m_shader;

    MeshData m_groundMesh;
    MeshData m_cubeMesh;

    std::unique_ptr<Player>            m_player;
    std::vector<std::unique_ptr<Door>> m_doors;
    std::vector<std::string>           m_doorNames;

    int  m_nearDoorIndex = -1;

    // HUD state
    glm::vec3   m_hudPlayerPos{};
    std::string m_hudInteractText;
    std::string m_hudNarratorText;
    float       m_hudNarratorAlpha = 0.0f;

    MeshData buildGroundQuad();
    MeshData buildCube();
    void     freeMesh(MeshData& m);

    void drawMesh(const MeshData& mesh, const glm::mat4& model,
                  const glm::vec3& color, const glm::mat4& view,
                  const glm::mat4& proj);
};
