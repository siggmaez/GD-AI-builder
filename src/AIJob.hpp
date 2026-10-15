#pragma once
#include "AIStyle.hpp"
#include "ChatStore.hpp"

// Owned by EditorUI, independently of the popup; leaving the editor cancels it.
class AIJob : public CCNode {
public:
    LevelEditorLayer* m_editor = nullptr;
    std::string status = "Ready", reply, m_chat;
    ccColor3B statusColor = ui::kMuted;
    size_t revision = 0;
    bool m_busy = false, m_applying = false, m_requestChat = false;
    bool m_requestDecor = false, m_decor = false;
    CCRect m_bounds;
    std::string area = "View";
    async::TaskHolder<web::WebResponse> m_request;
    std::unordered_set<int> m_allowed;
    size_t m_limit = 300, m_cursor = 0;
    int m_timeout = 180, m_elapsed = 0;
    std::vector<matjson::Value> m_pending;
    Ref<CCArray> m_created;
    std::unordered_set<std::string> m_placements;
    std::unordered_set<int64_t> m_solidCells;
    CCDrawNode* m_outline = nullptr;

    static AIJob* get(LevelEditorLayer* editor) {
        if (auto node = editor->m_editorUI->getChildByID("ai-builder-job"_spr))
            return static_cast<AIJob*>(node);
        auto node = new AIJob;
        node->init(); node->autorelease(); node->m_editor = editor;
        node->setID("ai-builder-job"_spr);
        editor->m_editorUI->addChild(node);
        auto selection = editor->m_editorUI->getSelectedObjects();
        if (selection && selection->count()) node->area = "Selection";
        node->computeBounds();
        return node;
    }
    void onExit() override {
        m_request.cancel();
        if (m_applying) finishApply();
        if (m_outline) { m_outline->removeFromParent(); m_outline = nullptr; }
        m_editor = nullptr;
        CCNode::onExit();
    }
    std::string bridgeURL() const {
        auto url = Mod::get()->getSettingValue<std::string>("bridge-url");
        while (!url.empty() && url.back() == '/') url.pop_back();
        return url;
    }
    void setStatus(std::string const& value, ccColor3B color) {
        status = value; statusColor = color; ++revision;
    }
    void setAnswer(std::string const& value, bool persist = true) {
        reply = value; ++revision;
        if (persist && !m_chat.empty()) ChatStore::append(m_chat, "assistant", value);
    }
    void tick(float) {
        if (!m_applying) setStatus("Thinking... " + std::to_string(++m_elapsed) + "s", ui::kMuted);
    }
    void setBusy(bool busy) {
        m_busy = busy; ++revision;
        if (busy) { m_elapsed = 0; schedule(schedule_selector(AIJob::tick), 1.f); }
        else {
            unschedule(schedule_selector(AIJob::tick));
            if (m_outline) { m_outline->removeFromParent(); m_outline = nullptr; }
        }
    }
    void cancel() {
        m_request.cancel();
        if (m_applying) finishApply();
        setBusy(false); setStatus("Stopped", ui::kMuted);
        setAnswer("Stopped. Objects already placed remain in one undo step.");
    }
    void computeBounds() {
        if (m_busy || !m_editor) return;
        auto win = CCDirector::get()->getWinSize();
        auto toolbar = std::clamp(m_editor->m_editorUI->m_toolbarHeight, 40.f, win.height * .45f);
        auto a = m_editor->m_objectLayer->convertToNodeSpace({0.f, toolbar});
        auto b = m_editor->m_objectLayer->convertToNodeSpace({win.width, win.height - 48.f});
        float left = std::min(a.x,b.x), right = std::max(a.x,b.x);
        float bottom = std::min(a.y,b.y), top = std::max(a.y,b.y);
        auto selected = m_editor->m_editorUI->getSelectedObjects();
        if (area == "Selection" && selected && selected->count()) {
            left = 999000; right = 0; bottom = 30000; top = 0;
            for (auto obj : CCArrayExt<GameObject*>(selected)) {
                auto r = obj->getObjectRect();
                left = std::min(left,r.getMinX()); right = std::max(right,r.getMaxX());
                bottom = std::min(bottom,r.getMinY()); top = std::max(top,r.getMaxY());
            }
        } else if (area == "Selection") area = "View";
        if (area == "Extend") {
            left = 0;
            for (auto obj : CCArrayExt<GameObject*>(m_editor->m_objects))
                if (std::isfinite(obj->getPositionX())) left = std::max(left,obj->getObjectRect().getMaxX());
            right = left + Mod::get()->getSettingValue<int64_t>("section-length");
            bottom = 0; top = Mod::get()->getSettingValue<int64_t>("area-height");
        }
        left = std::clamp(left,0.f,999000.f); right = std::clamp(right,left,999000.f);
        bottom = std::clamp(bottom,0.f,30000.f); top = std::clamp(top,bottom,30000.f);
        m_bounds = {left,bottom,right-left,top-bottom};
    }
    void drawBounds() {
        if (m_outline) m_outline->removeFromParent();
        m_outline = CCDrawNode::create();
        m_editor->m_objectLayer->addChild(m_outline, 1000);
        m_outline->drawRect(m_bounds, {0,0,0,0}, 1.5f, {.5f,.65f,1.f,.8f});
    }
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

	void sendRequest(bool chat, std::string prompt, bool decor, matjson::Value image = {}) {
		m_requestChat = chat;
		m_decor = decor;
		m_chat = ChatStore::active();
		if (prompt.find_first_not_of(" \t\r\n") == std::string::npos)
			return this->setStatus("Write a prompt first", ui::kBad);
		m_requestDecor = m_decor;
		Mod::get()->setSavedValue("draft", prompt);
		m_limit = static_cast<size_t>(std::max<int64_t>(0, Mod::get()->getSettingValue<int64_t>("object-limit")));
		m_timeout = static_cast<int>(std::clamp<int64_t>(Mod::get()->getSettingValue<int64_t>("request-timeout"), 15, INT_MAX));
		if (!loadPolicy()) return setStatus("Missing or invalid object whitelist", ui::kBad);

		computeBounds();
        auto maxX = m_bounds.getMinX();

		matjson::Value body;
		body["mode"] = std::string(m_decor ? "decor" : "gameplay");
		body["prompt"] = prompt;
		body["action"] = std::string(chat ? "chat" : "build");
		body["language"] = Mod::get()->getSettingValue<std::string>("reply-language");
		body["object_policy"] = Mod::get()->getSettingValue<std::string>("object-policy");
		body["difficulty"] = Mod::get()->getSettingValue<std::string>("difficulty");
		body["decor_density"] = Mod::get()->getSettingValue<std::string>("decor-density");
		body["strict_bounds"] = true;
		body["allow_mode_changes"] = Mod::get()->getSettingValue<bool>("allow-mode-changes");
		body["avoid_overlap"] = Mod::get()->getSettingValue<bool>("avoid-overlap");
		body["timeout"] = m_timeout;
		body["start_x"] = static_cast<double>(maxX);
		body["cli"] = Mod::get()->getSettingValue<std::string>("cli");
		body["max_objects"] = m_limit;
        body["bounds"] = boundsJSON();
        body["editor_bounds"] = boundsJSON();
        body["strict_bounds"] = true;
        body["model"] = Mod::get()->getSettingValue<std::string>("model");
        body["history"] = ChatStore::messages(m_chat);
        if (image.isObject()) body["image"] = image;
        std::vector<matjson::Value> existing;
        for (auto obj : CCArrayExt<GameObject*>(m_editor->m_objects)) {
            if (!m_bounds.intersectsRect(obj->getObjectRect())) continue;
            matjson::Value entry;
            entry["id"] = obj->m_objectID;
            entry["x"] = obj->getPositionX(); entry["y"] = obj->getPositionY();
            existing.push_back(entry);
            if (existing.size() == 300) break;
        }
        body["existing_objects"] = existing;
        ChatStore::append(m_chat, "user", prompt, image.isObject());

		drawBounds();
		this->setBusy(true);
		this->setStatus("Thinking...", ui::kMuted);
		setAnswer("Working... You can close this window and watch the editor.", false);
		log::info("AI request started: action={}, cli={}", chat ? "chat" : "build", Mod::get()->getSettingValue<std::string>("cli"));

		auto url = bridgeURL() + "/generate";
		web::WebRequest req;
		req.bodyJSON(body);
		req.timeout(std::chrono::seconds(m_timeout));

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
		auto arr = json["objects"].asArray();
		if (!arr) { setBusy(false); return setStatus("Bad response from bridge", ui::kBad); }
		m_pending = arr.unwrap();
		m_cursor = 0;
		m_created = CCArray::create();
		m_placements.clear();
		m_solidCells.clear();
		for (auto obj : CCArrayExt<GameObject*>(m_editor->m_objects)) {
			m_placements.insert(fmt::format("{}:{:.2f}:{:.2f}:{:.2f}", obj->m_objectID, obj->getPositionX(), obj->getPositionY(), obj->getRotation()));
			if (obj->m_objectType == GameObjectType::Solid) m_solidCells.insert(solidCell(obj->getPosition()));
		}
		m_applying = true;
		schedule(schedule_selector(AIJob::applyBatch), .04f);
	}

	int64_t solidCell(CCPoint p) const {
		return static_cast<int64_t>(std::floor(p.x / 30.f)) * 100000 + static_cast<int64_t>(std::floor(p.y / 30.f));
	}

	void applyBatch(float) {
		auto batch = std::clamp<int64_t>(Mod::get()->getSettingValue<int64_t>("apply-batch"), 1, 1000);
		for (int64_t n = 0; n < batch && m_cursor < m_pending.size(); ++n) {
			if (m_limit && m_created->count() >= m_limit) { m_cursor = m_pending.size(); break; }
			auto const& o = m_pending[m_cursor++];
			if (!o.isObject()) continue;
			auto rawID = o["id"].asInt().unwrapOr(0);
			if (o["id"].asDouble().unwrapOr(0.) != static_cast<double>(rawID)) continue;
			if (rawID <= 0 || rawID >= 100000 || !m_allowed.contains(static_cast<int>(rawID))) continue;
			int id = static_cast<int>(rawID);
			static std::unordered_set<int> const portals = {10,11,12,13,47,99,101,111,200,201,202,203,660,745,1331,1334,1933};
			if (!Mod::get()->getSettingValue<bool>("allow-mode-changes") && portals.contains(id)) continue;
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
				scale < 0.1f || scale > 10.f || std::abs(rot) > 360.f) continue;
			bool strict = true;
			if (strict && !m_bounds.containsPoint({x, y})) continue;
			auto key = fmt::format("{}:{:.2f}:{:.2f}:{:.2f}", id, x, y, rot);
			if (m_placements.contains(key)) continue;

			auto obj = m_editor->createObject(id, {x, y}, true);
			if (!obj) continue;
			if ((m_requestDecor && obj->m_objectType != GameObjectType::Decoration) ||
				(Mod::get()->getSettingValue<bool>("avoid-overlap") && obj->m_objectType == GameObjectType::Solid && m_solidCells.contains(solidCell({x,y})))) {
				m_editor->removeObject(obj, true); continue;
			}
			if (rot != 0.f) obj->setRotation(rot);
			if (scale != 1.f) {
				obj->updateCustomScaleX(scale);
				obj->updateCustomScaleY(scale);
			}
			// Test the actual footprint, not just the object's center.
			auto rect = obj->getObjectRect();
			if (strict && (rect.getMinX() < m_bounds.getMinX() - 0.1f || rect.getMaxX() > m_bounds.getMaxX() + 0.1f ||
				rect.getMinY() < m_bounds.getMinY() - 0.1f || rect.getMaxY() > m_bounds.getMaxY() + 0.1f)) {
				m_editor->removeObject(obj, true);
				continue;
			}
			m_created->addObject(obj);
			m_placements.insert(std::move(key));
			if (obj->m_objectType == GameObjectType::Solid) m_solidCells.insert(solidCell({x,y}));
		}
		setStatus("Applying: " + std::to_string(m_created->count()) + " objects", ui::kMuted);
		if (m_cursor >= m_pending.size()) finishApply();
	}

	void finishApply() {
		unschedule(schedule_selector(AIJob::applyBatch));
		m_applying = false;
		auto count = m_created ? m_created->count() : 0;
		if (count) {
			m_editor->addToUndoList(UndoObject::createWithArray(m_created, UndoCommand::Paste), false);
			m_editor->m_editorUI->updateButtons();
		}
		this->setStatus(count ? "Added " + std::to_string(count) + " objects (one undo)" :
			"No valid objects in response", count ? ui::kGood : ui::kBad);
		if (!count) setAnswer("No objects were added. Try a simpler section or a different area.");
		m_pending.clear();
		m_created = nullptr;
		setBusy(false);
	}

};
