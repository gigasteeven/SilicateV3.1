#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

// ── Trakines settings menu ─────────────────────────────────
// A simple popup that shows the mod settings and status.
class TrakinesMenu : public geode::Popup<> {
protected:
    bool setup() override;

public:
    static TrakinesMenu* create() {
        auto ret = new TrakinesMenu();
        if (ret->initAnchored(280.f, 200.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
