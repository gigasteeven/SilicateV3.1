#pragma once

#include <variant>
#include <functional>

namespace mod {

namespace events {
    struct TickPhysics {};
    struct LevelRestart {
        bool intentional = false;
    };
}

template<class... Ts>
struct Match : Ts... { using Ts::operator()...; };

template<class... Ts>
Match(Ts...) -> Match<Ts...>;

class Module {
public:
    using Event = std::variant<events::TickPhysics, events::LevelRestart>;

    virtual ~Module() = default;
    virtual void onEvent(Event e) { (void)e; }
};

} // namespace mod

#define INIT_MODULE(Name) \
    public: \
        static Name& instance() { \
            static Name inst; \
            return inst; \
        }

#define REGISTER_MODULE(Name)
