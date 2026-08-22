/*
	This file is part of Ingnomia https://github.com/rschurade/Ingnomia
    Copyright (C) 2017-2020  Ralph Schurade, Ingnomia Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
/** @file aggregatorcreatureinfo.cpp
 *  @brief AggregatorCreatureInfo implementation: fills GuiCreatureInfo for gnomes, monsters,
 *         and animals for the RmlUi view model to consume.
 */
#include "aggregatorcreatureinfo.h"

#include "../base/db.h"
#include "../base/dbhelper.h"
#include "../base/global.h"
#include "../base/util.h"

#include "../game/game.h"
#include "../game/creaturemanager.h"
#include "../game/inventory.h"
#include "../game/gnomemanager.h"
#include "../game/militarymanager.h"

#include "../gfx/spritefactory.h"

#include "../gui/strings.h"

/// @brief Constructs the AggregatorCreatureInfo.
/// @param parent Qt parent object.
AggregatorCreatureInfo::AggregatorCreatureInfo( QObject* parent ) :
	QObject(parent)
{
	for ( const auto& group : DB::selectRows( "SkillGroups" ) )
		for ( const auto& skillID : group.value( "SkillID" ).toString().split( "|" ) )
			if ( !skillID.isEmpty() ) m_skillIds.append( skillID );
}

/// @brief Binds the aggregator to a Game instance.
/// @param game Game to bind to.
void AggregatorCreatureInfo::init( Game* game )
{
	g = game;
}

/// @brief Re-sends the current creature payload if a creature is currently being displayed.
void AggregatorCreatureInfo::update()
{
	if( m_currentID == 0 )
		return;

	// This method is called from the game loop. Rebuilding all skills, equipment,
	// and carried-item designations every tick can starve both simulation and UI.
	// Creature needs/activity remain live, but four updates per second is enough
	// for an inspection panel and keeps the work off the critical tick path.
	constexpr qint64 refreshIntervalMs = 250;
	if( m_lastUpdate.isValid() && m_lastUpdate.elapsed() < refreshIntervalMs )
		return;

	m_lastUpdate.restart();
	onRequestCreatureUpdate( m_currentID );
}

/// @brief Fills GuiCreatureInfo from the given creature (gnome first, then monster, then
///        animal) and emits signalCreatureUpdate. For gnomes it also rebuilds the per-slot
///        equipment data when the equipment has changed or the creature has switched.
/// @param id Creature UID to display.
void AggregatorCreatureInfo::onRequestCreatureUpdate( unsigned int id )
{
	if( !g ) return;
	m_lastUpdate.restart();
	// Rebuild every payload from the selected creature. In particular, a gnome's
	// equipment/uniform must not leak into a subsequent monster or animal update.
	m_info = GuiCreatureInfo{};
	m_currentID = id;
	const auto reportInventory = [this]( const Creature* creature ) {
		m_info.inventoryReported = true;
		if ( !creature || !g || !g->inv() ) return;
		for ( const auto itemID : creature->inventoryItems() )
		{
			const auto designation = g->inv()->designation( itemID );
			if ( !designation.isEmpty() ) m_info.inventory.append( designation );
		}
	};
	auto gnome = g->gm()->gnome( id );
	if( gnome )
	{
		m_info.name = gnome->name();
		m_info.id = id;
		m_info.position = gnome->getPos().toString();
		m_info.profession = gnome->profession();

		m_info.str = gnome->attribute( "Str" );
		m_info.con = gnome->attribute( "Con" );
		m_info.dex = gnome->attribute( "Dex" );
		m_info.intel = gnome->attribute( "Int" );
		m_info.wis = gnome->attribute( "Wis" );
		m_info.cha = gnome->attribute( "Cha" );

		m_info.hunger = gnome->need( "Hunger" );
		m_info.thirst = gnome->need( "Thirst" );
		m_info.sleep = gnome->need( "Sleep" );
		m_info.happiness = gnome->need( "Happiness" );
		m_info.needsReported.fill( true );

		m_info.activity = gnome->getActivity();
		for ( const auto& skillID : m_skillIds )
		{
			GuiCreatureInfo::Skill skill;
			skill.id = skillID;
			skill.name = S::s( "$SkillName_" + skillID );
			skill.level = gnome->getSkillLevel( skillID );
			skill.active = gnome->getSkillActive( skillID );
			m_info.skills.append( skill );
		}

		if( gnome->roleID() )
		{
			m_info.uniform = g->mil()->uniformCopy( gnome->roleID() );
		}
		m_info.equipment = gnome->equipment();
		reportInventory( gnome );

		if( m_previousID != m_currentID || gnome->equipmentChanged() )
		{
			m_previousID = m_currentID;

		}

		emit signalCreatureUpdate( m_info );
		return;
	}
	else
	{
		auto monster = g->cm()->monster( id );
		if( monster )
		{
			m_info.name = monster->name();
			m_info.id = id;
			m_info.position = monster->getPos().toString();
			//m_info.profession = monster->profession();

			m_info.str = monster->attribute( "Str" );
			m_info.con = monster->attribute( "Con" );
			m_info.dex = monster->attribute( "Dex" );
			m_info.intel = monster->attribute( "Int" );
			m_info.wis = monster->attribute( "Wis" );
			m_info.cha = monster->attribute( "Cha" );
			reportInventory( monster );

			emit signalCreatureUpdate( m_info );
			return;
		}
		else
		{
			auto animal = g->cm()->animal( id );
			if( animal )
			{
				m_info.name = animal->name();
				m_info.id = id;
				m_info.position = animal->getPos().toString();
				//m_info.profession = animal->profession();

				m_info.str = animal->attribute( "Str" );
				m_info.con = animal->attribute( "Con" );
				m_info.dex = animal->attribute( "Dex" );
				m_info.intel = animal->attribute( "Int" );
				m_info.wis = animal->attribute( "Wis" );
				m_info.cha = animal->attribute( "Cha" );

				m_info.hunger = animal->hunger();
				m_info.needsReported[0] = true;
				reportInventory( animal );

				emit signalCreatureUpdate( m_info );
				return;
			}
		}

	}
	m_currentID = 0;
	m_info = GuiCreatureInfo{};
	emit signalCreatureCleared();
}


/// @brief Emits the list of available profession names to the GUI.
void AggregatorCreatureInfo::onRequestProfessionList()
{
	if( !g ) return;
	emit signalProfessionList( g->gm()->professions() );
}

/// @brief Assigns a new profession to the given gnome (no-op if it's the current profession).
/// @param gnomeID    Creature UID of the gnome.
/// @param profession New profession name.
void AggregatorCreatureInfo::onSetProfession( unsigned int gnomeID, QString profession )
{
	if( !g ) return;
	auto gnome = g->gm()->gnome( gnomeID );
	if( gnome )
	{
		QString oldProf = gnome->profession();
		if( oldProf != profession )
		{
			gnome->selectProfession( profession );
			//onUpdateSingleGnome( gnomeID );
		}
	}
}
