/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <QCoreApplication>
#include <QKeyEvent>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <functional>
#include <stdexcept>

inline std::string verifyShellHostInput(MainWindow& window, Rml::Context& context,
    const std::function<Rml::ElementDocument*()>& document)
{
    struct Clicks : Rml::EventListener {
        int back = 0;
        void ProcessEvent(Rml::Event& event) override {
            for(auto* node=event.GetTargetElement(); node; node=node->GetParentNode())
                if(node->GetId()=="shell-back") { ++back; break; }
        }
    } clicks;
    int checks=0, pauses=0, worldKeys=0;
    std::string failures;
    const auto pauseConnection=QObject::connect(&window,&MainWindow::signalTogglePause,&window,[&]{++pauses;});
    const auto worldConnection=QObject::connect(&window,&MainWindow::signalKeyPress,&window,[&]{++worldKeys;});
    context.AddEventListener("click",&clicks,true);
    const auto check=[&](bool value,const char* label) { ++checks; if(!value) failures+=std::string(label)+";"; };
    const auto get=[&](const char* id) {
        auto* doc=document(); auto* element=doc ? doc->GetElementById(id) : nullptr;
        if(!element) throw std::runtime_error(id);
        return element;
    };
    const auto key=[&](QEvent::Type type,int code,bool repeat=false,Qt::KeyboardModifiers mods=Qt::NoModifier,QString text={}) {
        QKeyEvent event(type,code,mods,text,repeat,1); QCoreApplication::sendEvent(&window,&event); context.Update();
        return event.isAccepted();
    };
    const auto open=[&] { check(window.dispatchShellClickForProbe("shell-new-setup"),"reopen setup");context.Update(); };
    try {
        get("new-tab-settlement")->Click();context.Update();
        auto* field=dynamic_cast<Rml::ElementFormControlInput*>(get("new-kingdom-name"));
        if(!field) throw std::runtime_error("name input");
        field->Focus(true);const auto oldText=field->GetValue();
        key(QEvent::KeyPress,Qt::Key_Space,false,Qt::NoModifier," ");key(QEvent::KeyRelease,Qt::Key_Space);
        check(field->GetValue().size()==oldText.size()+1,"Space edits field");check(pauses==0,"editable Space cannot pause");
        check(!get("new-start")->HasAttribute("disabled"),"field edit does not strand default action");
        key(QEvent::KeyPress,Qt::Key_Tab,false,Qt::ControlModifier);key(QEvent::KeyRelease,Qt::Key_Tab,false,Qt::ControlModifier);
        check(get("new-tab-terrain")->IsClassSet("is-selected"),"Qt CtrlTab cycles page");
        get("shell-back")->Focus(true);
        const int before=clicks.back;
        check(key(QEvent::KeyPress,Qt::Key_Space),"Space key accepted");
        for(int i=0;i<5;++i) {key(QEvent::KeyRelease,Qt::Key_Space,true);key(QEvent::KeyPress,Qt::Key_Space,true);}
        check(clicks.back==before && window.shellRouteForProbe()=="shell.new_game","held Space does not activate");
        key(QEvent::KeyRelease,Qt::Key_Space);
        check(clicks.back==before+1 && window.shellRouteForProbe()=="shell.main_menu","release activates once");
        open();get("shell-back")->Focus(true);
        key(QEvent::KeyPress,Qt::Key_Return);
        for(int i=0;i<5;++i)key(QEvent::KeyPress,Qt::Key_Return,true);
        key(QEvent::KeyRelease,Qt::Key_Return);
        check(clicks.back==before+2 && window.shellRouteForProbe()=="shell.main_menu","Enter repeat stays single across route change");
        open();get("shell-back")->Focus(true);key(QEvent::KeyPress,Qt::Key_Space);
        key(QEvent::KeyPress,Qt::Key_Escape);key(QEvent::KeyRelease,Qt::Key_Escape);key(QEvent::KeyRelease,Qt::Key_Space);
        check(clicks.back==before+2 && window.shellRouteForProbe()=="shell.main_menu","Escape cancels held command and navigates back");
        check(pauses==0 && worldKeys==0,"no pause or world-key leakage");
        open();get("new-tab-settlement")->Click();context.Update();get("shell-back")->Focus(true);
    } catch(const std::exception& error) {failures+=std::string("missing ")+error.what();}
    context.RemoveEventListener("click",&clicks,true);QObject::disconnect(pauseConnection);QObject::disconnect(worldConnection);
    return failures.empty() ? "PASS checks="+std::to_string(checks)+" pause_signals=0 world_keys=0" : "FAIL "+failures;
}
