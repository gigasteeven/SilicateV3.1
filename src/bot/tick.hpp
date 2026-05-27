#include "module/module.hpp"

class TickCounter : public mod::Module {
    INIT_MODULE(TickCounter)

   private:
    uint64_t m_tick;

    void advance() { m_tick++; }
    void reset() { m_tick = 0; }

   public:
    uint64_t current() const { return m_tick; }
    void onEvent(Event e) override;
};

REGISTER_MODULE(TickCounter)
