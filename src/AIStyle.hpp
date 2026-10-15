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
	inline char const* inputFont() {
		static auto path = (Mod::get()->getResourcesDir() / "ai-builder-font.fnt").string();
		return path.c_str();
	}

	inline char const* font() { return "bigFont.fnt"; }

	inline std::string limitUTF8(std::string const& text, size_t limit) {
		size_t count = 0;
		for (size_t i = 0; i < text.size(); ++i) {
			if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80 && count++ == limit)
				return text.substr(0, i);
		}
		return text;
	}
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

		auto lbl = CCLabelBMFont::create(text, font());
		lbl->limitLabelWidth(w - 12.f, labelScale, 0.2f);
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

