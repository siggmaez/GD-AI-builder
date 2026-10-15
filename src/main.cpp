#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "AIButton.hpp"

using namespace geode::prelude;

class $modify(AIEditorUI, EditorUI) {
	bool init(LevelEditorLayer* editor) {
		if (!EditorUI::init(editor)) return false;

		if (auto button = AIButton::create(editor)) this->addChild(button, 100);
		return true;
	}
};
