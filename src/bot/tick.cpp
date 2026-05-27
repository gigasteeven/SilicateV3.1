#include "tick.hpp"

#include "module/module.hpp"

using E = mod::Module::Event;
using namespace mod::events;

void TickCounter::onEvent(E e) {
    std::visit(mod::Match {
        [this](TickPhysics) {
            this->advance();
        },
        [this](LevelRestart lr) {
            if (!lr.intentional) {
                this->reset();
            }
        },
    }, e);
}
