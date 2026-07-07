#include "trakines_menu.hpp"
#include "../global.hpp"

bool TrakinesMenu::setup() {
    setTitle("Trakines");

    auto& g = TrakinesGlobal::get();

    auto label = CCLabelBMFont::create("Trakines Settings", "goldFont.fnt");
    label->setPosition(m_mainLayer->getContentSize() / 2);
    m_mainLayer->addChild(label);

    // Status info
    std::string status;
    status += "Layout Mode: " + std::string(g.layoutMode ? "ON" : "OFF") + "\n";
    status += "Spout2: " + std::string(g.spoutEnabled ? "ON" : "OFF") + "\n";
    status += "Mirror: " + std::to_string(g.mirrorWidth) + "x" + std::to_string(g.mirrorHeight);
    status += " @" + std::to_string(g.mirrorFps) + " FPS";

    auto statusLabel = CCLabelBMFont::create(status.c_str(), "chatFont.fnt");
    statusLabel->setPosition({m_mainLayer->getContentSize().width / 2,
                              m_mainLayer->getContentSize().height / 2 - 40.f});
    statusLabel->setScale(0.8f);
    m_mainLayer->addChild(statusLabel);

    // Close button
    auto closeButton = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("OK"),
        this,
        menu_selector(TrakinesMenu::onClose)
    );
    closeButton->setPosition({m_mainLayer->getContentSize().width / 2, 30.f});
    m_buttonMenu->addChild(closeButton);

    return true;
}
