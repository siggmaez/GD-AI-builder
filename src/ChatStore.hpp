#pragma once
#include <Geode/Geode.hpp>
#include <chrono>

using namespace geode::prelude;

class ChatStore {
public:
    static std::string title(std::string const& text) {
        size_t end = std::min<size_t>(64, text.size());
        while (end < text.size() && end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
        return text.substr(0, end);
    }
    static std::vector<matjson::Value>& chats() {
        static auto value = Mod::get()->getSavedValue<std::vector<matjson::Value>>("chat-history", {});
        return value;
    }
    static void save() {
        Mod::get()->setSavedValue("chat-history", chats());
        (void)Mod::get()->saveData();
    }
    static std::string newChat() {
        auto id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        matjson::Value chat;
        chat["id"] = id; chat["title"] = "New chat";
        chat["messages"] = std::vector<matjson::Value>{};
        chats().push_back(chat);
        select(id); save();
        return id;
    }
    static void select(std::string const& id) { Mod::get()->setSavedValue("active-chat", id); }
    static std::string active() {
        auto id = Mod::get()->getSavedValue<std::string>("active-chat", "");
        for (auto const& chat : chats()) if (chat["id"].asString().unwrapOr("") == id) return id;
        return newChat();
    }
    static std::vector<matjson::Value> messages(std::string const& id) {
        for (auto const& chat : chats()) if (chat["id"].asString().unwrapOr("") == id)
            return chat["messages"].asArray().unwrapOr({});
        return {};
    }
    static void append(std::string const& id, std::string const& role, std::string const& text, bool image = false) {
        for (auto& chat : chats()) if (chat["id"].asString().unwrapOr("") == id) {
            auto list = chat["messages"].asArray().unwrapOr({});
            matjson::Value m; m["role"] = role; m["text"] = text; m["image"] = image;
            list.push_back(m); chat["messages"] = list;
            if (role == "user" && chat["title"].asString().unwrapOr("") == "New chat")
                chat["title"] = text.empty() ? "Image reference" : title(text);
            save(); return;
        }
    }
};
