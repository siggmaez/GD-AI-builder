#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "AIPanel.hpp"

using namespace geode::prelude;

class $modify(AIEditorUI, EditorUI) {
	bool init(LevelEditorLayer* editor) {
		if (!EditorUI::init(editor)) return false;

		auto win = CCDirector::get()->getWinSize();

		auto menu = CCMenu::create();
		menu->setID("ai-builder-menu"_spr);
		menu->setPosition({0, 0});

		// Round minimal floating button, left edge, above the middle
		auto pill = ui::makeRoundButton("AI", 34.f, ui::kAccent);
		ui::recolor(pill, ui::kAccent, ui::kSurface);
		auto btn = CCMenuItemSpriteExtra::create(pill, this, menu_selector(AIEditorUI::onAI));
		btn->setPosition({26.f, win.height / 2 + 40.f});
		menu->addChild(btn);

		this->addChild(menu, 100);
		return true;
	}

	void onAI(CCObject*) {
		AIPanel::create(m_editorLayer)->show();
	}
};
