#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/binding/EditorUI.hpp>
#include "AILevelLayer.hpp"

using namespace geode::prelude;

class $modify(AIEditorMod, EditorUI) {
    struct Fields {
        AILevelLayer* aiLayer = nullptr;
        bool aiTabActive = false;
    };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        auto winSize = CCDirector::get()->getWinSize();
        float panelW = 200.f;
        float panelH = winSize.height - 80.f;

        auto aiLayer = AILevelLayer::create(this);
        if (!aiLayer) return true;

        aiLayer->setContentSize({panelW, panelH});
        aiLayer->setPosition({winSize.width - panelW - 2, 40});
        aiLayer->setZOrder(10);
        aiLayer->setVisible(false);
        this->addChild(aiLayer);
        m_fields->aiLayer = aiLayer;

        addAITabButton();
        return true;
    }

    void addAITabButton() {
        auto winSize = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({winSize.width - 25, winSize.height - 25});
        this->addChild(menu, 20);

        auto onSpr  = ButtonSprite::create("AI", "bigFont.fnt", "GJ_button_02.png", 0.5f);
        auto offSpr = ButtonSprite::create("AI", "bigFont.fnt", "GJ_button_01.png", 0.5f);
        onSpr->setColor({80, 200, 255});

        auto btn = CCMenuItemToggler::create(
            offSpr, onSpr, this,
            menu_selector(AIEditorMod::onAITabToggle));
        btn->setPosition({0, 0});
        menu->addChild(btn);
    }

    void onAITabToggle(CCObject*) {
        m_fields->aiTabActive = !m_fields->aiTabActive;
        bool show = m_fields->aiTabActive;

        if (m_fields->aiLayer) m_fields->aiLayer->setVisible(show);
        if (m_createButtonBar) m_createButtonBar->setVisible(!show);
        if (m_editButtonBar)   m_editButtonBar->setVisible(!show);
    }
};

$on_mod(Loaded) {
    log::info("AI Level Generator loaded!");
}
