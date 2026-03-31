#include "Narrator.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <map>
#include <QDebug>

using json = nlohmann::json;

Narrator& Narrator::instance()
{
    static Narrator n;
    return n;
}

bool Narrator::loadDialogues(const std::string& path)
{
    std::ifstream f(path);
    if (!f.is_open()) {
        qWarning() << "Narrator: cannot open" << path.c_str();
        return false;
    }
    json root;
    try {
        f >> root;
    } catch (const json::exception& e) {
        qWarning() << "Narrator JSON error:" << e.what();
        return false;
    }

    m_dialogues.clear();
    if (root.contains("dialogues")) {
        for (auto& [key, val] : root["dialogues"].items()) {
            m_dialogues[key] = val.get<std::string>();
        }
    }
    return true;
}

void Narrator::onEvent(const std::string& event, const std::string& /*data*/)
{
    auto it = m_dialogues.find(event);
    if (it != m_dialogues.end()) {
        m_currentText = it->second;
        m_timer = 0.0f;
        m_alpha = 1.0f;
    }
}

void Narrator::update(float dt)
{
    if (m_alpha <= 0.0f) return;

    m_timer += dt;
    if (m_timer >= m_displayDur) {
        m_alpha = 0.0f;
        m_currentText.clear();
    } else if (m_timer >= m_displayDur - m_fadeDur) {
        float fadeProgress = (m_timer - (m_displayDur - m_fadeDur)) / m_fadeDur;
        m_alpha = 1.0f - fadeProgress;
    }
}

std::string Narrator::currentText() const
{
    return m_currentText;
}

float Narrator::currentAlpha() const
{
    return m_alpha;
}
