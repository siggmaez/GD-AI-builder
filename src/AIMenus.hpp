#pragma once
#include "AIStyle.hpp"
#include "ChatStore.hpp"
#include "AccountLimits.hpp"
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/utils/general.hpp>

namespace ui {
    inline std::string bridgeURL() {
        auto url = Mod::get()->getSettingValue<std::string>("bridge-url");
        while (!url.empty() && url.back() == '/') url.pop_back();
        return url;
    }
    inline CCMenuItemSpriteExtra* button(CCMenu* menu, char const* text, CCPoint pos,
                                        float width, auto callback, ccColor3B color = kIdle) {
        auto item = CCMenuItemExt::createSpriteExtra(makePill(text, width, 24.f, color, .32f), callback);
        item->setPosition(pos); menu->addChild(item); return item;
    }
    inline CCLabelBMFont* label(CCNode* parent, std::string const& text, CCPoint pos,
                                float scale = .4f, bool unicode = false) {
        auto node = CCLabelBMFont::create(text.c_str(), unicode ? inputFont() : font());
        node->setScale(scale); node->setPosition(pos); node->setColor(kText);
        parent->addChild(node); return node;
    }
}

class AccountPopup : public Popup {
    CCLabelBMFont* m_message = nullptr;
    CCLabelBMFont* m_code = nullptr;
    CCLabelBMFont* m_urlLabel = nullptr;
    std::string m_url, m_deviceCode;
    async::TaskHolder<web::WebResponse> m_request;
    bool m_pending = false;
    bool init() {
        if (!Popup::init(370, 240, "square02b_001.png")) return false;
        m_bgSprite->setColor(ui::kSurface); setTitle("ChatGPT Account", ui::font(), .5f);
        m_message = ui::label(m_mainLayer, "Connect your bridge, then sign in.", {185, 183}, .36f, true);
        m_code = ui::label(m_mainLayer, "", {185, 143}, .7f);
        m_urlLabel = ui::label(m_mainLayer, "Official Codex sign-in. No API keys.", {185, 108}, .3f, true);
        ui::button(m_buttonMenu, "Sign in", {78, 66}, 100, [this](auto){ start(); }, ui::kAccent);
        ui::button(m_buttonMenu, "Open link", {185, 66}, 100, [this](auto){
            if (m_url.starts_with("https://auth.openai.com/")) web::openLinkInBrowser(m_url);
        });
        ui::button(m_buttonMenu, "Copy code", {292, 66}, 100, [this](auto){
            if (!m_deviceCode.empty()) clipboard::write(m_deviceCode);
        });
        ui::label(m_mainLayer, "Keep your phone or PC bridge running.", {185, 30}, .29f, true);
        poll(0); schedule(schedule_selector(AccountPopup::poll), 3.f);
        return true;
    }
    void updateResponse(web::WebResponse response) {
        m_pending = false;
        auto json = response.json();
        if (!response.ok() || !json) {
            m_message->setString("Bridge offline. Check Bridge URL in Settings.");
            m_message->limitLabelWidth(330, .36f, .2f); return;
        }
        auto data = json.unwrap();
        auto message = data["message"].asString().unwrapOr("Sign in with ChatGPT.");
        m_message->setString(message.c_str()); m_message->limitLabelWidth(330, .36f, .2f);
        m_url = data["url"].asString().unwrapOr("");
        if (!m_url.starts_with("https://auth.openai.com/")) m_url.clear();
        m_deviceCode = data["code"].asString().unwrapOr("");
        m_code->setString(m_deviceCode.c_str());
        m_urlLabel->setString(m_url.empty() ? "Official ChatGPT account sign-in" : m_url.c_str());
        m_urlLabel->limitLabelWidth(330, .3f, .2f);
    }
    void poll(float) {
        if (m_pending) return;
        m_pending = true;
        m_request.spawn(web::WebRequest().timeout(std::chrono::seconds(8)).get(ui::bridgeURL()+"/auth/status"),
            [weak=WeakRef(this)](web::WebResponse response){ if (auto self=weak.lock()) self->updateResponse(response); });
    }
    void start() {
        m_pending = true;
        web::WebRequest request;
        request.header("X-AI-Builder", "account-login");
        request.timeout(std::chrono::seconds(8));
        m_request.spawn(request.post(ui::bridgeURL()+"/auth/start"),
            [weak=WeakRef(this)](web::WebResponse response){ if (auto self=weak.lock()) self->updateResponse(response); });
    }
public:
    static AccountPopup* create() {
        auto popup = new AccountPopup;
        if (popup->init()) { popup->autorelease(); return popup; }
        delete popup; return nullptr;
    }
};

class ModelsPopup : public Popup {
    TextInput* m_input = nullptr;
    ScrollLayer* m_list = nullptr;
    async::TaskHolder<web::WebResponse> m_request;
    bool init() {
        if (!Popup::init(350, 260, "square02b_001.png")) return false;
        m_bgSprite->setColor(ui::kSurface); setTitle("Model", ui::font(), .5f);
        m_list = ScrollLayer::create(CCSize{310, 150}); m_list->setPosition({20, 70});
        m_mainLayer->addChild(m_list);
        m_input = TextInput::create(220, "Custom model / empty = default", ui::inputFont());
        m_input->setPosition({135, 40}); m_input->setCommonFilter(CommonFilter::Any);
        m_input->setString(Mod::get()->getSettingValue<std::string>("model"));
        m_mainLayer->addChild(m_input);
        ui::button(m_buttonMenu, "Use", {295, 40}, 65, [this](auto){
            Mod::get()->setSettingValue("model", std::string(m_input->getString())); onClose(nullptr);
        }, ui::kAccent);
        populate({});
        auto cli=Mod::get()->getSettingValue<std::string>("cli");
        m_request.spawn(web::WebRequest().timeout(std::chrono::seconds(5)).get(ui::bridgeURL()+"/models?cli="+cli),
            [weak=WeakRef(this)](web::WebResponse response){
                auto self=weak.lock(); if (!self) return;
                auto json=response.json(); if (!response.ok() || !json) return;
                auto models = json.unwrap()["models"].asArray();
                if (models) self->populate(models.unwrap());
            });
        return true;
    }
    void populate(std::vector<matjson::Value> const& models) {
        auto content=m_list->m_contentLayer; content->removeAllChildren();
        float height=std::max(150.f, (models.size()+1)*30.f);
        content->setContentSize({310,height});
        auto menu=CCMenu::create(); menu->setPosition({0,0}); content->addChild(menu);
        ui::button(menu, "CLI default", {155,height-16}, 290, [this](auto){
            Mod::get()->setSettingValue("model", std::string("")); onClose(nullptr);
        });
        float y=height-46;
        for (auto const& model:models) {
            auto id=model["id"].asString().unwrapOr(""); if (id.empty()) continue;
            auto label=model["name"].asString().unwrapOr(id);
            ui::button(menu, label.c_str(), {155,y}, 290, [this,id](auto){
                Mod::get()->setSettingValue("model", id); onClose(nullptr);
            }); y-=30;
        }
        m_list->scrollToTop();
    }
public:
    static ModelsPopup* create() {
        auto popup=new ModelsPopup;
        if (popup->init()) { popup->autorelease(); return popup; }
        delete popup; return nullptr;
    }
};

class HistoryPopup : public Popup {
    bool init() {
        if (!Popup::init(350,260,"square02b_001.png")) return false;
        m_bgSprite->setColor(ui::kSurface); setTitle("Chat History",ui::font(),.5f);
        auto list=ScrollLayer::create(CCSize{310,170}); list->setPosition({20,55}); m_mainLayer->addChild(list);
        auto& chats=ChatStore::chats();
        float height=std::max(170.f,chats.size()*34.f);
        list->m_contentLayer->setContentSize({310,height});
        auto menu=CCMenu::create(); menu->setPosition({0,0}); list->m_contentLayer->addChild(menu);
        float y=height-18;
        for (auto it=chats.rbegin();it!=chats.rend();++it) {
            auto id=(*it)["id"].asString().unwrapOr("");
            auto title=(*it)["title"].asString().unwrapOr("Chat");
            auto pill=ui::makePill("",252,28,ui::kIdle);
            auto label=ui::label(pill,title,{126,14},.38f,true); label->limitLabelWidth(235,.38f,.2f);
            auto item=CCMenuItemExt::createSpriteExtra(pill,[this,id](auto){ChatStore::select(id); ChatStore::save(); onClose(nullptr);});
            item->setPosition({136,y});menu->addChild(item);
            ui::button(menu,"x",{290,y},24,[weak=WeakRef(this),id](auto){
                createQuickPopup("Delete chat","Delete this chat and its local messages?","Cancel","Delete",
                    [weak,id](auto,bool confirmed){
                        auto self=weak.lock();if(!self||!confirmed)return;
                        ChatStore::remove(id);self->onClose(nullptr);
                    });
            });
            y-=34;
        }
        list->scrollToTop();
        ui::button(m_buttonMenu,"New chat",{175,28},150,[this](auto){ChatStore::newChat();onClose(nullptr);},ui::kAccent);
        return true;
    }
public:
    static HistoryPopup* create() {
        auto popup=new HistoryPopup;
        if (popup->init()) {popup->autorelease();return popup;}
        delete popup;return nullptr;
    }
};

class PhoneBridgePopup : public Popup {
    bool init() {
        if(!Popup::init(320,230,"square02b_001.png"))return false;
        m_bgSprite->setColor(ui::kSurface);setTitle("Phone Bridge",ui::font(),.5f);
        auto help=CCLabelBMFont::create("Install the companion app once.\nStart it and sign in, then return to GD.\nNo PC or Termux is needed.",ui::inputFont(),680,kCCTextAlignmentCenter);
        help->setScale(.4f);help->setPosition({160,155});m_mainLayer->addChild(help);
        ui::button(m_buttonMenu,"Download Android app",{160,96},270,[](auto){
            web::openLinkInBrowser("https://github.com/siggmaez/GD-AI-builder/releases/latest/download/AI-Builder-Phone-Bridge.apk");
        });
        ui::button(m_buttonMenu,"Open / Start bridge",{160,60},270,[](auto){
#ifdef GEODE_IS_ANDROID
            Mod::get()->setSettingValue("bridge-url",std::string("http://127.0.0.1:8765"));
            web::openLinkInBrowser("aibuilder://bridge/start");
#else
            FLAlertLayer::create("Android app","Install the companion APK on your Android phone, then open Phone Bridge in the mod on that phone.","OK")->show();
#endif
        },ui::kAccent);
        ui::label(m_mainLayer,"Local URL: http://127.0.0.1:8765",{160,25},.27f,true);
        return true;
    }
public:
    static PhoneBridgePopup* create(){auto p=new PhoneBridgePopup;if(p->init()){p->autorelease();return p;}delete p;return nullptr;}
};

class AIToolsPopup : public Popup {
    bool init() {
        if (!Popup::init(280,260,"square02b_001.png")) return false;
        m_bgSprite->setColor(ui::kSurface);setTitle("AI Builder",ui::font(),.5f);
        ui::button(m_buttonMenu,"Model",{140,204},220,[](auto){if(auto p=ModelsPopup::create())p->show();});
        ui::button(m_buttonMenu,"Chat history",{140,168},220,[](auto){if(auto p=HistoryPopup::create())p->show();});
        ui::button(m_buttonMenu,"Codex limits",{140,132},220,[](auto){if(auto p=AccountLimits::create())p->show();});
        ui::button(m_buttonMenu,"ChatGPT account",{140,96},220,[](auto){if(auto p=AccountPopup::create())p->show();});
        ui::button(m_buttonMenu,"Settings",{140,60},220,[](auto){geode::openSettingsPopup(Mod::get());});
        ui::button(m_buttonMenu,"Phone bridge",{140,24},220,[](auto){if(auto p=PhoneBridgePopup::create())p->show();});
        return true;
    }
public:
    static AIToolsPopup* create() {
        auto p=new AIToolsPopup;
        if(p->init()){p->autorelease();return p;}
        delete p;return nullptr;
    }
};
