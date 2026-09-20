/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "InspectorQtCommandPort.h"
#include "../../../aggregatoragri.h"
#include "../../../aggregatorcreatureinfo.h"
#include "../../../aggregatormilitary.h"
#include "../../../aggregatorrenderer.h"
#include "../../../aggregatorselection.h"
#include "../../../aggregatorstockpile.h"
#include "../../../aggregatortileinfo.h"
#include "../../../aggregatorworkshop.h"
#include "../../../eventconnector.h"
#include "../../../../base/position.h"
#include "../../actions/UiActionRegistry.h"
#include <QMetaObject>

namespace ingnomia::ui::inspector
{
namespace
{
std::optional<QString> domainSlot( UniformSlot value )
{
	switch( value )
	{
		case UniformSlot::HeadArmor: return "HeadArmor";
		case UniformSlot::ChestArmor: return "ChestArmor";
		case UniformSlot::ArmArmor: return "ArmArmor";
		case UniformSlot::HandArmor: return "HandArmor";
		case UniformSlot::LegArmor: return "LegArmor";
		case UniformSlot::FootArmor: return "FootArmor";
		case UniformSlot::LeftHandHeld: return "LeftHandHeld";
		case UniformSlot::RightHandHeld: return "RightHandHeld";
		case UniformSlot::Back: return "Back";
	}
	return std::nullopt;
}
}
InspectorQtCommandPort::InspectorQtCommandPort(EventConnector*c):connector_(c){}
CommandResult InspectorQtCommandPort::reject(const char*e)const{return{CommandStatus::Rejected,false,e};}
CommandResult InspectorQtCommandPort::queue(std::function<void()>fn)const{if(!connector_||!QMetaObject::invokeMethod(connector_,std::move(fn),Qt::QueuedConnection))return reject("ui.error.bridge_queue_failed");return{CommandStatus::Accepted,true,{}};}
CommandResult InspectorQtCommandPort::dispatch(const UiActionEnvelope&a)
{
	if(!connector_)return reject("ui.error.bridge_unavailable");
	ActionValidationContext validation;validation.activeWorld=activeWorld_;validation.acceptsWorldActions=acceptsActions_;validation.primaryRoute=RouteId{"game.hud"};
	if(!UiActionRegistry{}.validate(a,validation).valid())return reject("ui.error.invalid_or_stale_action");
	if(a.id.value=="view.center_on"){
		const auto*p=std::get_if<CenterPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");std::optional<WorldPosition> pos;if(auto*w=std::get_if<WorldPosition>(&p->target))pos=*w;else pos=std::get<EntityRef>(p->target).position;if(!pos)return reject("ui.error.target_has_no_position");const Position target(pos->x,pos->y,pos->z);return queue([c=connector_,target]{if(c)c->aggregatorRenderer()->onCenterCamera(target);});}
	if(a.id.value=="inspect.select"){
		const auto*p=std::get_if<SelectPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");const auto t=p->target;
		return queue([c=connector_,t]{if(!c)return;switch(t.kind){case EntityKind::Tile:c->aggregatorTileInfo()->onShowTileInfo(t.id);break;case EntityKind::Creature:c->aggregatorCreatureInfo()->onRequestCreatureUpdate(t.id);c->aggregatorCreatureInfo()->onRequestProfessionList();break;case EntityKind::Workshop:c->aggregatorWorkshop()->onOpenWorkshopInfo(t.id);break;case EntityKind::Stockpile:c->aggregatorStockpile()->onOpenStockpileInfo(t.id);break;case EntityKind::Farm:c->aggregatorAgri()->onUpdateFarm(t.id);break;case EntityKind::Pasture:c->aggregatorAgri()->onUpdatePasture(t.id);break;case EntityKind::Grove:c->aggregatorAgri()->onUpdateGrove(t.id);break;default:break;}});}
	if(a.id.value=="population.set_profession"){
		const auto*p=std::get_if<SetProfessionPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,v=*p]{if(!c)return;auto*info=c->aggregatorCreatureInfo();info->onSetProfession(v.creature.value,QString::fromStdString(v.profession.value));info->onRequestCreatureUpdate(v.creature.value);});}
	if(a.id.value=="military.set_uniform_slot"){
		const auto*p=std::get_if<SetUniformSlotPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");const auto slot=domainSlot(p->slot);if(!slot)return reject("ui.error.uniform_slot_unavailable");const auto material=p->material?QString::fromStdString(p->material->value):QStringLiteral("any");return queue([c=connector_,v=*p,slot=*slot,material]{if(!c)return;auto*military=c->aggregatorMilitary();if(!military)return;military->onSetArmorType(v.role.value,slot,QString::fromStdString(v.type.value),material);c->aggregatorCreatureInfo()->update();});}
	if(a.id.value=="inspect.clear")return queue([c=connector_]{if(!c)return;c->aggregatorCreatureInfo()->onRequestCreatureUpdate(0);c->aggregatorWorkshop()->onCloseWindow();c->aggregatorStockpile()->onCloseWindow();c->aggregatorAgri()->onCloseWindow();});
	if(a.id.value=="tile.execute_context_action"){
		const auto*p=std::get_if<TileContextPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");if(p->action==TileContextAction::Manage)return queue([c=connector_,id=p->tile.value]{if(c)c->onManageCommand(id);});
		const char*cmd=p->action==TileContextAction::Mine?"Mine":p->action==TileContextAction::RemoveFloor?"Remove":p->action==TileContextAction::Harvest?"Harvest":p->action==TileContextAction::FellTree?"Fell":p->action==TileContextAction::RemovePlant?"Destroy":p->action==TileContextAction::CancelJob?"CancelJob":p->action==TileContextAction::RaisePriority?"RaisePrio":p->action==TileContextAction::LowerPriority?"LowerPrio":nullptr;if(!cmd)return reject("ui.error.action_unavailable");return queue([c=connector_,id=p->tile.value,command=QString::fromLatin1(cmd)]{if(c)c->onTerrainCommand(id,command);});}
	if(a.id.value=="tool.cancel"||a.id.value=="tool.rotate"){auto*s=connector_->aggregatorSelection();const bool ok=a.id.value=="tool.cancel"?QMetaObject::invokeMethod(s,&AggregatorSelection::onRightClick,Qt::QueuedConnection):QMetaObject::invokeMethod(s,&AggregatorSelection::onRotateSelection,Qt::QueuedConnection);return ok?CommandResult{CommandStatus::Accepted,true,{}}:reject("ui.error.bridge_queue_failed");}
	if(a.id.value=="workshop.refresh"){const auto*p=std::get_if<WorkshopTargetPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,id=p->workshop.value]{if(c)c->aggregatorWorkshop()->onUpdateWorkshopInfo(id);});}
	if(a.id.value=="workshop.set_basics"){const auto*p=std::get_if<SetWorkshopBasicsPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");const bool linked=p->linkStockpile.value_or(p->connectStockpile.has_value()||workshopLinks_.value(p->workshop.value,false));return queue([c=connector_,v=*p,linked]{if(!c)return;auto*a=c->aggregatorWorkshop();a->onSetBasicOptions(v.workshop.value,QString::fromStdString(v.name),v.priority,v.suspended,v.acceptGenerated,v.autoCraftMissing,linked);a->onUpdateWorkshopInfo(v.workshop.value);});}
	if(a.id.value=="stockpile.refresh"){const auto*p=std::get_if<StockpileTargetPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,id=p->stockpile.value]{if(c)c->aggregatorStockpile()->onUpdateStockpileInfo(id);});}
	if(a.id.value=="stockpile.set_basics"){const auto*p=std::get_if<SetStockpileBasicsPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,v=*p]{if(!c)return;auto*a=c->aggregatorStockpile();a->onSetBasicOptions(v.stockpile.value,QString::fromStdString(v.name),v.priority,v.suspended,v.pull,v.allowPull);a->onUpdateStockpileInfo(v.stockpile.value);});}
	if(a.id.value=="agriculture.refresh"){const auto*p=std::get_if<AgricultureTargetPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,t=p->target]{if(!c)return;auto*a=c->aggregatorAgri();if(t.kind==AgricultureKind::Farm)a->onUpdateFarm(t.designation.value);else if(t.kind==AgricultureKind::Pasture)a->onUpdatePasture(t.designation.value);else a->onUpdateGrove(t.designation.value);});}
	if(a.id.value=="agriculture.set_basics"){const auto*p=std::get_if<SetAgricultureBasicsPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,v=*p]{if(!c)return;auto*a=c->aggregatorAgri();const AgriType k=v.target.kind==AgricultureKind::Farm?AgriType::Farm:v.target.kind==AgricultureKind::Pasture?AgriType::Pasture:AgriType::Grove;a->onSetBasicOptions(k,v.target.designation.value,QString::fromStdString(v.name),v.priority,v.suspended);a->onUpdate(v.target.designation.value);});}
	if(a.id.value=="agriculture.set_harvest_options"){const auto*p=std::get_if<SetHarvestOptionsPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,v=*p]{if(!c)return;auto*a=c->aggregatorAgri();const AgriType k=v.target.kind==AgricultureKind::Farm?AgriType::Farm:AgriType::Pasture;a->onSetHarvestOptions(k,v.target.designation.value,v.harvest,v.harvestHay,v.tame);a->onUpdate(v.target.designation.value);});}
	if(a.id.value=="agriculture.set_grove_options"){const auto*p=std::get_if<SetGroveOptionsPayload>(&a.payload);if(!p)return reject("ui.error.invalid_payload");return queue([c=connector_,v=*p]{if(!c)return;auto*x=c->aggregatorAgri();x->onSetGroveOptions(v.grove.value,v.pick,v.plant,v.fell);x->onUpdateGrove(v.grove.value);});}
	return reject("ui.error.action_unavailable");
}
} // namespace ingnomia::ui::inspector
