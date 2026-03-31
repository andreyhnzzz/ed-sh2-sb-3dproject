#pragma once
#include <map>
#include <string>

class Narrator
{
public:
    static Narrator& instance();

    bool loadDialogues(const std::string& path);
    void onEvent(const std::string& event, const std::string& data = "");
    void update(float dt);

    std::string currentText()  const;
    float       currentAlpha() const;

private:
    Narrator() = default;

    std::map<std::string, std::string> m_dialogues;
    std::string m_currentText;
    float m_timer      = 0.0f;
    float m_alpha      = 0.0f;
    float m_displayDur = 4.0f;
    float m_fadeDur    = 1.0f;
};
