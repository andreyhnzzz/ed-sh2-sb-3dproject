#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

class EventBus
{
public:
    using Callback = std::function<void(const std::string& data)>;

    static EventBus& instance();

    void subscribe(const std::string& event, Callback cb);
    void emit(const std::string& event, const std::string& data = "");

private:
    EventBus() = default;
    std::map<std::string, std::vector<Callback>> m_listeners;
};
