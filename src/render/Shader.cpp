#include "Shader.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLContext>
#include <fstream>
#include <sstream>
#include <QDebug>

static QOpenGLFunctions_3_3_Core* gl()
{
    return QOpenGLContext::currentContext()
               ->versionFunctions<QOpenGLFunctions_3_3_Core>();
}

Shader::~Shader()
{
    if (m_programId) gl()->glDeleteProgram(m_programId);
}

std::string Shader::readFile(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        qWarning() << "Shader: cannot open" << path.c_str();
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool Shader::compileShader(unsigned int& shaderId, unsigned int type,
                            const std::string& source)
{
    auto* g = gl();
    shaderId = g->glCreateShader(type);
    const char* src = source.c_str();
    g->glShaderSource(shaderId, 1, &src, nullptr);
    g->glCompileShader(shaderId);

    int success = 0;
    g->glGetShaderiv(shaderId, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        g->glGetShaderInfoLog(shaderId, 512, nullptr, log);
        qWarning() << "Shader compile error:" << log;
        return false;
    }
    return true;
}

bool Shader::load(const std::string& vertPath, const std::string& fragPath)
{
    auto* g = gl();
    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);
    if (vertSrc.empty() || fragSrc.empty()) return false;

    unsigned int vert = 0, frag = 0;
    if (!compileShader(vert, GL_VERTEX_SHADER,   vertSrc)) return false;
    if (!compileShader(frag, GL_FRAGMENT_SHADER, fragSrc)) {
        g->glDeleteShader(vert);
        return false;
    }

    m_programId = g->glCreateProgram();
    g->glAttachShader(m_programId, vert);
    g->glAttachShader(m_programId, frag);
    g->glLinkProgram(m_programId);

    int success = 0;
    g->glGetProgramiv(m_programId, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        g->glGetProgramInfoLog(m_programId, 512, nullptr, log);
        qWarning() << "Shader link error:" << log;
    }

    g->glDeleteShader(vert);
    g->glDeleteShader(frag);
    return success != 0;
}

void Shader::use() const
{
    gl()->glUseProgram(m_programId);
}

void Shader::setMat4(const std::string& name, const float* value) const
{
    int loc = gl()->glGetUniformLocation(m_programId, name.c_str());
    gl()->glUniformMatrix4fv(loc, 1, GL_FALSE, value);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
    int loc = gl()->glGetUniformLocation(m_programId, name.c_str());
    gl()->glUniform3f(loc, x, y, z);
}

void Shader::setFloat(const std::string& name, float value) const
{
    int loc = gl()->glGetUniformLocation(m_programId, name.c_str());
    gl()->glUniform1f(loc, value);
}
