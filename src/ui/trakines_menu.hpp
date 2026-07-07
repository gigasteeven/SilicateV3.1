#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

// ── Trakines settings menu ─────────────────────────────────
// A simple popup that shows the mod settings and status.
class TrakinesMenu : public geode::Popup {
protected:
    bool init(float width, float height);
    void onClose(CCObject* sender) override;

public:
    static TrakinesMenu* create() {
        auto ret = new TrakinesMenu();
        if (ret && ret->init(280.f, 200.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};
