#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "AIButton.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    ButtonSettingPressedEventV3(Mod::get(), "account-actions").listen([](auto key) {
        if (key == "sign-in") { if (auto popup = AccountPopup::create()) popup->show(); }
        else if (key == "limits") { if (auto popup = AccountLimits::create()) popup->show(); }
    }).leak();
}

class $modify(AIEditorUI, EditorUI) {
	bool init(LevelEditorLayer* editor) {
		if (!EditorUI::init(editor)) return false;

		if (auto button = AIButton::create(editor)) this->addChild(button, 100);
		return true;
	}
};
