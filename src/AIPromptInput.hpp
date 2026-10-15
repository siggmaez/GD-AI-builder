#pragma once
#include "AIStyle.hpp"

// Only the AI prompt uses this delegate. Keep UTF-8 editing out of GD's
// byte-by-byte allowed-character filter, while retaining its keyboard lifecycle.
class AIPromptInput final : public CCNode, public CCTextFieldDelegate {
    TextInput* m_input=nullptr;
    CCTextInputNode* m_node=nullptr;
    CCTextFieldDelegate* m_original=nullptr;
    bool m_updating=false;

    size_t cursor(std::string const& text) const {
        size_t pos=std::min(text.size(),static_cast<size_t>(std::max(0,m_node->m_textField->m_uCursorPos)));
        while(pos<text.size() && pos && (static_cast<unsigned char>(text[pos])&0xc0)==0x80)--pos;
        return pos;
    }
    void replace(std::string text,size_t pos){
        m_updating=true;
        text=ui::limitUTF8(text,2000);
        m_input->setString(text,true);
        m_node->m_textField->m_uCursorPos=static_cast<int>(std::min(pos,text.size()));
        int glyph=0;
        for(size_t i=0;i<std::min(pos,text.size());++i)
            if((static_cast<unsigned char>(text[i])&0xc0)!=0x80)++glyph;
        m_node->updateBlinkLabelToChar(glyph);
        m_updating=false;
    }
public:
    static void install(TextInput* input){
        auto editor=new AIPromptInput;
        editor->init();editor->autorelease();editor->m_input=input;
        editor->m_node=input->getInputNode();
        editor->m_original=editor->m_node->m_textField->getDelegate();
        input->addChild(editor);
        editor->m_node->m_textField->setDelegate(editor);
    }
    void onEnter() override {CCNode::onEnter();m_node->m_textField->setDelegate(this);}
    void onExit() override {m_node->m_textField->setDelegate(m_original);CCNode::onExit();}
    bool onTextFieldAttachWithIME(CCTextFieldTTF* field) override {
        return m_original?m_original->onTextFieldAttachWithIME(field):false;
    }
    bool onTextFieldDetachWithIME(CCTextFieldTTF* field) override {
        return m_original?m_original->onTextFieldDetachWithIME(field):false;
    }
    bool onTextFieldInsertText(CCTextFieldTTF*,char const* data,int length,enumKeyCodes) override {
        if(m_updating||!data||length<=0)return true;
        auto current=std::string(m_input->getString());
        auto pos=cursor(current);
        std::string incoming(data,static_cast<size_t>(length));
        for(auto& ch:incoming)if(ch=='\r'||ch=='\n'||ch=='\t')ch=' ';
        current.insert(pos,incoming);
        replace(std::move(current),pos+incoming.size());return true;
    }
    bool onTextFieldDeleteBackward(CCTextFieldTTF*,char const*,int) override {
        if(m_updating)return true;
        auto current=std::string(m_input->getString());
        auto end=cursor(current);if(!end)return true;
        auto begin=end-1;
        while(begin && (static_cast<unsigned char>(current[begin])&0xc0)==0x80)--begin;
        current.erase(begin,end-begin);replace(std::move(current),begin);return true;
    }
    void textChanged() override {if(!m_updating&&m_original)m_original->textChanged();}
};
