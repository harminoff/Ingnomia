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


#include "../base/global.h"
#include "../base/util.h"

#include "../game/game.h"
#include "../game/tutorialmanager.h"
#include "../game/creaturemanager.h"
#include "../game/inventory.h"
#include "../game/gnomemanager.h"
#include "../game/militarymanager.h"

#include "../gfx/spritefactory.h"

#include "../gui/strings.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace
{
void traceInspectorSkills( const QString& message )
{
	if ( !qEnvironmentVariableIsSet( "INGNOMIA_TRACE_INSPECTOR_SKILLS" ) ) return;
	qInfo().noquote() << message;
	const auto path = qEnvironmentVariable( "INGNOMIA_TRACE_INSPECTOR_SKILLS_PATH" );
	if ( path.isEmpty() ) return;
	QFile trace( path );
	if ( !trace.open( QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text ) ) return;
	QTextStream stream( &trace );
	stream << QDateTime::currentDateTime().toString( Qt::ISODateWithMs ) << ' ' << message << '\n';
}
} // namespace

/// @brief Constructs the AggregatorCreatureInfo.
/// @param parent Qt parent object.
AggregatorCreatureInfo::AggregatorCreatureInfo( QObject* parent ) :
	QObject(parent)
{
}

/// @brief Binds the aggregator to a Game instance.
/// @param game Game to bind to.
void AggregatorCreatureInfo::init( Game* game )
{
	g = game;
	m_currentID = 0;
	m_previousID = 0;
	m_info = GuiCreatureInfo{};
	m_gnomeSkillSnapshots.clear();
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
	// Closing an inspector can race with another close request. Once the
	// authoritative selection is already empty, do not emit another clear
	// event and feed the UI close path back into itself.
	if( id == 0 && m_currentID == 0 ) return;
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
	const auto reportAttributes = [this]( const Creature* creature ) {
		static constexpr const char* ids[] = { "Str", "Dex", "Con", "Int", "Wis", "Cha" };
		for ( std::size_t index = 0; index < sizeof( ids ) / sizeof( ids[0] ); ++index )
			m_info.attributesReported[index] = creature && creature->hasAttribute( ids[index] );
	};
	auto gnome = g->gm()->gnome( id );
	if( gnome )
	{
		m_info.name = gnome->name();
		m_info.id = id;
		m_info.position = gnome->getPos().toString();
		m_info.profession = gnome->profession();
		m_info.professionReported = true;
		m_info.skillsReported = true;
		m_info.equipmentReported = true;
		reportAttributes( gnome );

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
		// Build rows only from this gnome's serialized skill map and priority list.
		// A global catalog here would make every inspector display the same skills,
		// and a refresh for one gnome could make another window appear blank.
		const auto availableSkillIDs = gnome->availableSkillIDs();
		const auto prioritySkillIDs = gnome->skillPrios();
		QStringList skillIds;
		const auto appendSkill = [&skillIds]( const QString& skillID )
		{
			if ( !skillID.isEmpty() && !skillIds.contains( skillID ) ) skillIds.append( skillID );
		};
		for ( const auto& skillID : availableSkillIDs ) appendSkill( skillID );
		for ( const auto& skillID : prioritySkillIDs ) appendSkill( skillID );
		for ( const auto& skillID : skillIds )
		{
			GuiCreatureInfo::Skill skill;
			skill.id = skillID;
			skill.name = S::s( "$SkillName_" + skillID );
			if ( skill.name.isEmpty() ) skill.name = skillID;
			skill.level = gnome->getSkillLevel( skillID );
			skill.active = gnome->getSkillActive( skillID );
			m_info.skills.append( skill );
		}
		bool usedSkillSnapshot = false;
		if ( !m_info.skills.isEmpty() )
			m_gnomeSkillSnapshots.insert( id, m_info.skills );
		else if ( m_gnomeSkillSnapshots.contains( id ) )
		{
			m_info.skills = m_gnomeSkillSnapshots.value( id );
			usedSkillSnapshot = true;
		}
		traceInspectorSkills( QStringLiteral( "producer id=%1 name=%2 available=%3 priorities=%4 unique=%5 rows=%6 snapshot=%7 first=%8 last=%9" )
			.arg( static_cast<qulonglong>( id ) )
			.arg( gnome->name() )
			.arg( availableSkillIDs.size() )
			.arg( prioritySkillIDs.size() )
			.arg( skillIds.size() )
			.arg( m_info.skills.size() )
			.arg( usedSkillSnapshot ? QStringLiteral( "true" ) : QStringLiteral( "false" ) )
			.arg( skillIds.isEmpty() ? QStringLiteral( "-" ) : skillIds.front() )
			.arg( skillIds.isEmpty() ? QStringLiteral( "-" ) : skillIds.back() ) );

		if( gnome->roleID() )
		{
			m_info.roleID = gnome->roleID();
			m_info.roleName = g->mil()->roleName( gnome->roleID() );
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
			reportAttributes( monster );

			m_info.str = monster->attribute( "Str" );
			m_info.con = monster->attribute( "Con" );
			m_info.dex = monster->attribute( "Dex" );
			m_info.intel = monster->attribute( "Int" );
			m_info.wis = monster->attribute( "Wis" );
			m_info.cha = monster->attribute( "Cha" );

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
				// Animals currently expose hunger only. Their internal creature
				// attributes are not player-facing inspection data, so do not project
				// empty/null attribute cells into the profile.
				m_info.hunger = animal->hunger();
				m_info.needsReported[0] = true;

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
	// Only gnomes can receive a profession assignment. Avoid rebuilding the
	// complete profession choice list for animals and monsters; detached
	// inspectors do not need that hidden DOM and it can make selection appear
	// to stall on large profession catalogs.
	if( !g || !g->gm()->gnome( m_currentID ) ) return;
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
			if( g->tutorial() ) g->tutorial()->observeProfession( gnomeID, gnome->profession() );
			//onUpdateSingleGnome( gnomeID );
		}
	}
}
