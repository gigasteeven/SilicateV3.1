#include "trakines_menu.hpp"
#include "../global.hpp"

bool TrakinesMenu::init(float width, float height) {
    if (!geode::Popup::init(width, height))
        return false;

    setTitle("Trakines");

    auto& g = TrakinesGlobal::get();

    auto label = CCLabelBMFont::create("Trakines Settings", "goldFont.fnt");
    label->setPosition(m_size.width / 2, m_size.height / 2);
    this->addChild(label);

    // Status info
    std::string status;
    status += "Layout Mode: " + std::string(g.layoutMode ? "ON" : "OFF") + "\n";
    status += "Spout2: " + std::string(g.spoutEnabled ? "ON" : "OFF") + "\n";
    status += "Mirror: " + std::to_string(g.mirrorWidth) + "x" + std::to_string(g.mirrorHeight);
    status += " @" + std::to_string(g.mirrorFps) + " FPS";

    auto statusLabel = CCLabelBMFont::create(status.c_str(), "chatFont.fnt");
    statusLabel->setPosition({m_size.width / 2, m_size.height / 2 - 40.f});
    statusLabel->setScale(0.8f);
    this->addChild(statusLabel);

    return true;
}

void TrakinesMenu::onClose(CCObject* sender) {
    geode::Popup::onClose(sender);
}
