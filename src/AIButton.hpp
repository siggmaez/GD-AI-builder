#pragma once
#include "AIPanel.hpp"

// Own the gesture so dragging never activates the editor or opens the popup.
class AIButton : public CCLayer {
    LevelEditorLayer* m_editor = nullptr;
    CCNode* m_visual = nullptr;
    CCTouch* m_touch = nullptr;
    CCPoint m_startTouch;
    CCPoint m_startPosition;
    bool m_dragging = false;

    CCPoint bounded(CCPoint p) const {
        auto size = CCDirector::get()->getWinSize();
        return {std::clamp(p.x, 24.f, std::max(24.f, size.width - 24.f)),
                std::clamp(p.y, 24.f, std::max(24.f, size.height - 24.f))};
    }

    bool init(LevelEditorLayer* editor) {
        if (!CCLayer::init()) return false;
        m_editor = editor;
        setID("ai-builder-button"_spr);
        setContentSize({44.f, 44.f});
        ignoreAnchorPointForPosition(false);
        setAnchorPoint({0.5f, 0.5f});
        auto win = CCDirector::get()->getWinSize();
        float x = Mod::get()->getSavedValue<double>("button-x", 28.f / win.width);
        float y = Mod::get()->getSavedValue<double>("button-y", 0.5f + 40.f / win.height);
        setPosition(bounded({std::isfinite(x) ? x * win.width : 28.f,
                             std::isfinite(y) ? y * win.height : win.height / 2}));
        m_visual = ui::makeRoundButton("AI", 38.f, ui::kAccent);
        ui::recolor(m_visual, ui::kAccent, ui::kSurface);
        m_visual->setPosition({22.f, 22.f});
        addChild(m_visual);
        setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -300, true);
    }

    bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
        if (m_touch || !isVisible() || !getParent() ||
            !CCRect{0.f, 0.f, 44.f, 44.f}.containsPoint(convertToNodeSpace(touch->getLocation()))) return false;
        m_touch = touch;
        m_startTouch = getParent()->convertToNodeSpace(touch->getLocation());
        m_startPosition = getPosition();
        m_dragging = false;
        m_visual->setScale(0.94f);
        return true;
    }

    void ccTouchMoved(CCTouch* touch, CCEvent*) override {
        if (touch != m_touch) return;
        auto delta = getParent()->convertToNodeSpace(touch->getLocation()) - m_startTouch;
        if (delta.x * delta.x + delta.y * delta.y > 36.f) m_dragging = true;
        if (m_dragging) setPosition(bounded(m_startPosition + delta));
    }

    void ccTouchEnded(CCTouch* touch, CCEvent*) override {
        if (touch != m_touch) return;
        m_touch = nullptr;
        m_visual->setScale(1.f);
        if (m_dragging) {
            auto win = CCDirector::get()->getWinSize();
            Mod::get()->setSavedValue("button-x", static_cast<double>(getPositionX() / win.width));
            Mod::get()->setSavedValue("button-y", static_cast<double>(getPositionY() / win.height));
            (void)Mod::get()->saveData();
        }
        else if (CCRect{0.f, 0.f, 44.f, 44.f}.containsPoint(convertToNodeSpace(touch->getLocation()))) {
            if (auto panel = AIPanel::create(m_editor)) panel->show();
        }
    }

    void ccTouchCancelled(CCTouch* touch, CCEvent*) override {
        if (touch != m_touch) return;
        m_touch = nullptr;
        m_visual->setScale(1.f);
        if (m_dragging) setPosition(m_startPosition);
        m_dragging = false;
    }

public:
    static AIButton* create(LevelEditorLayer* editor) {
        auto ret = new AIButton;
        if (ret->init(editor)) { ret->autorelease(); return ret; }
        delete ret;
        return nullptr;
    }
};
