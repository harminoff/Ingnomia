/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include <QKeyEvent>
#include <QCoreApplication>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
inline std::string verifyStage05Shell(MainWindow& window,Rml::ElementDocument& doc) {
 int checks=0;std::string failures;
 auto check=[&](bool value,const char* label){++checks;if(!value)failures+=std::string(label)+";";};
 auto* c=doc.GetContext();
 auto key=[&](int code,Qt::KeyboardModifiers mods=Qt::NoModifier,QString text={}){QKeyEvent down(QEvent::KeyPress,code,mods,text);QCoreApplication::sendEvent(&window,&down);QKeyEvent up(QEvent::KeyRelease,code,mods);QCoreApplication::sendEvent(&window,&up);c->Update();};
 auto* tab=doc.GetElementById("new-tab-settlement");tab->Click();c->Update();
 auto* field=static_cast<Rml::ElementFormControlInput*>(doc.GetElementById("new-gnomes-exact"));auto* slider=doc.GetElementById("new-gnomes");
 check(field&&slider,"numeric pair exists");if(!field||!slider)return "FAIL missing pair";
 field->Focus(true);key(Qt::Key_A,Qt::ControlModifier);key(Qt::Key_1,Qt::NoModifier,"17");check(field->GetValue()=="17","Qt typing exact value");
 key(Qt::Key_Return);check(slider->GetAttribute<int>("value",0)==17,"Qt Enter updates slider");check(window.shellRouteForProbe()=="shell.new_game","Enter retains setup route");
 key(Qt::Key_A,Qt::ControlModifier);key(Qt::Key_9,Qt::NoModifier,"999");key(Qt::Key_Return);check(field->IsClassSet("is-invalid"),"invalid numeric draft visible");check(slider->GetAttribute<int>("value",0)==17,"invalid draft retains accepted slider");
 key(Qt::Key_Escape);check(field->GetValue()=="17"&&!field->IsClassSet("is-invalid"),"Qt Escape restores accepted field");check(window.shellRouteForProbe()=="shell.new_game","Escape cancels field before route");
 doc.GetElementById("new-gnomes-exact-up")->Click();c->Update();check(field->GetValue()=="18"&&slider->GetAttribute<int>("value",0)==18,"arrow and slider converge");
 auto* choice=doc.GetElementById("new-peaceful");bool checked=choice->HasAttribute("checked");choice->GetParentNode()->Click();c->Update();check(choice->HasAttribute("checked")!=checked,"production label toggles choice");
 doc.GetElementById("shell-back")->Focus(true);
 return failures.empty()?"PASS checks="+std::to_string(checks):"FAIL "+failures;
}
