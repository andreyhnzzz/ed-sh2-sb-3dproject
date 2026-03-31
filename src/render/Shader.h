#pragma once
#include <string>

class Shader
{
public:
    Shader() = default;
    ~Shader();

    bool load(const std::string& vertPath, const std::string& fragPath);
    void use() const;

    void setMat4(const std::string& name, const float* value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setFloat(const std::string& name, float value) const;

    unsigned int id() const { return m_programId; }

private:
    unsigned int m_programId = 0;

    bool compileShader(unsigned int& shaderId, unsigned int type,
                       const std::string& source);
    static std::string readFile(const std::string& path);
};
