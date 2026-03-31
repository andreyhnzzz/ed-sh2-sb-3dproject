#include "EventBus.h"

EventBus& EventBus::instance()
{
    static EventBus bus;
    return bus;
}

void EventBus::subscribe(const std::string& event, Callback cb)
{
    m_listeners[event].push_back(std::move(cb));
}

void EventBus::emit(const std::string& event, const std::string& data)
{
    auto it = m_listeners.find(event);
    if (it != m_listeners.end()) {
        for (auto& cb : it->second) {
            cb(data);
        }
    }
}
