#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <ctime>

using namespace geode::prelude;

class AccountLimits : public Popup {
    CCLabelBMFont* m_text = nullptr;
    async::TaskHolder<web::WebResponse> m_request;

    bool init() {
        if (!Popup::init(330.f, 220.f)) return false;
        setTitle("ChatGPT Limits", "bigFont.fnt", .5f);
        m_bgSprite->setColor({20, 22, 27});
        m_text = CCLabelBMFont::create("Checking account limits...", "bigFont.fnt", 540.f,
                                      kCCTextAlignmentCenter);
        m_text->setScale(.5f);
        m_text->setPosition({165.f, 118.f});
        m_mainLayer->addChild(m_text);
        auto button = CCMenuItemSpriteExtra::create(ButtonSprite::create("Refresh"), this,
                                                   menu_selector(AccountLimits::refresh));
        button->setPosition({165.f, 34.f});
        m_buttonMenu->addChild(button);
        refresh(nullptr);
        return true;
    }

    void refresh(CCObject*) {
        m_text->setString("Checking account limits...");
        auto url = Mod::get()->getSettingValue<std::string>("bridge-url");
        while (!url.empty() && url.back() == '/') url.pop_back();
        m_request.spawn(web::WebRequest().timeout(std::chrono::seconds(30)).get(url + "/limits"),
            [weak = WeakRef(this)](web::WebResponse response) {
                auto self = weak.lock();
                if (!self) return;
                auto json = response.json();
                std::string text = "Limits unavailable\nCheck bridge and ChatGPT sign-in.";
                if (response.ok() && json && json.unwrap()["available"].asBool().unwrapOr(false)) {
                    text.clear();
                    auto windows = json.unwrap()["windows"].asArray();
                    if (windows) for (auto const& window : windows.unwrap()) {
                        auto minutes = window["window_minutes"].asInt().unwrapOr(0);
                        auto remaining = window["remaining_percent"].asDouble();
                        if (!remaining) continue;
                        auto label = minutes == 300 ? "5 hours" : minutes == 10080 ? "Weekly" :
                            std::to_string(minutes) + " min";
                        text += fmt::format("{}: {:.1f}% remaining\n", label, remaining.unwrap());
                        auto reset = static_cast<std::time_t>(window["resets_at"].asInt().unwrapOr(0));
                        if (reset > 0) {
                            char date[64] = {};
                            // localtime is used only on the game thread; the bridge sends Unix time.
                            if (auto local = std::localtime(&reset)) {
                                std::strftime(date, sizeof(date), "%d %b %H:%M", local);
                                text += std::string("Resets: ") + date + "\n";
                            }
                        }
                        text += "\n";
                    }
                    text += "Account-wide usage. Updated every 60s.";
                }
                self->m_text->setString(text.c_str());
                self->m_text->setScale(std::min(.45f, 130.f / std::max(1.f, self->m_text->getContentHeight())));
            });
    }

public:
    static AccountLimits* create() {
        auto popup = new AccountLimits();
        if (popup->init()) { popup->autorelease(); return popup; }
        delete popup;
        return nullptr;
    }
};
