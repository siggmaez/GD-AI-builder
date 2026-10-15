#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

using namespace geode::prelude;

namespace ui {
	// Palette: near-black surface, soft indigo accent
	constexpr ccColor3B kSurface = {20, 22, 27};
	constexpr ccColor3B kIdle    = {52, 56, 68};
	constexpr ccColor3B kAccent  = {124, 156, 255};
	constexpr ccColor3B kText    = {235, 238, 245};
	constexpr ccColor3B kMuted   = {140, 146, 160};
	constexpr ccColor3B kBad     = {255, 120, 120};
	constexpr ccColor3B kGood    = {130, 220, 160};

	// Rounded pill: tag 1 = background, tag 2 = label
	inline CCNode* makePill(char const* text, float w, float h, ccColor3B col,
	                        float labelScale = 0.5f) {
		auto node = CCNode::create();
		node->setContentSize({w, h});
		node->setAnchorPoint({0.5f, 0.5f});

		auto bg = NineSlice::create("square02_001.png");
		bg->setContentSize({w, h});
		bg->setColor(col);
		bg->setPosition({w / 2, h / 2});
		bg->setTag(1);
		node->addChild(bg);

		auto lbl = CCLabelBMFont::create(text, "bigFont.fnt");
		lbl->setScale(labelScale);
		lbl->setPosition({w / 2, h / 2});
		lbl->setTag(2);
		node->addChild(lbl);
		return node;
	}

	inline void recolor(CCNode* pill, ccColor3B col, ccColor3B textCol = kText) {
		if (auto bg = typeinfo_cast<NineSlice*>(pill->getChildByTag(1))) bg->setColor(col);
		if (auto lb = typeinfo_cast<CCLabelBMFont*>(pill->getChildByTag(2))) lb->setColor(textCol);
	}

	inline CCNode* makeRoundButton(char const* text, float d, ccColor3B col) {
		return makePill(text, d, d, col, 0.45f);
	}
}

class AIPanel : public Popup {
protected:
	Ref<LevelEditorLayer> m_editor;
	TextInput* m_input = nullptr;
	CCLabelBMFont* m_status = nullptr;
	CCNode* m_pillGame = nullptr;
	CCNode* m_pillDecor = nullptr;
	CCMenuItemSpriteExtra* m_genBtn = nullptr;
	bool m_decor = false;
	bool m_busy = false;

	bool init(LevelEditorLayer* editor) {
		if (!Popup::init(340.f, 210.f, "square02_001.png")) return false;
		m_editor = editor;

		m_bgSprite->setColor(ui::kSurface);
		m_bgSprite->setOpacity(240);
		m_closeBtn->setVisible(false);

		float W = m_size.width, H = m_size.height;

		// All content lives in one root node (the popup layout would otherwise re-center children)
		auto root = CCNode::create();
		root->setContentSize(m_size);
		root->setAnchorPoint({0.5f, 0.5f});
		root->setPosition(m_size / 2);
		m_mainLayer->addChild(root);

		auto title = CCLabelBMFont::create("AI Builder", "bigFont.fnt");
		title->setScale(0.5f);
		title->setColor(ui::kText);
		title->setAnchorPoint({0.f, 0.5f});
		title->setPosition({20.f, H - 24.f});
		root->addChild(title);

		auto menu = CCMenu::create();
		menu->setPosition({0, 0});
		menu->setContentSize(m_size);
		root->addChild(menu);

		auto closeBtn = CCMenuItemSpriteExtra::create(
			ui::makePill("x", 26.f, 26.f, ui::kIdle, 0.5f), this, menu_selector(AIPanel::onClose));
		closeBtn->setPosition({W - 22.f, H - 24.f});
		menu->addChild(closeBtn);

		m_pillGame = ui::makePill("Gameplay", 120.f, 28.f, ui::kAccent, 0.42f);
		m_pillDecor = ui::makePill("Decor", 120.f, 28.f, ui::kIdle, 0.42f);
		auto bGame = CCMenuItemSpriteExtra::create(m_pillGame, this, menu_selector(AIPanel::onMode));
		bGame->setTag(0);
		bGame->setPosition({W / 2 - 66.f, H - 62.f});
		auto bDecor = CCMenuItemSpriteExtra::create(m_pillDecor, this, menu_selector(AIPanel::onMode));
		bDecor->setTag(1);
		bDecor->setPosition({W / 2 + 66.f, H - 62.f});
		menu->addChild(bGame);
		menu->addChild(bDecor);
		this->refreshMode();

		m_input = TextInput::create(W - 40.f, "Describe what to build...", "chatFont.fnt");
		m_input->setPosition({W / 2, H / 2 - 12.f});
		m_input->setMaxCharCount(300);
		root->addChild(m_input);

		m_status = CCLabelBMFont::create("Ready", "chatFont.fnt");
		m_status->setScale(0.6f);
		m_status->setColor(ui::kMuted);
		m_status->setPosition({W / 2, 52.f});
		root->addChild(m_status);

		m_genBtn = CCMenuItemSpriteExtra::create(
			ui::makePill("Generate", 160.f, 32.f, ui::kAccent, 0.5f), this, menu_selector(AIPanel::onGenerate));
		m_genBtn->setPosition({W / 2, 26.f});
		menu->addChild(m_genBtn);
		ui::recolor(m_genBtn->getNormalImage(), ui::kAccent, ui::kSurface);

		return true;
	}

	void refreshMode() {
		ui::recolor(m_pillGame, m_decor ? ui::kIdle : ui::kAccent, m_decor ? ui::kMuted : ui::kSurface);
		ui::recolor(m_pillDecor, m_decor ? ui::kAccent : ui::kIdle, m_decor ? ui::kSurface : ui::kMuted);
	}

	void onMode(CCObject* sender) {
		m_decor = static_cast<CCNode*>(sender)->getTag() == 1;
		this->refreshMode();
	}

	void setStatus(std::string const& s, ccColor3B col) {
		m_status->setString(s.c_str());
		m_status->setColor(col);
	}

	void setBusy(bool b) {
		m_busy = b;
		m_genBtn->setEnabled(!b);
		ui::recolor(m_genBtn->getNormalImage(), b ? ui::kIdle : ui::kAccent, b ? ui::kMuted : ui::kSurface);
	}

	void onGenerate(CCObject*) {
		if (m_busy) return;
		std::string prompt = m_input->getString();
		if (prompt.empty()) return this->setStatus("Write a prompt first", ui::kBad);

		float maxX = 0.f;
		for (auto obj : CCArrayExt<GameObject*>(m_editor->m_objects)) {
			maxX = std::max(maxX, obj->getPositionX());
		}

		matjson::Value body;
		body["mode"] = std::string(m_decor ? "decor" : "gameplay");
		body["prompt"] = prompt;
		body["start_x"] = static_cast<double>(maxX);

		this->setBusy(true);
		this->setStatus("Thinking... keep this panel open", ui::kMuted);

		auto url = Mod::get()->getSettingValue<std::string>("bridge-url") + "/generate";
		web::WebRequest req;
		req.bodyJSON(body);
		req.timeout(std::chrono::seconds(180));

		async::spawn(req.post(url), [self = Ref(this)](web::WebResponse res) {
			if (!res.ok()) {
				self->setBusy(false);
				return self->setStatus("Bridge error " + std::to_string(res.code()), ui::kBad);
			}
			auto json = res.json();
			if (!json) {
				self->setBusy(false);
				return self->setStatus("Bad JSON from bridge", ui::kBad);
			}
			self->applyObjects(json.unwrap());
		});
	}

	void applyObjects(matjson::Value const& json) {
		this->setBusy(false);
		auto arr = json["objects"].asArray();
		if (!arr) return this->setStatus("Bad response from bridge", ui::kBad);

		int count = 0;
		for (auto const& o : arr.unwrap()) {
			int id = o["id"].asInt().unwrapOr(0);
			if (id <= 0) continue;
			float x = static_cast<float>(o["x"].asDouble().unwrapOr(0.0));
			float y = static_cast<float>(o["y"].asDouble().unwrapOr(0.0));
			float rot = static_cast<float>(o["rot"].asDouble().unwrapOr(0.0));
			float scale = static_cast<float>(o["scale"].asDouble().unwrapOr(1.0));

			auto obj = m_editor->createObject(id, {x, y}, true);
			if (!obj) continue;
			if (rot != 0.f) obj->setRotation(rot);
			if (scale != 1.f) {
				obj->updateCustomScaleX(scale);
				obj->updateCustomScaleY(scale);
			}
			count++;
		}
		this->setStatus("Added " + std::to_string(count) + " objects", ui::kGood);
	}

public:
	static AIPanel* create(LevelEditorLayer* editor) {
		auto ret = new AIPanel();
		if (ret->init(editor)) {
			ret->autorelease();
			return ret;
		}
		delete ret;
		return nullptr;
	}
};
