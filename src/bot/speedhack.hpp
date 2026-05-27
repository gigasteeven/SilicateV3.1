#include "module/module.hpp"
#include "settings/settings.hpp"
#include "shared/value/value.hpp"

class Speedhack : public mod::Module {
    INIT_MODULE(Speedhack)

    SLValuePtr<double> m_speed =
        SLValue<double>::create("updater.speedhack", &SLSettings::get()->speed);
};

REGISTER_MODULE(Speedhack)
