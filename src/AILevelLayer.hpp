#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

class EditorUI;

class AILevelLayer : public CCLayer {
public:
    static AILevelLayer* create(EditorUI* editor);
    bool init(EditorUI* editor);

    void onSend(CCObject* sender);
    void onApply(CCObject* sender);
    void onClear(CCObject* sender);

    void handleResponse(std::string const& body);
    void handleError(std::string const& error);
    void applyObjectsToEditor(matjson::Value const& objects);

private:
    EditorUI*       m_editor    = nullptr;
    CCTextFieldTTF* m_input     = nullptr;
    CCLabelBMFont*  m_status    = nullptr;
    CCMenu*         m_applyMenu = nullptr;
    CCLayer*        m_chatArea  = nullptr;
    float           m_chatScrollY = 0.f;
    std::optional<matjson::Value> m_pendingLevel = std::nullopt;

    void appendChatBubble(std::string const& text, bool isUser);
    void setStatus(std::string const& msg, ccColor3B color = {180,180,220});
    void sendToClaudeAPI(std::string const& prompt);
    std::string buildSystemPrompt() const;
};
