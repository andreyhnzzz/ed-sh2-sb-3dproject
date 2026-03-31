#pragma once
#include <Qt>
#include <set>

class QObject;

class InputHandler
{
public:
    InputHandler() = default;
    explicit InputHandler(QObject* /*parent*/) {}

    void keyPressed(int key)  { m_keys.insert(key); }
    void keyReleased(int key) { m_keys.erase(key);  }

    bool isDown(Qt::Key key) const {
        return m_keys.count(static_cast<int>(key)) > 0;
    }

    bool forward()  const { return isDown(Qt::Key_W) || isDown(Qt::Key_Up);    }
    bool back()     const { return isDown(Qt::Key_S) || isDown(Qt::Key_Down);  }
    bool left()     const { return isDown(Qt::Key_A) || isDown(Qt::Key_Left);  }
    bool right()    const { return isDown(Qt::Key_D) || isDown(Qt::Key_Right); }
    bool interact() const { return isDown(Qt::Key_E); }

    void markEConsumed()  { m_eConsumed = true; }
    void clearEConsumed() { if (!isDown(Qt::Key_E)) m_eConsumed = false; }
    bool eJustPressed()   const { return isDown(Qt::Key_E) && !m_eConsumed; }

private:
    std::set<int> m_keys;
    bool m_eConsumed = false;
};
