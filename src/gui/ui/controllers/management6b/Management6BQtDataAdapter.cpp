/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6BQtDataAdapter.h"
#include "../../../aggregatorinventory.h"
#include "../../../aggregatorcreatureinfo.h"
#include "../../../aggregatorinventory.h"
#include "../../../aggregatorpopulation.h"
#include <algorithm>

namespace ingnomia::ui::management6b
{
namespace
{
std::string text(const QString&v){const auto bytes=v.toUtf8();return {bytes.constData(),static_cast<std::size_t>(bytes.size())};}
std::string displayName(const QString&value,const QString&fallback){auto out=text(value);if(!out.empty()&&out.rfind("Error:",0)!=0&&out.front()!='$')return out;out=text(fallback);const auto marker=out.find('_');if(marker!=std::string::npos&&out.front()=='$')out.erase(0,marker+1);std::replace(out.begin(),out.end(),'_',' ');return out.empty()?"(unnamed)":out;}
PopulationRow populationRow(const GuiGnomeInfo&i){PopulationRow o;o.id=CreatureId{i.id};o.name=text(i.name);o.profession=ProfessionId{text(i.profession)};for(const auto&s:i.skills)o.skills.push_back({CatalogId{text(s.sid)},text(s.name),text(s.group),s.level,s.xpValue,s.active});return o;}
ManagedScheduleActivity activity(::ScheduleActivity a){switch(a){case ::ScheduleActivity::None:return ManagedScheduleActivity::None;case ::ScheduleActivity::Eat:return ManagedScheduleActivity::Eat;case ::ScheduleActivity::Sleep:return ManagedScheduleActivity::Sleep;case ::ScheduleActivity::Training:return ManagedScheduleActivity::Training;}return ManagedScheduleActivity::None;}
ScheduleRow scheduleRow(const GuiGnomeScheduleInfo&i){ScheduleRow o;o.creature=CreatureId{i.id};o.name=text(i.name);for(int n=0;n<24&&n<i.schedule.size();++n)o.hours[static_cast<std::size_t>(n)]=activity(i.schedule[n]);return o;}
InventoryRowId rowId(const QString&cat,const QString&group,const QString&item,const QString&material,InventoryDepth depth){return{CatalogId{text(cat)},CatalogId{text(group)},CatalogId{text(item)},CatalogId{text(material)},depth};}
template<class I> void totals(InventoryRow&o,const I&i){o.total=i.countTotal;o.inJobs=i.countInJob;o.stockpiled=i.countInStockpiles;o.equipped=i.countEquipped;o.constructed=i.countConstructed;o.loose=i.countLoose;o.totalValue=i.totalValue;o.watched=i.watched;}
void slot(std::vector<EquipmentSlotRow>&out,const char*name,const EquipmentItem&i){if(i.itemID)out.push_back({name,text(i.item),text(i.material)});}
}
void Management6BQtDataAdapter::setWorld(WorldEpoch w){world_=w;populationRevision_={};professionRevision_={};scheduleRevision_={};inventoryRevision_={};creatureRevision_={};}
Snapshot<std::vector<PopulationRow>> Management6BQtDataAdapter::population(const GuiPopulationInfo&i){std::vector<PopulationRow> rows;rows.reserve(i.gnomes.size());for(const auto&r:i.gnomes)rows.push_back(populationRow(r));return{world_,Revision{++populationRevision_.value},std::move(rows)};}
RowPatch<PopulationRow> Management6BQtDataAdapter::populationPatch(const GuiGnomeInfo&i){const auto base=populationRevision_;return{world_,base,Revision{++populationRevision_.value},populationRow(i)};}
Snapshot<std::vector<ProfessionRow>> Management6BQtDataAdapter::professions(const QStringList&i){std::vector<ProfessionRow> rows;rows.reserve(i.size());for(const auto&name:i)rows.push_back({ProfessionId{text(name)},text(name),{}});return{world_,Revision{++professionRevision_.value},std::move(rows)};}
std::pair<ProfessionId,std::vector<CatalogId>> Management6BQtDataAdapter::professionSkills(const QString&profession,const QList<GuiSkillInfo>&skills)const{std::vector<CatalogId> out;out.reserve(skills.size());for(const auto&s:skills)if(s.active)out.push_back(CatalogId{text(s.sid)});return{ProfessionId{text(profession)},std::move(out)};}
Snapshot<std::vector<ScheduleRow>> Management6BQtDataAdapter::schedules(const GuiScheduleInfo&i){std::vector<ScheduleRow> rows;rows.reserve(i.schedules.size());for(const auto&r:i.schedules)rows.push_back(scheduleRow(r));return{world_,Revision{++scheduleRevision_.value},std::move(rows)};}
RowPatch<ScheduleRow> Management6BQtDataAdapter::schedulePatch(const GuiGnomeScheduleInfo&i){const auto base=scheduleRevision_;return{world_,base,Revision{++scheduleRevision_.value},scheduleRow(i)};}
Snapshot<std::vector<InventoryRow>> Management6BQtDataAdapter::inventory(const QList<GuiInventoryCategory>&i){std::vector<InventoryRow> rows;for(const auto&c:i){InventoryRow cr;cr.id=rowId(c.id,{},{},{},InventoryDepth::Category);cr.name=displayName(c.name,c.id);totals(cr,c);rows.push_back(cr);for(const auto&g:c.groups){InventoryRow gr;gr.id=rowId(c.id,g.id,{},{},InventoryDepth::Group);gr.name=displayName(g.name,g.id);totals(gr,g);rows.push_back(gr);for(const auto&it:g.items){InventoryRow ir;ir.id=rowId(c.id,g.id,it.id,{},InventoryDepth::Item);ir.name=displayName(it.name,it.id);totals(ir,it);ir.spriteSheet=text(it.spriteSheet);ir.spriteX=it.spriteX;ir.spriteY=it.spriteY;ir.spriteWidth=it.spriteWidth;ir.spriteHeight=it.spriteHeight;ir.spriteSheetWidth=it.spriteSheetWidth;ir.spriteSheetHeight=it.spriteSheetHeight;rows.push_back(ir);for(const auto&m:it.materials){InventoryRow mr;mr.id=rowId(c.id,g.id,it.id,m.id,InventoryDepth::Material);mr.name=displayName(m.name,m.id)+" "+displayName(it.name,it.id);totals(mr,m);mr.spriteSheet=text(m.spriteSheet);mr.spriteX=m.spriteX;mr.spriteY=m.spriteY;mr.spriteWidth=m.spriteWidth;mr.spriteHeight=m.spriteHeight;mr.spriteSheetWidth=m.spriteSheetWidth;mr.spriteSheetHeight=m.spriteSheetHeight;rows.push_back(mr);}}}}return{world_,Revision{++inventoryRevision_.value},std::move(rows)};}
Snapshot<CreatureDetail> Management6BQtDataAdapter::creature(const GuiCreatureInfo&i){CreatureDetail o;o.id=CreatureId{i.id};o.name=text(i.name);o.profession=text(i.profession);const auto a=text(i.activity);if(!a.empty())o.activity=a;o.strength=i.str;o.dexterity=i.dex;o.constitution=i.con;o.intelligence=i.intel;o.wisdom=i.wis;o.charisma=i.cha;o.hunger=i.hunger;o.thirst=i.thirst;o.sleep=i.sleep;o.happiness=i.happiness;slot(o.equipment,"Head",i.equipment.head);slot(o.equipment,"Chest",i.equipment.chest);slot(o.equipment,"Arms",i.equipment.arm);slot(o.equipment,"Hands",i.equipment.hand);slot(o.equipment,"Legs",i.equipment.leg);slot(o.equipment,"Feet",i.equipment.foot);slot(o.equipment,"Left hand",i.equipment.leftHandHeld);slot(o.equipment,"Right hand",i.equipment.rightHandHeld);slot(o.equipment,"Back",i.equipment.back);return{world_,Revision{++creatureRevision_.value},std::move(o)};}
std::vector<InventoryHistoryPoint> Management6BQtDataAdapter::inventoryHistory(const QList<GuiInventoryHistoryPoint>& points) const
{
	std::vector<InventoryHistoryPoint> out;
	out.reserve( static_cast<std::size_t>( points.size() ) );
	for ( const auto& p : points )
		out.push_back( InventoryHistoryPoint { p.dayIndex, p.total, p.created, p.destroyed } );
	return out;
}
} // namespace ingnomia::ui::management6b
