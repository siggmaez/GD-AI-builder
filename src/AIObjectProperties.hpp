#pragma once
#include "AIStyle.hpp"
#include <map>

class AIObjectProperties {
    LevelEditorLayer* m_editor=nullptr;
    std::map<int,int> m_groups,m_colors;
public:
    std::string mappingSummary() const {
        std::string result;
        auto append=[&](char const* title,auto const& entries){
            if(entries.empty())return;
            if(!result.empty())result+=" | ";result+=title;
            size_t shown=0;
            for(auto [local,actual]:entries){if(shown++)result+=", ";result+=fmt::format("{} -> {}",local,actual);if(shown==8){if(entries.size()>8)result+=", ...";break;}}
        };
        append("Groups: ",m_groups);append("Colors: ",m_colors);return result;
    }
    static bool trigger(int id){return id==899||id==901||id==1006||id==1007||id==1049||id==1268||id==1346;}
    bool prepare(LevelEditorLayer* editor,std::vector<matjson::Value> const& objects,matjson::Value const& palette){
        m_editor=editor;m_groups.clear();m_colors.clear();
        gd::unordered_set<int> reserved;
        auto group=[&](int local){
            if(local<=0||local>9999)return false;
            if(m_groups.contains(local))return true;
            int free=editor->getNextFreeGroupID(reserved);
            if(free<=0||free>9999)return false;
            reserved.insert(free);m_groups[local]=free;return true;
        };
        for(auto const& obj:objects){
            if(auto list=obj["groups"].asArray())for(auto const& id:list.unwrap())if(!group(id.asInt().unwrapOr(0)))return false;
            for(auto key:{"target_group","center_group"})
                if(obj["trigger"].contains(key)&&!group(obj["trigger"][key].asInt().unwrapOr(0)))return false;
        }
        auto manager=editor->m_effectManager;if(!manager)return false;
        std::unordered_set<int> used;
        if(auto actions=manager->getAllColorActions())for(auto action:CCArrayExt<ColorAction*>(actions)){
            used.insert(action->m_colorID);used.insert(action->m_copyID);
        }
        for(auto obj:CCArrayExt<GameObject*>(editor->m_objects)){
            if(obj->m_baseColor)used.insert(obj->m_baseColor->m_colorID);
            if(obj->m_detailColor)used.insert(obj->m_detailColor->m_colorID);
            if(obj->m_targetColor>0)used.insert(obj->m_targetColor);
            if(auto effect=typeinfo_cast<EffectGameObject*>(obj)){
                used.insert(effect->m_copyColorID);
                if(obj->m_objectID==1006&&effect->m_pulseTargetType==0)used.insert(effect->m_targetGroupID);
            }
        }
        auto colors=palette.asArray();if(!colors)return true;
        if(colors.unwrap().size()>128)return false;
        for(auto const& color:colors.unwrap()){
            int local=color["id"].asInt().unwrapOr(0),free=1;
            if(local<=0||local>9999||m_colors.contains(local))return false;
            while(free<=999&&used.contains(free))++free;
            if(free>999)return false;
            used.insert(free);m_colors[local]=free;
        }
        for(auto const& color:colors.unwrap()){
            auto component=[&](char const* key){return static_cast<GLubyte>(std::clamp<int64_t>(color[key].asInt().unwrapOr(255),0,255));};
            auto action=ColorAction::create({component("r"),component("g"),component("b")},color["blending"].asBool().unwrapOr(false),-1);
            float opacity=static_cast<float>(std::clamp(color["opacity"].asDouble().unwrapOr(1.),0.,1.));
            action->m_fromOpacity=opacity;action->m_toOpacity=opacity;action->m_currentOpacity=opacity;
            manager->setColorAction(action,m_colors.at(color["id"].asInt().unwrapOr(0)));
            if(action->m_colorSprite)manager->updateColorAction(action);
        }
        return true;
    }
    std::string encode(matjson::Value const& o){
        auto id=o["id"].asInt().unwrapOr(0);
        std::string text=fmt::format("1,{},2,{},3,{},6,{},32,{}",id,o["x"].asDouble().unwrapOr(0.),
            o["y"].asDouble().unwrapOr(0.),o["rot"].asDouble().unwrapOr(0.),o["scale"].asDouble().unwrapOr(1.));
        auto put=[&](int key,auto value){text+=fmt::format(",{},{}",key,value);};
        auto mapped=[&](auto const& map,matjson::Value const& value){auto it=map.find(value.asInt().unwrapOr(0));return it==map.end()?0:it->second;};
        for(auto [name,key]:std::initializer_list<std::pair<char const*,int>>{{"scale_x",128},{"scale_y",129},{"z_layer",24},{"z_order",25},{"editor_layer",20}}){
            if(!o.contains(name))continue;
            auto value=o[name].asDouble();if(!value||!std::isfinite(value.unwrap())||std::abs(value.unwrap())>9999)return "";
            if((key==128||key==129)&&(value.unwrap()<.1||value.unwrap()>10))return "";
            put(key,value.unwrap());
        }
        for(auto [name,key]:std::initializer_list<std::pair<char const*,int>>{{"flip_x",4},{"flip_y",5},{"no_glow",96}})
            if(o.contains(name))put(key,o[name].asBool().unwrapOr(false)?1:0);
        for(auto [name,key]:std::initializer_list<std::pair<char const*,int>>{{"base_color",21},{"detail_color",22}})
            if(o.contains(name)){int channel=mapped(m_colors,o[name]);if(!channel)return "";put(key,channel);}
        if(auto groups=o["groups"].asArray()){
            if(groups.unwrap().size()>10)return "";
            std::string list;
            for(auto const& local:groups.unwrap()){int group=mapped(m_groups,local);if(!group)return "";if(!list.empty())list+='.';list+=std::to_string(group);}
            if(!list.empty()){text+=",57,";text+=list;}
        }
        auto t=o["trigger"];
        if(trigger(static_cast<int>(id))){
            if(!t.isObject())return "";
            put(36,1);
            if((id==899&&!t.contains("target_color"))||
                (id==1006&&!t.contains("target_group")&&!t.contains("target_color"))||
                (id!=899&&id!=1006&&!t.contains("target_group")))return "";
            for(auto [name,key]:std::initializer_list<std::pair<char const*,int>>{{"target_group",51},{"center_group",71}})
                if(t.contains(name)){int group=mapped(m_groups,t[name]);if(!group)return "";put(key,group);}
            // Pulse uses key 51 for either target type; Color uses key 23.
            // Verified against G.js src/types/color.ts (color.pulse / color.set).
            if(t.contains("target_color")){int channel=mapped(m_colors,t["target_color"]);if(!channel)return "";put(id==1006?51:23,channel);}
            for(auto [name,key]:std::initializer_list<std::pair<char const*,int>>{{"duration",10},{"move_x",28},{"move_y",29},{"opacity",35},{"easing",30},{"angle",68},{"delay",63},{"fade_in",45},{"hold",46},{"fade_out",47},{"r",7},{"g",8},{"b",9}})
                if(t.contains(name)){auto value=t[name].asDouble();if(!value||!std::isfinite(value.unwrap())||std::abs(value.unwrap())>30000)return "";put(key,value.unwrap());}
            for(auto [name,key]:std::initializer_list<std::pair<char const*,int>>{{"activate",56},{"spawn_triggered",62},{"touch_triggered",11},{"multi_triggered",87},{"blending",17}})
                if(t.contains(name))put(key,t[name].asBool().unwrapOr(false)?1:0);
            if(id==1006){put(48,0);put(52,t.contains("target_group")?1:0);}
        }
        text+=';';return text;
    }
    GameObject* create(matjson::Value const& object){
        auto text=encode(object);if(text.empty())return nullptr;
        auto objects=m_editor->createObjectsFromString(text,true,true);
        if(!objects||objects->count()!=1)return nullptr;
        return static_cast<GameObject*>(objects->objectAtIndex(0));
    }
};
