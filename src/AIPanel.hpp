#pragma once
#include "AIJob.hpp"
#include "AIMenus.hpp"
#include <Geode/utils/base64.hpp>
#include <Geode/utils/string.hpp>

class AIPanel : public Popup {
    Ref<AIJob> m_job;
    TextInput* m_input = nullptr;
    ScrollLayer* m_chat = nullptr;
    CCLabelBMFont *m_status = nullptr, *m_health = nullptr, *m_model = nullptr, *m_area = nullptr, *m_attachment = nullptr;
    CCMenuItemSpriteExtra* m_generate = nullptr;
    CCMenuItemSpriteExtra *m_referenceButton = nullptr, *m_areaButton = nullptr;
    CCNode *m_gamePill = nullptr, *m_decorPill = nullptr;
    CCDrawNode* m_preview = nullptr;
    CCSprite* m_photo = nullptr;
    matjson::Value m_image;
    bool m_decor = false;
    bool m_showPhoto = false;
    size_t m_revision = SIZE_MAX;
    CCRect m_previewBounds;
    unsigned int m_previewObjects = 0;
    std::string m_chatSignature;
    async::TaskHolder<web::WebResponse> m_healthRequest;
    async::TaskHolder<file::PickResult> m_picker;

    bool init(LevelEditorLayer* editor) {
        if (!Popup::init(460,280,"square02b_001.png")) return false;
        m_job = AIJob::get(editor);m_decor=m_job->m_decor;
        m_bgSprite->setColor(ui::kSurface);
        m_bgSprite->setOpacity(static_cast<GLubyte>(std::clamp<int64_t>(Mod::get()->getSettingValue<int64_t>("panel-opacity"),80,255)));
        m_closeBtn->setVisible(false);
        auto win=CCDirector::get()->getWinSize();
        // Keep a visible editor margin on every side, including phones.
        m_mainLayer->setScale(std::min({1.f,win.width*.82f/460.f,win.height*.82f/280.f}));
        auto root=CCNode::create();root->setContentSize(m_size);root->setAnchorPoint({.5f,.5f});root->setPosition(m_size/2);
        m_mainLayer->addChild(root);
        auto menu=CCMenu::create();menu->setPosition({0,0});root->addChild(menu,10);
        auto title=ui::label(root,"AI Builder",{78,259},.45f);title->limitLabelWidth(128,.45f,.3f);
        ui::button(menu,"Chats",{177,259},60,[](auto){if(auto p=HistoryPopup::create())p->show();});
        ui::button(menu,"More",{391,259},55,[](auto){if(auto p=AIToolsPopup::create())p->show();});
        ui::button(menu,"x",{440,259},24,[this](auto){onClose(nullptr);});
        auto modelButton=ui::button(menu,"CLI default",{111,237},196,[](auto){if(auto p=ModelsPopup::create())p->show();});
        m_model=static_cast<CCLabelBMFont*>(modelButton->getNormalImage()->getChildByTag(2));
        auto bg=NineSlice::create("square02b_001.png");bg->setContentSize({202,149});bg->setPosition({111,155});bg->setColor({12,14,19});root->addChild(bg);
        m_chat=ScrollLayer::create(CCSize{190,137});m_chat->setPosition({16,87});root->addChild(m_chat);
        m_gamePill=ui::makePill("Gameplay",107,24,ui::kAccent,.33f);
        m_decorPill=ui::makePill("Decor",107,24,ui::kIdle,.33f);
        auto game=CCMenuItemExt::createSpriteExtra(m_gamePill,[this](auto){if(!m_job->m_busy){m_decor=false;mode();}});
        auto decor=CCMenuItemExt::createSpriteExtra(m_decorPill,[this](auto){if(!m_job->m_busy){m_decor=true;mode();}});
        game->setPosition({277,237});decor->setPosition({393,237});menu->addChild(game);menu->addChild(decor);
        bg=NineSlice::create("square02b_001.png");bg->setContentSize({223,113});bg->setPosition({335,164});bg->setColor({12,14,19});root->addChild(bg);
        m_preview=CCDrawNode::create();m_preview->setPosition({230,114});root->addChild(m_preview);
        m_referenceButton=ui::button(menu,"Reference",{390,210},90,[this](auto){m_showPhoto=!m_showPhoto;refresh(0);});
        m_referenceButton->setVisible(false);
        m_area=ui::label(root,"Area",{335,101},.25f);
        m_areaButton=ui::button(menu,"Area: View >",{335,80},218,[this](auto){
            if(m_job->m_busy)return;
            auto selected=m_job->m_editor->m_editorUI->getSelectedObjects();
            m_job->area=m_job->area=="View"?(selected&&selected->count()?"Selection":"Extend"):
                m_job->area=="Selection"?"Extend":"View";
            m_job->computeBounds();refresh(0);
        });
        m_status=ui::label(root,"Ready",{335,57},.29f);
        m_health=ui::label(root,"Bridge checking...",{111,75},.27f);
        // The bridge label doubles as a reconnect button, without another toolbar.
        auto healthHit=CCMenuItemExt::createSpriteExtra(ui::makePill("",194,16,ui::kSurface),[this](auto){checkHealth();});
        healthHit->setPosition({111,75});healthHit->setOpacity(0);menu->addChild(healthHit);
        m_input=TextInput::create(194,"Describe your level...",ui::inputFont());
        m_input->setPosition({111,52});m_input->setFilter(std::string(getCommonFilterAllowedChars(CommonFilter::Any))+
            "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯабвгдеёжзийклмнопрстуфхцчшщъыьэюя«»—–…№");
        m_input->setMaxCharCount(0);m_input->setCallback([this](std::string const& s){
            auto value=ui::limitUTF8(s,300);if(value!=s)m_input->setString(value);
        });
        m_input->setString(Mod::get()->getSavedValue<std::string>("draft",""));root->addChild(m_input);
        ui::button(menu,"+",{25,21},26,[this](auto){
            if(!m_photo){pickImage();return;}
            createQuickPopup("Reference photo","Replace or remove the attached image?","Remove","Replace",[weak=WeakRef(this)](auto,bool replace){
                auto self=weak.lock();if(!self)return;
                if(replace){self->pickImage();return;}
                self->m_image=matjson::Value();self->m_showPhoto=false;
                if(self->m_photo){self->m_photo->removeFromParent();self->m_photo=nullptr;}
                self->m_attachment->setString("");self->refresh(0);
            });
        });
        m_attachment=ui::label(root,"",{76,21},.24f,true);
        ui::button(menu,"Ask AI",{165,21},90,[this](auto){send(true);});
        m_generate=ui::button(menu,"Generate",{335,26},218,[this](auto){if(m_job->m_busy)m_job->cancel();else send(false);},ui::kAccent);
        mode();refresh(0);checkHealth();schedule(schedule_selector(AIPanel::refresh),.25f);
        return true;
    }
    void mode(){
        ui::recolor(m_gamePill,m_decor?ui::kIdle:ui::kAccent,m_decor?ui::kMuted:ui::kSurface);
        ui::recolor(m_decorPill,m_decor?ui::kAccent:ui::kIdle,m_decor?ui::kSurface:ui::kMuted);
    }
    void send(bool chat){
        if(m_job->m_busy)return;
        m_job->sendRequest(chat,m_input->getString(),m_decor,m_image);
        if(m_job->m_busy){
            m_input->setString("");Mod::get()->setSavedValue("draft",std::string(""));
            m_image=matjson::Value();m_attachment->setString("");
        }
        refresh(0);
    }
    void refresh(float){
        if(!m_job->m_editor){onClose(nullptr);return;}
        auto model=Mod::get()->getSettingValue<std::string>("model");
        m_model->setString(model.empty()?"CLI default":model.c_str());m_model->limitLabelWidth(180,.3f,.18f);
        m_input->setEnabled(!m_job->m_busy);
        static_cast<CCLabelBMFont*>(m_generate->getNormalImage()->getChildByTag(2))->setString(m_job->m_busy?"Stop":"Generate");
        m_status->setString(m_job->status.c_str());m_status->setColor(m_job->statusColor);m_status->limitLabelWidth(218,.29f,.18f);
        auto id=ChatStore::active();auto messages=ChatStore::messages(id);
        auto signature=id+":"+std::to_string(messages.size());
        if(signature!=m_chatSignature){m_chatSignature=signature;renderChat(messages);}
        m_job->computeBounds();
        auto b=m_job->m_bounds;
        auto bounds=fmt::format("{:.0f} x {:.0f} units",b.size.width,b.size.height);
        m_area->setString(bounds.c_str());m_area->limitLabelWidth(218,.25f,.18f);
        auto areaLabel=std::string("Area: ")+m_job->area+" >";
        static_cast<CCLabelBMFont*>(m_areaButton->getNormalImage()->getChildByTag(2))->setString(areaLabel.c_str());
        m_referenceButton->setVisible(m_photo!=nullptr);
        if(m_photo)m_photo->setVisible(m_showPhoto);
        m_preview->setVisible(!m_showPhoto || !m_photo);
        if(!m_preview->isVisible())return;
        auto objectCount=m_job->m_editor->m_objects->count();
        if(m_revision==m_job->revision && objectCount==m_previewObjects &&
            b.getMinX()==m_previewBounds.getMinX() && b.getMinY()==m_previewBounds.getMinY() &&
            b.size.width==m_previewBounds.size.width && b.size.height==m_previewBounds.size.height)return;
        m_revision=m_job->revision;m_previewObjects=objectCount;m_previewBounds=b;
        m_preview->clear();
        float factor=std::min(210.f/std::max(1.f,b.size.width),98.f/std::max(1.f,b.size.height));
        m_preview->drawRect({0,0,b.size.width*factor,b.size.height*factor},{0,0,0,0},.7f,{.4f,.5f,.8f,1});
        size_t count=0;
        for(auto obj:CCArrayExt<GameObject*>(m_job->m_editor->m_objects)){
            auto r=obj->getObjectRect();if(!b.intersectsRect(r))continue;
            float x=std::clamp(r.getMinX(),b.getMinX(),b.getMaxX()),y=std::clamp(r.getMinY(),b.getMinY(),b.getMaxY());
            float right=std::clamp(r.getMaxX(),b.getMinX(),b.getMaxX()),top=std::clamp(r.getMaxY(),b.getMinY(),b.getMaxY());
            m_preview->drawRect({(x-b.getMinX())*factor,(y-b.getMinY())*factor,std::max(.8f,(right-x)*factor),std::max(.8f,(top-y)*factor)},
                {.5f,.6f,.85f,.8f},0,{0,0,0,0});
            if(++count>=2000)break;
        }
    }
    void renderChat(std::vector<matjson::Value> const& messages){
        auto content=m_chat->m_contentLayer;content->removeAllChildren();
        float y=8;
        if(messages.empty()){
            auto hint=CCLabelBMFont::create("Describe a section.\n\nAsk AI for advice, or press Generate to build.\n\nUse + to add a reference.",ui::inputFont(),425,kCCTextAlignmentLeft);
            hint->setScale(.4f);hint->setAnchorPoint({0,1});hint->setPosition({10,129});hint->setColor(ui::kMuted);
            content->addChild(hint);content->setContentSize({190,137});content->setPosition({0,0});return;
        }
        // Lay out newest at the bottom, so position zero shows the latest reply.
        size_t shown=0;
        for(auto it=messages.rbegin();it!=messages.rend() && shown<60;++it,++shown){
            auto text=(*it)["text"].asString().unwrapOr("");
            bool user=(*it)["role"].asString().unwrapOr("")=="user";
            if((*it)["image"].asBool().unwrapOr(false))text="[Reference image]\n"+text;
            auto label=CCLabelBMFont::create(text.c_str(),ui::inputFont(),415,kCCTextAlignmentLeft);
            label->setScale(.4f);label->setAnchorPoint({0,0});label->setColor(ui::kText);
            float height=label->getContentHeight()*.4f+25;
            auto bg=NineSlice::create("square02b_001.png");bg->setColor(user?ccColor3B{38,45,68}:ccColor3B{27,30,37});
            bg->setContentSize({182,height});bg->setPosition({95,y+height/2});content->addChild(bg);
            auto role=ui::label(content,user?"You":"AI",{20,y+height-9},.22f);role->setAnchorPoint({0,.5f});
            label->setPosition({12,y+6});content->addChild(label);y+=height+7;
        }
        content->setContentSize({190,std::max(137.f,y)});content->setPosition({0,0});
    }
    void checkHealth(){
        m_healthRequest.spawn(web::WebRequest().timeout(std::chrono::seconds(4)).get(ui::bridgeURL()+"/health"),
            [weak=WeakRef(this)](web::WebResponse response){
                if(auto self=weak.lock()){
                    auto json=response.json();bool online=response.ok()&&json&&json.unwrap()["status"].asString().unwrapOr("")=="ok";
                    self->m_health->setString(online?"Bridge online":"Bridge offline");self->m_health->setColor(online?ui::kGood:ui::kBad);
                }
            });
    }
    void pickImage(){
        file::FilePickOptions options;options.filters={{"PNG or JPEG",{"*.png","*.jpg","*.jpeg"}}};
        m_picker.spawn(file::pick(file::PickMode::OpenFile,options),[weak=WeakRef(this)](file::PickResult result){
            auto self=weak.lock();if(!self)return;
            if(!result){self->m_attachment->setString("Cannot open image picker");return;}
            auto selected=result.unwrap();if(!selected)return;
            std::error_code error;auto size=std::filesystem::file_size(*selected,error);
            if(error||size>5*1024*1024){self->m_attachment->setString("Max 5 MB");return;}
            auto bytes=file::readBinary(*selected);
            if(!bytes){self->m_attachment->setString("Cannot read image");return;}
            auto const& raw=bytes.unwrap();
            bool png=raw.size()>=8&&raw[0]==137&&raw[1]==80&&raw[2]==78&&raw[3]==71;
            bool jpg=raw.size()>=3&&raw[0]==255&&raw[1]==216&&raw[2]==255;
            if(!png&&!jpg){self->m_attachment->setString("Choose a PNG or JPEG image");return;}
            self->m_image["data"]=utils::base64::encode(raw,utils::base64::Base64Variant::Normal);
            self->m_attachment->setString("1 photo");
            if(self->m_photo){self->m_photo->removeFromParent();self->m_photo=nullptr;}
            auto path=utils::string::pathToString(*selected);auto photo=CCSprite::create(path.c_str());
            if(photo){
                photo->setScale(std::min(211.f/photo->getContentSize().width,101.f/photo->getContentSize().height));
                photo->setPosition({335,164});
                // Preview and controls share the content coordinate system.
                self->m_preview->getParent()->addChild(photo);self->m_photo=photo;self->m_showPhoto=true;
            }
        });
    }
    void onClose(CCObject* sender) override {
        Mod::get()->setSavedValue("draft",std::string(m_input->getString()));
        // The job belongs to EditorUI and keeps running after this view is destroyed.
        m_healthRequest.cancel();m_picker.cancel();Popup::onClose(sender);
    }
public:
    static AIPanel* create(LevelEditorLayer* editor){
        auto panel=new AIPanel;
        if(panel->init(editor)){panel->autorelease();return panel;}
        delete panel;return nullptr;
    }
};
