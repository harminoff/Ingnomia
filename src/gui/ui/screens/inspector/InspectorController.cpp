/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "InspectorController.h"

#include <algorithm>

namespace ingnomia::ui::inspector
{
InspectorController::InspectorController( InspectorCommandPort& commands, InspectorViewPort& view ) : commands_( commands ), view_( view ) { notify(); }
void InspectorController::notify() { ++state_.revision.value; view_.stateChanged( state_ ); }
void InspectorController::beginWorld( WorldEpoch world ) { state_ = {}; state_.world = world; state_.acceptsWorldActions = static_cast<bool>( world ); notify(); }
void InspectorController::endWorld() { state_ = {}; notify(); }
void InspectorController::select( EntityRef ref, InspectorKind kind ) { if( state_.selected != ref ) state_.previous = state_.selected; state_.selected=std::move(ref); state_.kind=kind; state_.status.clear(); }
void InspectorController::showTile( TileInspectorState value ) { if(!state_.acceptsWorldActions||!value.id)return; select({state_.world,EntityKind::Tile,value.id.value,value.position},InspectorKind::Tile);state_.tile=std::move(value);state_.creature.reset();state_.workshop.reset();state_.stockpile.reset();state_.agriculture.reset();state_.professionChoices.clear();state_.creatureStatsOpen=false;state_.creatureSkillsOpen=false;state_.creatureDetailsOpen=false;notify(); }
void InspectorController::showCreature( CreatureInspectorState value, std::optional<WorldPosition> position )
{
	if( !state_.acceptsWorldActions || !value.id ) return;
	const bool sameCreature = state_.kind == InspectorKind::Creature && state_.creature && state_.creature->id == value.id;
	if( sameCreature )
	{
		// Live selection refreshes can arrive as a partial payload after a map
		// click. Keep stable profile data that was already shown instead of
		// turning an open Professions & Skills panel into an empty panel.
		const auto& previous = *state_.creature;
		if( value.skills.empty() && !previous.skills.empty() ) value.skills = previous.skills;
		if( !value.skillsReported && previous.skillsReported ) value.skillsReported = true;
		if( value.profession.empty() && !previous.profession.empty() ) value.profession = previous.profession;
		if( !value.professionReported && previous.professionReported ) value.professionReported = true;
		if( !value.equipmentReported && previous.equipmentReported ) value.equipmentReported = true;
		if( !value.inventoryReported && previous.inventoryReported ) value.inventoryReported = true;
		for( std::size_t index = 0; index < value.attributesReported.size(); ++index )
			if( !value.attributesReported[index] && previous.attributesReported[index] )
			{
				value.attributesReported[index] = true;
				switch( index )
				{
					case 0: value.strength = previous.strength; break;
					case 1: value.dexterity = previous.dexterity; break;
					case 2: value.constitution = previous.constitution; break;
					case 3: value.intelligence = previous.intelligence; break;
					case 4: value.wisdom = previous.wisdom; break;
					case 5: value.charisma = previous.charisma; break;
				}
			}
		for( std::size_t index = 0; index < value.needsReported.size(); ++index )
			if( !value.needsReported[index] && previous.needsReported[index] )
			{
				value.needsReported[index] = true;
				switch( index )
				{
					case 0: value.hunger = previous.hunger; break;
					case 1: value.thirst = previous.thirst; break;
					case 2: value.sleep = previous.sleep; break;
					case 3: value.happiness = previous.happiness; break;
				}
			}
	}
	select( { state_.world, EntityKind::Creature, value.id.value, position }, InspectorKind::Creature );
	state_.tile.reset();
	state_.creature = std::move( value );
	state_.workshop.reset();
	state_.stockpile.reset();
	state_.agriculture.reset();
	if( !sameCreature )
	{
		state_.professionChoices.clear();
		state_.creatureStatsOpen = false;
		state_.creatureSkillsOpen = false;
		state_.creatureDetailsOpen = false;
		state_.equipmentSlotEditor.reset();
		state_.equipmentDraftType = {};
		state_.equipmentDraftMaterial.reset();
	}
	notify();
}
void InspectorController::setProfessionChoices( std::vector<std::string> value ) { state_.professionChoices=std::move(value);notify(); }
void InspectorController::setProfession( std::string value ) { if(state_.kind==InspectorKind::Creature&&state_.creature&&state_.creature->id&&!value.empty())dispatch("population.set_profession",SetProfessionPayload{state_.creature->id,ProfessionId{std::move(value)}}); }
void InspectorController::showWorkshop( WorkshopInspectorState value ) { if(!state_.acceptsWorldActions||!value.id)return;auto pos=state_.selected?state_.selected->position:std::nullopt;select({state_.world,EntityKind::Workshop,value.id.value,pos},InspectorKind::Workshop);state_.workshop=std::move(value);notify(); }
void InspectorController::showStockpile( StockpileInspectorState value ) { if(!state_.acceptsWorldActions||!value.id)return;auto pos=state_.selected?state_.selected->position:std::nullopt;select({state_.world,EntityKind::Stockpile,value.id.value,pos},InspectorKind::Stockpile);state_.stockpile=std::move(value);notify(); }
void InspectorController::showAgriculture( AgricultureInspectorState value ) { if(!state_.acceptsWorldActions||!value.target.designation)return;auto pos=state_.selected?state_.selected->position:std::nullopt;const auto kind=value.target.kind==AgricultureKind::Farm?EntityKind::Farm:value.target.kind==AgricultureKind::Pasture?EntityKind::Pasture:EntityKind::Grove;select({state_.world,kind,value.target.designation.value,pos},InspectorKind::Agriculture);state_.agriculture=std::move(value);notify(); }
void InspectorController::setSelectionAction( std::string action, bool canRotate ){state_.selection.actionLabel=std::move(action);state_.selection.active=!state_.selection.actionLabel.empty();state_.selection.canRotate=state_.selection.active&&canRotate;if(!state_.selection.active){state_.selection.anchor.reset();state_.selection.sizeLabel.clear();}notify();}
void InspectorController::setSelectionCursor( std::optional<WorldPosition> v ){state_.selection.cursor=v;notify();}
void InspectorController::setSelectionAnchor( std::optional<WorldPosition> v ){state_.selection.anchor=v;notify();}
void InspectorController::setSelectionSize( std::string v ){state_.selection.sizeLabel=std::move(v);notify();}
void InspectorController::setSelectionPointer( std::optional<PointerPosition> v ){if(state_.selection.pointer==v)return;state_.selection.pointer=std::move(v);if(state_.selection.active)notify();}
bool InspectorController::dispatch(std::string_view id,UiActionPayload payload){if(!state_.acceptsWorldActions||!state_.world)return false;UiActionEnvelope a{ActionId{id},RequestId{nextRequest_++},state_.world,std::nullopt,std::move(payload)};auto r=commands_.dispatch(a);if(r.status==CommandStatus::Rejected){state_.status=r.error;notify();return false;}state_.status.clear();if(r.pending)state_.pendingAction=a.request;notify();return true;}
void InspectorController::openCreature(CreatureId id){if(!id)return;dispatch("inspect.select",SelectPayload{{state_.world,EntityKind::Creature,id.value,state_.selected?state_.selected->position:std::nullopt}});}
void InspectorController::toggleCreatureStats(){if(state_.kind==InspectorKind::Creature&&state_.creature&&!state_.creatureDetailsOpen){state_.creatureStatsOpen=!state_.creatureStatsOpen;if(state_.creatureStatsOpen)state_.creatureSkillsOpen=false;notify();}}
void InspectorController::toggleCreatureSkills(){if(state_.kind==InspectorKind::Creature&&state_.creature&&!state_.creatureDetailsOpen){state_.creatureSkillsOpen=!state_.creatureSkillsOpen;if(state_.creatureSkillsOpen)state_.creatureStatsOpen=false;notify();}}
void InspectorController::toggleCreatureDetails(){if(state_.kind==InspectorKind::Creature&&state_.creature){state_.creatureDetailsOpen=!state_.creatureDetailsOpen;if(state_.creatureDetailsOpen){state_.creatureStatsOpen=false;state_.creatureSkillsOpen=false;}notify();}}
void InspectorController::selectEquipmentSlot( UniformSlot slot )
{
	if( state_.kind != InspectorKind::Creature || !state_.creature || !state_.creature->equipmentRole )
	{
		state_.status = "Assign a military role before changing equipment.";
		notify();
		return;
	}
	const auto& equipmentValues = state_.creature->equipmentSlots;
	const auto found = std::find_if( equipmentValues.begin(), equipmentValues.end(), [slot]( const EquipmentSlotState& value ) { return value.slot == slot; } );
	if( found == equipmentValues.end() ) return;
	state_.equipmentSlotEditor = slot;
	state_.equipmentDraftType = found->desiredType;
	state_.equipmentDraftMaterial = found->desiredMaterial;
	state_.status.clear();
	notify();
}
void InspectorController::setEquipmentDraftType( CatalogId type )
{
	if( !state_.equipmentSlotEditor || !state_.creature || type.value.empty() ) return;
	const auto& equipmentValues = state_.creature->equipmentSlots;
	const auto slot = std::find_if( equipmentValues.begin(), equipmentValues.end(), [this]( const EquipmentSlotState& value ) { return value.slot == *state_.equipmentSlotEditor; } );
	if( slot == equipmentValues.end() || std::none_of( slot->choices.begin(), slot->choices.end(), [&type]( const EquipmentTypeChoice& value ) { return value.type == type; } ) ) return;
	state_.equipmentDraftType = std::move( type );
	state_.equipmentDraftMaterial = CatalogId{ "any" };
	notify();
}
void InspectorController::setEquipmentDraftMaterial( CatalogId material )
{
	if( !state_.equipmentSlotEditor || !state_.creature || material.value.empty() ) return;
	const auto& equipmentValues = state_.creature->equipmentSlots;
	const auto slot = std::find_if( equipmentValues.begin(), equipmentValues.end(), [this]( const EquipmentSlotState& value ) { return value.slot == *state_.equipmentSlotEditor; } );
	if( slot == equipmentValues.end() ) return;
	const auto choice = std::find_if( slot->choices.begin(), slot->choices.end(), [this]( const EquipmentTypeChoice& value ) { return value.type == state_.equipmentDraftType; } );
	if( choice == slot->choices.end() || std::none_of( choice->materials.begin(), choice->materials.end(), [&material]( const CatalogId& value ) { return value == material; } ) ) return;
	state_.equipmentDraftMaterial = std::move( material );
	notify();
}
void InspectorController::applyEquipmentSlot()
{
	if( !state_.equipmentSlotEditor || !state_.creature || !state_.creature->equipmentRole || state_.equipmentDraftType.value.empty() ) return;
	const auto creature = state_.creature->id;
	const auto accepted = dispatch( "military.set_uniform_slot", SetUniformSlotPayload{ state_.creature->equipmentRole, *state_.equipmentSlotEditor, state_.equipmentDraftType, state_.equipmentDraftMaterial } );
	if( accepted )
	{
		state_.equipmentSlotEditor.reset();
		state_.equipmentDraftType = {};
		state_.equipmentDraftMaterial.reset();
		state_.status = "Uniform updated; the gnome will swap equipment when the item is available.";
		dispatch( "inspect.select", SelectPayload{ { state_.world, EntityKind::Creature, creature.value, state_.selected ? state_.selected->position : std::nullopt } } );
	}
}
void InspectorController::closeEquipmentEditor(){if(state_.equipmentSlotEditor){state_.equipmentSlotEditor.reset();state_.equipmentDraftType={};state_.equipmentDraftMaterial.reset();notify();}}
void InspectorController::executeContext(TileContextAction action){if(state_.tile && (action!=TileContextAction::DeleteStockpile || state_.tile->canDeleteStockpile))dispatch("tile.execute_context_action",TileContextPayload{state_.tile->id,action});}
void InspectorController::toggleWorkshopSuspended(){if(!state_.workshop)return;const auto&s=*state_.workshop;dispatch("workshop.set_basics",SetWorkshopBasicsPayload{s.id,s.name,s.priority,!s.suspended,s.acceptGenerated,s.autoCraftMissing,std::nullopt});}
void InspectorController::toggleStockpileSuspended(){if(!state_.stockpile)return;const auto&s=*state_.stockpile;dispatch("stockpile.set_basics",SetStockpileBasicsPayload{s.id,s.name,s.priority,!s.suspended,s.pullFromOthers,s.allowPullFromHere});}
void InspectorController::toggleAgricultureSuspended(){if(!state_.agriculture)return;const auto&s=*state_.agriculture;dispatch("agriculture.set_basics",SetAgricultureBasicsPayload{s.target,s.name,s.priority,!s.suspended});}
void InspectorController::toggleAgriculturePrimaryOption(){if(!state_.agriculture)return;const auto&s=*state_.agriculture;if(s.target.kind==AgricultureKind::Grove)dispatch("agriculture.set_grove_options",SetGroveOptionsPayload{s.target.designation,!s.pick,s.plant,s.fell});else dispatch("agriculture.set_harvest_options",SetHarvestOptionsPayload{s.target,!s.harvest,s.harvestHay,s.tame});}
void InspectorController::refresh(){if(state_.workshop)dispatch("workshop.refresh",WorkshopTargetPayload{state_.workshop->id});else if(state_.stockpile)dispatch("stockpile.refresh",StockpileTargetPayload{state_.stockpile->id});else if(state_.agriculture)dispatch("agriculture.refresh",AgricultureTargetPayload{state_.agriculture->target});else if(state_.selected)dispatch("inspect.select",SelectPayload{*state_.selected});}
void InspectorController::locate(){if(state_.selected&&(state_.selected->position||state_.selected->id))dispatch("view.center_on",CenterPayload{*state_.selected});}
void InspectorController::cancelSelection(){dispatch("tool.cancel",NoPayload{});}
void InspectorController::rotateSelection(){if(state_.selection.canRotate)dispatch("tool.rotate",NoPayload{});}
void InspectorController::close(){if(dispatch("inspect.clear",NoPayload{})){state_.kind=InspectorKind::None;state_.selected.reset();state_.tile.reset();state_.creature.reset();state_.workshop.reset();state_.stockpile.reset();state_.agriculture.reset();state_.creatureStatsOpen=false;state_.creatureSkillsOpen=false;state_.creatureDetailsOpen=false;notify();}}
void InspectorController::back(){if(state_.kind==InspectorKind::Creature&&state_.creatureDetailsOpen){state_.creatureDetailsOpen=false;notify();}else if(state_.kind==InspectorKind::Creature&&state_.previous){auto p=*state_.previous;dispatch("inspect.select",SelectPayload{p});}else close();}
void InspectorController::onActionFinished(RequestId id,CommandResult r){if(state_.pendingAction!=id)return;state_.pendingAction.reset();state_.status=r.status==CommandStatus::Rejected?r.error:std::string{};notify();}
} // namespace ingnomia::ui::inspector
