#include "AILevelLayer.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/binding/EditorUI.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>

using namespace geode::prelude;

struct ObjTemplate { std::string name; int objId; };
static const std::vector<ObjTemplate> kObjMap = {
    {"block", 1}, {"spike", 8}, {"orb", 36}, {"pad", 35},
    {"portal_cube", 12}, {"portal_ship", 13}, {"portal_ball", 47},
    {"portal_wave", 660}, {"coin", 1329}, {"ring", 6},
    {"sawblade", 1706}, {"platform", 1916}, {"dash_orb", 1751},
};

static ObjTemplate findObj(std::string const& name) {
    std::string low = name;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    for (auto const& t : kObjMap)
        if (low.find(t.name) != std::string::npos) return t;
    return kObjMap[0];
}

std::string AILevelLayer::buildSystemPrompt() const {
    return R"(You are an AI plugin inside the Geometry Dash level editor.
Respond ONLY with a raw JSON object, no markdown, no extra text.
Format:
{
  "name": "Level name",
  "description": "Short description",
  "objects": [
    {"type": "block|spike|orb|pad|ring|portal_cube|portal_ship|portal_ball|portal_wave|coin|sawblade|platform|dash_orb", "x": 75, "y": 75, "rotation": 0, "scale": 1.0}
  ]
}
Rules: x is 0-1500, y is 75 (ground) to 450, grid = 30 units. Place 15-25 objects. Match difficulty to description.)";
}

AILevelLayer* AILevelLayer::create(EditorUI* editor) {
    auto ret = new AILevelLayer();
    if (ret && ret->init(editor)) { ret->autorelease(); return ret; }
    delete ret; return nullptr;
}

bool AILevelLayer::init(EditorUI* editor) {
    if (!CCLayer::init()) return false;
    m_editor = editor;

    auto winSize = CCDirector::get()->getWinSize();
    float w = 200.f;
    float h = winSize.height - 80.f;

    auto bg = CCLayerColor::create({10, 10, 28, 240});
    bg->setContentSize({w, h});
    this->addChild(bg);

    auto title = CCLabelBMFont::create("AI LEVEL GEN", "bigFont.fnt");
    title->setScale(0.4f);
    title->setColor({80, 200, 255});
    title->setPosition({w / 2, h - 15});
    this->addChild(title);

    m_chatArea = CCLayer::create();
    m_chatArea->setContentSize({w, h - 100});
    m_chatArea->setPosition({0, 60});
    this->addChild(m_chatArea);
    m_chatScrollY = h - 110;

    m_status = CCLabelBMFont::create("Type a prompt below!", "chatFont.fnt");
    m_status->setScale(0.4f);
    m_status->setColor({150, 150, 200});
    m_status->setAnchorPoint({0, 0.5f});
    m_status->setPosition({5, 52});
    this->addChild(m_status);

    auto inputBg = CCLayerColor::create({20, 20, 50, 255});
    inputBg->setContentSize({w - 50, 24});
    inputBg->setPosition({2, 26});
    this->addChild(inputBg);

    m_input = CCTextFieldTTF::textFieldWithPlaceHolder(
        "Describe level...", "Arial", 11);
    m_input->setColor({220, 220, 255});
    m_input->setAnchorPoint({0, 0.5f});
    m_input->setPosition({5, 38});
    m_input->setWidth(w - 55);
    this->addChild(m_input);

    auto sendMenu = CCMenu::create();
    sendMenu->setPosition({0, 0});
    this->addChild(sendMenu);

    auto sendBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Go", "bigFont.fnt", "GJ_button_01.png", 0.45f),
        this, menu_selector(AILevelLayer::onSend));
    sendBtn->setPosition({w - 20, 38});
    sendMenu->addChild(sendBtn);

    m_applyMenu = CCMenu::create();
    m_applyMenu->setPosition({0, 0});
    m_applyMenu->setVisible(false);
    this->addChild(m_applyMenu);

    auto applyBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Apply!", "bigFont.fnt", "GJ_button_02.png", 0.45f),
        this, menu_selector(AILevelLayer::onApply));
    applyBtn->setPosition({w / 2, 12});
    m_applyMenu->addChild(applyBtn);

    appendChatBubble("Hi! Describe a level section and I'll generate it.", false);
    return true;
}

void AILevelLayer::appendChatBubble(std::string const& text, bool isUser) {
    float w = 190.f;
    auto lbl = CCLabelBMFont::create(isUser ? "You:" : "Claude:", "chatFont.fnt");
    lbl->setScale(0.4f);
    lbl->setColor(isUser ? ccColor3B{255,200,50} : ccColor3B{80,200,255});
    lbl->setAnchorPoint({0, 1});
    lbl->setPosition({5, m_chatScrollY});
    m_chatArea->addChild(lbl);
    m_chatScrollY -= 12;

    auto msg = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
    msg->setScale(0.38f);
    msg->setColor(isUser ? ccColor3B{230,230,200} : ccColor3B{200,225,255});
    msg->setAnchorPoint({0, 1});
    msg->setWidth(w / 0.38f);
    msg->setPosition({8, m_chatScrollY});
    m_chatArea->addChild(msg);
    m_chatScrollY -= msg->getContentSize().height * 0.38f + 8;
}

void AILevelLayer::setStatus(std::string const& msg, ccColor3B color) {
    m_status->setString(msg.c_str());
    m_status->setColor(color);
}

void AILevelLayer::onSend(CCObject*) {
    std::string prompt = m_input->getString();
    if (prompt.empty()) return;
    m_input->setString("");
    appendChatBubble(prompt, true);
    setStatus("Asking Claude...", {100, 200, 255});
    m_applyMenu->setVisible(false);
    sendToClaudeAPI(prompt);
}

void AILevelLayer::sendToClaudeAPI(std::string const& prompt) {
    auto apiKey = Mod::get()->getSettingValue<std::string>("api-key");
    auto model  = Mod::get()->getSettingValue<std::string>("model");

    if (apiKey.empty()) {
        handleError("No API key! Set it in Mod Settings.");
        return;
    }

    matjson::Value body = matjson::Object{
        {"model", model},
        {"max_tokens", 2000},
        {"system", buildSystemPrompt()},
        {"messages", matjson::Array{
            matjson::Object{{"role","user"},{"content", prompt}}
        }}
    };

    web::AsyncWebRequest()
        .userAgent("GD-AI-Mod/1.0")
        .header("Content-Type", "application/json")
        .header("x-api-key", apiKey)
        .header("anthropic-version", "2023-06-01")
        .bodyRaw(body.dump())
        .post("https://api.anthropic.com/v1/messages")
        .text()
        .then([this](std::string const& res) {
            Loader::get()->queueInMainThread([this, res]{ handleResponse(res); });
        })
        .expect([this](std::string const& err) {
            Loader::get()->queueInMainThread([this, err]{ handleError(err); });
        });
}

void AILevelLayer::handleResponse(std::string const& body) {
    auto outer = matjson::parse(body);
    if (!outer) { handleError("Parse failed."); return; }

    if (outer->contains("error")) {
        handleError(outer.value()["error"]["message"].as<std::string>().unwrapOr("API Error"));
        return;
    }

    std::string text;
    for (auto& block : outer.value()["content"].asArray().unwrapOr({})) {
        if (block["type"].as<std::string>().unwrapOr("") == "text") {
            text = block["text"].as<std::string>().unwrapOr(""); break;
        }
    }

    auto clean = text;
    auto s = clean.find('{'); auto e = clean.rfind('}');
    if (s != std::string::npos && e != std::string::npos)
        clean = clean.substr(s, e - s + 1);

    auto level = matjson::parse(clean);
    if (!level) { appendChatBubble(text, false); return; }

    m_pendingLevel = level.value();
    std::string name = m_pendingLevel.value()["name"].as<std::string>().unwrapOr("Level");
    int count = (int)m_pendingLevel.value()["objects"].asArray().unwrapOr({}).size();
    appendChatBubble("Generated: " + name + " (" + std::to_string(count) + " objects)\nTap Apply!", false);
    setStatus("Ready! Tap Apply.", {50, 255, 120});
    m_applyMenu->setVisible(true);
}

void AILevelLayer::handleError(std::string const& error) {
    appendChatBubble("Error: " + error, false);
    setStatus("Error.", {255, 80, 80});
}

void AILevelLayer::onApply(CCObject*) {
    if (!m_pendingLevel || !m_editor) return;
    applyObjectsToEditor(m_pendingLevel.value());
}

void AILevelLayer::applyObjectsToEditor(matjson::Value const& level) {
    auto editorLayer = m_editor->m_editorLayer;
    if (!editorLayer) return;

    for (auto const& obj : level["objects"].asArray().unwrapOr({})) {
        std::string type = obj["type"].as<std::string>().unwrapOr("block");
        int x = (int)obj["x"].as<double>().unwrapOr(75);
        int y = (int)obj["y"].as<double>().unwrapOr(75);
        int rot = (int)obj["rotation"].as<double>().unwrapOr(0);
        float scale = (float)obj["scale"].as<double>().unwrapOr(1.0);

        auto tmpl = findObj(type);
        std::string s = "1," + std::to_string(tmpl.objId) +
            ",2," + std::to_string(x) +
            ",3," + std::to_string(y) +
            ",6," + std::to_string(rot) +
            ",32," + std::to_string(scale) +
            ",33," + std::to_string(scale) + ";";
        editorLayer->createObjectsFromString(s, true, true);
    }

    m_editor->deselectAll();
    setStatus("Done! Objects placed.", {50, 255, 120});
    m_applyMenu->setVisible(false);
    m_pendingLevel = std::nullopt;
}

void AILevelLayer::onClear(CCObject*) {
    m_pendingLevel = std::nullopt;
    m_applyMenu->setVisible(false);
    setStatus("Cleared.", {150, 150, 200});
}
