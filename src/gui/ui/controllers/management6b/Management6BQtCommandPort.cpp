/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6BQtCommandPort.h"
#include "../../../aggregatorcreatureinfo.h"
#include "../../../aggregatorinventory.h"
#include "../../../aggregatorpopulation.h"
#include "../../../eventconnector.h"
#include "../../actions/UiActionRegistry.h"
#include <QMetaObject>

namespace ingnomia::ui::management6b
{
namespace
{
std::optional<::ScheduleActivity> domain(ScheduleActivity a){switch(a){case ScheduleActivity::None:return ::ScheduleActivity::None;case ScheduleActivity::Eat:return ::ScheduleActivity::Eat;case ScheduleActivity::Sleep:return ::ScheduleActivity::Sleep;case ScheduleActivity::Training:return ::ScheduleActivity::Training;}return{};}
GuiWatchedItem watched(const InventoryRowId&i){GuiWatchedItem o;o.category=QString::fromStdString(i.category.value);o.group=QString::fromStdString(i.group.value);o.item=QString::fromStdString(i.item.value);o.material=QString::fromStdString(i.material.value);return o;}
}
Management6BQtCommandPort::Management6BQtCommandPort(EventConnector*c):connector_(c){}
CommandResult Management6BQtCommandPort::reject(const char*e)const{return{CommandStatus::Rejected,false,e};}
CommandResult Management6BQtCommandPort::queue(std::function<void()>fn)const{if(!connector_||!QMetaObject::invokeMethod(connector_,std::move(fn),Qt::QueuedConnection))return reject("ui.error.bridge_queue_failed");return{CommandStatus::Accepted,true,{}};}
CommandResult Management6BQtCommandPort::dispatch(const UiActionEnvelope&a)
{
	if(!connector_)return reject("ui.error.bridge_unavailable");ActionValidationContext c;c.activeWorld=world_;c.acceptsWorldActions=accepts_;c.primaryRoute=RouteId{"game.hud"};const auto valid=UiActionRegistry{}.validate(a,c);if(!valid.valid())return reject(valid.code==ActionValidationCode::ConfirmationRequired?"ui.error.confirmation_required":"ui.error.invalid_or_stale_action");
	auto pop=connector_->aggregatorPopulation();auto inv=connector_->aggregatorInventory();auto creature=connector_->aggregatorCreatureInfo();
	if(a.id.value=="population.refresh")return queue([pop]{if(pop){pop->onRequestPopulationUpdate();pop->onRequestSchedules();pop->onRequestProfessions();}});
	if(a.id.value=="profession.refresh")return queue([pop]{if(pop)pop->onRequestProfessions();});
	if(a.id.value=="inventory.refresh")return queue([inv]{if(inv)inv->onRequestCategories();});
	if(a.id.value=="inventory.request_history"){const auto*p=std::get_if<InventoryHistoryPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");int days=0;switch(p->range){case HistoryRange::Week:days=7;break;case HistoryRange::Month:days=30;break;case HistoryRange::Season:days=91;break;case HistoryRange::Year:days=365;break;case HistoryRange::All:days=0;break;}return queue([inv,v=*p,days]{if(inv)inv->onRequestHistory(QString::fromStdString(v.item.value),QString::fromStdString(v.material.value),days);});}
	if(a.id.value=="inspect.select"){const auto*p=std::get_if<SelectPayload>(&a.payload);if(!p||p->target.kind!=EntityKind::Creature)return reject("ui.error.invalid_payload");return queue([creature,id=p->target.id]{if(creature)creature->onRequestCreatureUpdate(id);});}
	if(a.id.value=="population.set_skill"){const auto*p=std::get_if<SetSkillPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([pop,v=*p]{if(pop){pop->onSetSkillActive(v.creature.value,QString::fromStdString(v.skill.value),v.active);pop->onUpdateSingleGnome(v.creature.value);}});}
	if(a.id.value=="population.set_all_skills_for_gnome"){const auto*p=std::get_if<SetGnomeSkillsPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([pop,v=*p]{if(pop)pop->onSetAllSkills(v.creature.value,v.active);});}
	if(a.id.value=="population.set_skill_for_all"){const auto*p=std::get_if<SetSkillForAllPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([pop,v=*p]{if(pop)pop->onSetAllGnomes(QString::fromStdString(v.skill.value),v.active);});}
	if(a.id.value=="population.set_profession"){const auto*p=std::get_if<SetProfessionPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([pop,v=*p]{if(pop)pop->onSetProfession(v.creature.value,QString::fromStdString(v.profession.value));});}
	if(a.id.value=="population.set_schedule_cell"){const auto*p=std::get_if<SetScheduleCellPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");const auto d=domain(p->activity);if(!d)return reject("ui.error.schedule_activity_unavailable");return queue([pop,v=*p,d=*d]{if(pop)pop->onSetSchedule(v.cell.creature.value,v.cell.hour,d);});}
	if(a.id.value=="population.set_schedule_row"){const auto*p=std::get_if<SetScheduleRowPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");const auto d=domain(p->activity);if(!d)return reject("ui.error.schedule_activity_unavailable");return queue([pop,v=*p,d=*d]{if(pop)pop->onSetAllHours(v.creature.value,d);});}
	if(a.id.value=="population.set_schedule_column"){const auto*p=std::get_if<SetScheduleColumnPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");const auto d=domain(p->activity);if(!d)return reject("ui.error.schedule_activity_unavailable");return queue([pop,v=*p,d=*d]{if(pop)pop->onSetHourForAll(v.hour,d);});}
	if(a.id.value=="profession.update"){const auto*p=std::get_if<UpdateProfessionPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");QStringList skills;for(const auto&s:p->skills)skills.push_back(QString::fromStdString(s.value));return queue([pop,v=*p,skills]{if(pop){pop->onUpdateProfession(QString::fromStdString(v.current.value),QString::fromStdString(v.newName),skills);pop->onRequestPopulationUpdate();pop->onRequestProfessions();}});}
	if(a.id.value=="profession.delete"){const auto*p=std::get_if<ProfessionTargetPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([pop,v=*p]{if(pop){pop->onDeleteProfession(QString::fromStdString(v.profession.value));pop->onRequestPopulationUpdate();}});}
	if(a.id.value=="watch.set"){const auto*p=std::get_if<WatchPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");const auto row=watched(p->row);return queue([inv,row,on=p->watched]{if(inv){inv->onSetActive(on,row);inv->onRequestCategories();}});}
	return reject("ui.error.action_unavailable");
}
} // namespace ingnomia::ui::management6b
