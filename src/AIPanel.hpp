#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_set>

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
		auto node = makePill(text, d, d, col, 0.45f);
		auto old = node->getChildByTag(1);
		old->removeFromParent();
		auto circle = CCSprite::create("GJ_button_01.png");
		circle->setScale(d / circle->getContentSize().width);
		circle->setPosition({d / 2, d / 2});
		circle->setColor(col);
		node->addChild(circle, -1, 1);
		return node;
	}
}

class AIPanel : public Popup {
protected:
	Ref<LevelEditorLayer> m_editor;
	TextInput* m_input = nullptr;
	CCLabelBMFont* m_status = nullptr;
	CCLabelBMFont* m_healthLabel = nullptr;
	CCLabelTTF* m_answer = nullptr;
	CCMenuItemSpriteExtra* m_askBtn = nullptr;
	int m_elapsed = 0;
	bool m_requestChat = false;
	async::TaskHolder<web::WebResponse> m_request;
	async::TaskHolder<web::WebResponse> m_healthRequest;
	std::unordered_set<int> m_allowed;
	CCRect m_bounds;
	int m_limit = 300;
	bool m_requestDecor = false;
	CCNode* m_pillGame = nullptr;
	CCNode* m_pillDecor = nullptr;
	CCMenuItemSpriteExtra* m_genBtn = nullptr;
	bool m_decor = false;
	bool m_busy = false;

	bool init(LevelEditorLayer* editor) {
		if (!Popup::init(400.f, 300.f, "square02_001.png")) return false;
		m_editor = editor;

		m_bgSprite->setColor(ui::kSurface);
		m_bgSprite->setOpacity(static_cast<GLubyte>(std::clamp<int64_t>(
			Mod::get()->getSettingValue<int64_t>("panel-opacity"), 80, 255)));
		m_closeBtn->setVisible(false);
		auto win = CCDirector::get()->getWinSize();
		m_mainLayer->setScale(std::min({1.f, (win.width - 16.f) / m_size.width,
			(win.height - 16.f) / m_size.height}));

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
		auto settings = CCMenuItemSpriteExtra::create(ui::makePill("Settings", 72.f, 22.f,
			ui::kIdle, 0.32f), this, menu_selector(AIPanel::onSettings));
		settings->setPosition({W - 82.f, H - 24.f});
		menu->addChild(settings);

		m_pillGame = ui::makePill("Gameplay", 170.f, 30.f, ui::kAccent, 0.42f);
		m_pillDecor = ui::makePill("Decor", 170.f, 30.f, ui::kIdle, 0.42f);
		auto bGame = CCMenuItemSpriteExtra::create(m_pillGame, this, menu_selector(AIPanel::onMode));
		bGame->setTag(0);
		bGame->setPosition({W / 2 - 90.f, H - 82.f});
		auto bDecor = CCMenuItemSpriteExtra::create(m_pillDecor, this, menu_selector(AIPanel::onMode));
		bDecor->setTag(1);
		bDecor->setPosition({W / 2 + 90.f, H - 82.f});
		menu->addChild(bGame);
		menu->addChild(bDecor);
		this->refreshMode();

		m_input = TextInput::create(W - 40.f, "Describe what to build...", "chatFont.fnt");
		m_input->setPosition({W / 2, 170.f});
		m_input->setMaxCharCount(300);
		m_input->setString(Mod::get()->getSavedValue<std::string>("draft", ""));
		root->addChild(m_input);
		auto answerBg = NineSlice::create("square02_001.png");
		answerBg->setContentSize({W - 40.f, 90.f});
		answerBg->setPosition({W / 2, 102.f});
		answerBg->setColor({12, 14, 19});
		root->addChild(answerBg);
		m_answer = CCLabelTTF::create("Ask AI a question, or describe a section and press Generate.",
			"Arial", 12.f, {W - 60.f, 72.f}, kCCTextAlignmentLeft, kCCVerticalTextAlignmentTop);
		m_answer->setColor(ui::kText);
		m_answer->setAnchorPoint({0.f, 1.f});
		m_answer->setPosition({30.f, 138.f});
		root->addChild(m_answer);

		m_status = CCLabelBMFont::create("Ready", "chatFont.fnt");
		m_status->setScale(0.6f);
		m_status->setColor(ui::kMuted);
		m_status->setPosition({W / 2, 45.f});
		root->addChild(m_status);
		m_healthLabel = CCLabelBMFont::create("Bridge checking...", "chatFont.fnt");
		m_healthLabel->setScale(0.45f);
		m_healthLabel->setColor(ui::kMuted);
		m_healthLabel->setAnchorPoint({0.f, 0.5f});
		m_healthLabel->setPosition({22.f, H - 48.f});
		root->addChild(m_healthLabel);
		auto refresh = CCMenuItemSpriteExtra::create(ui::makePill("Reconnect", 86.f, 20.f,
			ui::kIdle, 0.29f), this, menu_selector(AIPanel::onReconnect));
		refresh->setPosition({W - 62.f, H - 48.f});
		menu->addChild(refresh);
		m_askBtn = CCMenuItemSpriteExtra::create(ui::makePill("Ask AI", 156.f, 30.f,
			ui::kIdle, 0.42f), this, menu_selector(AIPanel::onAsk));
		m_askBtn->setPosition({W / 2 - 86.f, 23.f});
		menu->addChild(m_askBtn);

		m_genBtn = CCMenuItemSpriteExtra::create(
			ui::makePill("Generate", 156.f, 30.f, ui::kAccent, 0.42f), this, menu_selector(AIPanel::onGenerate));
		m_genBtn->setPosition({W / 2 + 86.f, 23.f});
		menu->addChild(m_genBtn);
		ui::recolor(m_genBtn->getNormalImage(), ui::kAccent, ui::kSurface);

		checkHealth();
		return true;
	}

	void refreshMode() {
		ui::recolor(m_pillGame, m_decor ? ui::kIdle : ui::kAccent, m_decor ? ui::kMuted : ui::kSurface);
		ui::recolor(m_pillDecor, m_decor ? ui::kAccent : ui::kIdle, m_decor ? ui::kSurface : ui::kMuted);
	}

	void onMode(CCObject* sender) {
		if (m_busy) return;
		m_decor = static_cast<CCNode*>(sender)->getTag() == 1;
		this->refreshMode();
	}

	void setStatus(std::string const& s, ccColor3B col) {
		m_status->setString(s.c_str());
		m_status->setColor(col);
		m_status->limitLabelWidth(m_size.width - 32.f, 0.45f, 0.3f);
	}

	void setAnswer(std::string const& text) {
		m_answer->setString(text.c_str());
	}

	void tick(float) {
		setStatus(std::string(m_requestChat ? "Asking AI... " : "Generating... ") +
			std::to_string(++m_elapsed) + "s / 180s", ui::kMuted);
	}

	void setBusy(bool b) {
		m_busy = b;
		m_input->setEnabled(!b);
		m_askBtn->setEnabled(!b);
		if (b) {
			m_elapsed = 0;
			schedule(schedule_selector(AIPanel::tick), 1.f);
		}
		else unschedule(schedule_selector(AIPanel::tick));
		static_cast<CCLabelBMFont*>(m_genBtn->getNormalImage()->getChildByTag(2))->setString(b ? "Cancel" : "Generate");
		ui::recolor(m_genBtn->getNormalImage(), b ? ui::kIdle : ui::kAccent, b ? ui::kMuted : ui::kSurface);
	}

	std::string bridgeURL() const {
		auto url = Mod::get()->getSettingValue<std::string>("bridge-url");
		while (!url.empty() && url.back() == '/') url.pop_back();
		return url;
	}

	void checkHealth() {
		m_healthLabel->setString("Bridge checking...");
		m_healthRequest.spawn(web::WebRequest().timeout(std::chrono::seconds(4)).get(bridgeURL() + "/health"),
			[weak = WeakRef(this)](web::WebResponse res) {
				auto self = weak.lock();
				if (!self) return;
				auto json = res.json();
				bool online = res.ok() && json && json.unwrap()["status"].asString().unwrapOr("") == "ok";
				self->m_healthLabel->setString(online ? "Bridge online" : "Bridge offline");
				self->m_healthLabel->setColor(online ? ui::kGood : ui::kBad);
				if (!online && !self->m_busy) self->setAnswer("Bridge offline. Start bridge.py, then press Reconnect. Check the Bridge URL in Settings.");
			});
	}

	void onClose(CCObject* sender) override {
		Mod::get()->setSavedValue("draft", std::string(m_input->getString()));
		unschedule(schedule_selector(AIPanel::tick));
		m_request.cancel();
		m_healthRequest.cancel();
		Popup::onClose(sender);
	}

	void onSettings(CCObject*) { geode::openSettingsPopup(Mod::get()); }
	void onReconnect(CCObject*) { checkHealth(); }
	void onAsk(CCObject*) { if (!m_busy) sendRequest(true); }

	bool loadPolicy() {
		m_allowed.clear();
		auto policy = file::readJson(Mod::get()->getResourcesDir() / "object-whitelist.json");
		if (!policy) return false;
		auto ids = policy.unwrap()[m_requestDecor ? "decor" : "gameplay"].asArray();
		if (!ids) return false;
		for (auto const& value : ids.unwrap()) {
			auto id = value.asInt().unwrapOr(0);
			if (id > 0 && id < 100000) m_allowed.insert(static_cast<int>(id));
		}
		return !m_allowed.empty();
	}

	matjson::Value boundsJSON() const {
		matjson::Value b;
		b["min_x"] = m_bounds.getMinX(); b["max_x"] = m_bounds.getMaxX();
		b["min_y"] = m_bounds.getMinY(); b["max_y"] = m_bounds.getMaxY();
		return b;
	}

	void onGenerate(CCObject*) {
		if (m_busy) {
			m_request.cancel();
			setBusy(false);
			setStatus("Cancelled", ui::kMuted);
			setAnswer("Request cancelled. No late response will be applied.");
			return;
		}
		sendRequest(false);
	}

	void sendRequest(bool chat) {
		m_requestChat = chat;
		std::string prompt = m_input->getString();
		if (prompt.find_first_not_of(" \t\r\n") == std::string::npos)
			return this->setStatus("Write a prompt first", ui::kBad);
		m_requestDecor = m_decor;
		Mod::get()->setSavedValue("draft", prompt);
		m_limit = static_cast<int>(std::clamp<int64_t>(Mod::get()->getSettingValue<int64_t>("object-limit"), 1, 1500));
		if (!loadPolicy()) return setStatus("Missing or invalid object whitelist", ui::kBad);

		float maxX = 0.f;
		for (auto obj : CCArrayExt<GameObject*>(m_editor->m_objects)) {
			if (std::isfinite(obj->getPositionX())) maxX = std::max(maxX, obj->getPositionX());
		}
		maxX = std::clamp(maxX, 0.f, 999000.f);
		m_bounds = {maxX, 0.f, std::min(3000.f, 999000.f - maxX), 3000.f};

		matjson::Value body;
		body["mode"] = std::string(m_decor ? "decor" : "gameplay");
		body["prompt"] = prompt;
		body["action"] = std::string(chat ? "chat" : "build");
		body["start_x"] = static_cast<double>(maxX);
		body["cli"] = Mod::get()->getSettingValue<std::string>("cli");
		body["max_objects"] = m_limit;
		if (m_requestDecor && !chat) {
			auto selected = m_editor->m_editorUI->getSelectedObjects();
			if (selected && selected->count()) {
				float left = 999000.f, right = 0.f, bottom = 30000.f, top = 0.f;
				for (auto obj : CCArrayExt<GameObject*>(selected)) {
					auto rect = obj->getObjectRect();
					left = std::min(left, rect.getMinX()); right = std::max(right, rect.getMaxX());
					bottom = std::min(bottom, rect.getMinY()); top = std::max(top, rect.getMaxY());
				}
				// A two-cell border allows decorations around the selected structures.
				left = std::clamp(left - 60.f, 0.f, 999000.f); right = std::clamp(right + 60.f, left, 999000.f);
				bottom = std::clamp(bottom - 60.f, 0.f, 30000.f); top = std::clamp(top + 60.f, bottom, 30000.f);
				m_bounds = {left, bottom, right - left, top - bottom};
			}
			else {
				auto win = CCDirector::get()->getWinSize();
				auto a = m_editor->m_objectLayer->convertToNodeSpace({0.f, 0.f});
				auto b = m_editor->m_objectLayer->convertToNodeSpace({win.width, win.height});
				float left = std::clamp(std::min(a.x, b.x), 0.f, 999000.f);
				float bottom = std::clamp(std::min(a.y, b.y), 0.f, 30000.f);
				float right = std::clamp(std::max(a.x, b.x), left, std::min(left + 6000.f, 999000.f));
				float top = std::clamp(std::max(a.y, b.y), bottom, std::min(bottom + 6000.f, 30000.f));
				m_bounds = {left, bottom, right - left, top - bottom};
			}
			std::vector<matjson::Value> existing;
			for (auto obj : CCArrayExt<GameObject*>(m_editor->m_objects)) {
				if (!m_bounds.intersectsRect(obj->getObjectRect())) continue;
				matjson::Value entry;
				entry["id"] = obj->m_objectID;
				entry["x"] = obj->getPositionX(); entry["y"] = obj->getPositionY();
				existing.push_back(std::move(entry));
				if (existing.size() == 300) break;
			}
			if (existing.empty()) return setStatus("Decor: select blocks or move to a built area", ui::kBad);
			body["existing_objects"] = existing;
			body["start_x"] = m_bounds.getMinX();
		}
		body["bounds"] = boundsJSON();
		if (!m_requestDecor && !chat) {
			std::vector<matjson::Value> nearby;
			for (auto obj : CCArrayExt<GameObject*>(m_editor->m_objects)) {
				if (obj->getPositionX() < maxX - 600.f || obj->getPositionX() > maxX) continue;
				matjson::Value entry;
				entry["id"] = obj->m_objectID;
				entry["x"] = obj->getPositionX(); entry["y"] = obj->getPositionY();
				nearby.push_back(std::move(entry));
				if (nearby.size() == 300) break;
			}
			body["existing_objects"] = nearby;
		}

		this->setBusy(true);
		this->setStatus("Thinking...", ui::kMuted);
		setAnswer("Waiting for the selected AI. Keep this panel open; Cancel stops waiting.");
		log::info("AI request started: action={}, cli={}", chat ? "chat" : "build", Mod::get()->getSettingValue<std::string>("cli"));

		auto url = bridgeURL() + "/generate";
		web::WebRequest req;
		req.bodyJSON(body);
		req.timeout(std::chrono::seconds(180));

		m_request.spawn(req.post(url), [weak = WeakRef(this)](web::WebResponse res) {
			auto self = weak.lock();
			if (!self) return;
			log::info("AI request completed: HTTP {}", res.code());
			if (!res.ok()) {
				self->setBusy(false);
				auto json = res.json();
				auto error = json ? json.unwrap()["error"].asString().unwrapOr("") : "";
				std::string message = "Bridge HTTP error " + std::to_string(res.code());
				if (res.code() <= 0) message = "Bridge offline or request timed out";
				else if (error == "cli_not_logged_in") message = "CLI not logged in: run login in terminal";
				else if (error == "cli_missing") message = "Selected CLI not installed on bridge";
				else if (error == "invalid_json") message = "CLI returned invalid JSON after retry";
				else if (error == "timeout") message = "CLI timed out; try a shorter prompt";
				else if (error == "busy") message = "Bridge busy; try again shortly";
				else if (error == "cli_failed") message = "CLI failed; check login and quota";
				self->checkHealth();
				self->setAnswer(message + ". Open Settings to check the bridge address and CLI.");
				return self->setStatus(message, ui::kBad);
			}
			auto json = res.json();
			if (!json) {
				self->setBusy(false);
				self->setAnswer("The bridge returned invalid JSON. Update or restart bridge.py.");
				return self->setStatus("Bad JSON from bridge", ui::kBad);
			}
			auto message = json.unwrap()["message"].asString().unwrapOr("");
			if (self->m_requestChat) {
				self->setBusy(false);
				self->setAnswer(message.empty() ? "Bridge returned no text. Restart the updated bridge.py." : message);
				self->setStatus(message.empty() ? "Empty reply" : "AI replied", message.empty() ? ui::kBad : ui::kGood);
			}
			else {
				self->applyObjects(json.unwrap());
				if (!message.empty()) self->setAnswer(message);
			}
		});
	}

	void applyObjects(matjson::Value const& json) {
		this->setBusy(false);
		auto arr = json["objects"].asArray();
		if (!arr) return this->setStatus("Bad response from bridge", ui::kBad);

		int count = 0;
		int scanned = 0;
		auto created = CCArray::create();
		for (auto const& o : arr.unwrap()) {
			if (++scanned > 10000 || count >= m_limit) break;
			if (!o.isObject()) continue;
			auto rawID = o["id"].asInt().unwrapOr(0);
			if (o["id"].asDouble().unwrapOr(0.) != static_cast<double>(rawID)) continue;
			if (rawID <= 0 || rawID >= 100000 || !m_allowed.contains(static_cast<int>(rawID))) continue;
			int id = static_cast<int>(rawID);
			auto px = o["x"].asDouble(); auto py = o["y"].asDouble();
			if (!px || !py) continue;
			float x = static_cast<float>(px.unwrap());
			float y = static_cast<float>(py.unwrap());
			if ((o.contains("rot") && !o["rot"].isNumber()) ||
				(o.contains("scale") && !o["scale"].isNumber())) continue;
			float rot = static_cast<float>(o["rot"].asDouble().unwrapOr(0.0));
			float scale = static_cast<float>(o["scale"].asDouble().unwrapOr(1.0));
			if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(rot) || !std::isfinite(scale)) continue;
			if (x < 0.f || x > 999000.f || y < 0.f || y > 30000.f ||
				!m_bounds.containsPoint({x, y}) || scale < 0.5f || scale > 2.f || std::abs(rot) > 360.f) continue;

			auto obj = m_editor->createObject(id, {x, y}, true);
			if (!obj) continue;
			if (rot != 0.f) obj->setRotation(rot);
			if (scale != 1.f) {
				obj->updateCustomScaleX(scale);
				obj->updateCustomScaleY(scale);
			}
			// Test the actual footprint, not just the object's center.
			auto rect = obj->getObjectRect();
			if (rect.getMinX() < m_bounds.getMinX() - 0.1f || rect.getMaxX() > m_bounds.getMaxX() + 0.1f ||
				rect.getMinY() < m_bounds.getMinY() - 0.1f || rect.getMaxY() > m_bounds.getMaxY() + 0.1f) {
				m_editor->removeObject(obj, true);
				continue;
			}
			created->addObject(obj);
			count++;
		}
		if (count) {
			m_editor->addToUndoList(UndoObject::createWithArray(created, UndoCommand::Paste), false);
			m_editor->m_editorUI->updateButtons();
		}
		this->setStatus(count ? "Added " + std::to_string(count) + " objects (one undo)" :
			"No valid objects in response", count ? ui::kGood : ui::kBad);
		setAnswer(count ? "Generation applied. Undo removes the whole section in one step." :
			"No objects were added. Ask for a simpler section or select a different area.");
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
