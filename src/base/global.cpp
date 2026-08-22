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
/** @file global.cpp
 * @brief Implementation of the Global static class: initialization and reset.
 */

#include "global.h"

#include "../base/config.h"
#include "../base/db.h"
#include "../base/gamestate.h"
#include "../base/io.h"
#include "../base/logger.h"
#include "../base/util.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

EventConnector* Global::eventConnector = nullptr;
Util* Global::util = nullptr;
Selection* Global::sel = nullptr;
NewGameSettings* Global::newGameSettings = nullptr;
Config* Global::cfg = nullptr;

Logger Global::m_logger;

bool Global::wallsLowered = false;
bool Global::showAxles    = false;
bool Global::showDesignations = true;
bool Global::showJobs = true;

unsigned int Global::waterSpriteUID  = 0;
unsigned int Global::undiscoveredUID = 0;

QVariantMap Global::copiedStockpileSettings;

QMap<QString, QVariantMap> Global::m_windowParams;

QMap<QString, QSet<QString>> Global::allowedInContainer;

int Global::dimX     = 100;
int Global::dimY     = 100;
int Global::dimZ     = 100;

int Global::zWeight = 20;

double Global::xpMod = 250.;

bool Global::debugMode = false;
bool Global::debugOpenGL = false;
bool Global::debugSound = false;
float Global::debugNeedDecayMultiplier = 1.0f;
QSet<QString> Global::disabledNeedDecays;

QMap<QString, QDomElement> Global::m_behaviorTrees;

QStringList Global::needIDs;
QMap<QString, float> Global::needDecays;

unsigned int Global::dirtUID = 0;

QMap<QString, CreaturePart> Global::creaturePartLookUp;
QMap<CreaturePart, QString> Global::creaturePartToString;

QSet<QString> Global::craftable;

/**
 * @brief Resets all global state for a new game or load.
 *
 * Clears overlays, reloads XP modifier from config, resets the logger,
 * reloads all behavior trees from content/ai, rebuilds the gnome need
 * tables from the Needs DB table, populates the creature part lookup maps,
 * and fills the craftable items set from the Crafts DB table.
 */
void Global::reset()
{
	qDebug() << "*** Global reset";

	GameState::stockOverlay.clear();
	GameState::squads.clear();

	Global::xpMod = Global::cfg->get( "XpMod" ).toDouble();

	m_logger.reset();

	wallsLowered = false;
	showAxles    = false;

	//m_keyBindings.update();

	if ( !loadBehaviorTrees() )
	{
		qCritical() << "failed to load behavior trees";
		abort();
	}

	needIDs.clear();
	needDecays.clear();

	for ( auto row : DB::selectRows( "Needs" ) )
	{
		if ( row.value( "Creature" ).toString() == "Gnome" )
		{
			auto need = row.value( "ID" ).toString();
			needIDs.append( need );
			needDecays.insert( need, row.value( "DecayPerMinute" ).toFloat() );
		}
	}

	Global::dirtUID = DBH::materialUID( "Dirt" );

	Global::cfg->set( "renderCreatures", true );

	creaturePartLookUp.insert( "Head", CP_HEAD );
	creaturePartLookUp.insert( "Torso", CP_TORSO );
	creaturePartLookUp.insert( "LeftArm", CP_LEFT_ARM );
	creaturePartLookUp.insert( "RightArm", CP_RIGHT_ARM );
	creaturePartLookUp.insert( "LeftHand", CP_LEFT_HAND );
	creaturePartLookUp.insert( "RightHand", CP_RIGHT_HAND );
	creaturePartLookUp.insert( "LeftLeg", CP_LEFT_LEG );
	creaturePartLookUp.insert( "RightLeg", CP_RIGHT_LEG );
	creaturePartLookUp.insert( "LeftFoot", CP_LEFT_FOOT );
	creaturePartLookUp.insert( "RightFoot", CP_RIGHT_FOOT );

	creaturePartLookUp.insert( "LeftFrontLeg", CP_LEFT_FRONT_LEG );
	creaturePartLookUp.insert( "LegFrontLeg", CP_RIGHT_FRONT_LEG );
	creaturePartLookUp.insert( "LeftFrontFoot", CP_LEFT_FRONT_FOOT );
	creaturePartLookUp.insert( "RightFrontFoot", CP_RIGHT_FRONT_FOOT );

	creaturePartLookUp.insert( "LeftWing", CP_RIGHT_WING );
	creaturePartLookUp.insert( "RightWing", CP_RIGHT_WING );

	creaturePartLookUp.insert( "Brain", CP_BRAIN );
	creaturePartLookUp.insert( "LeftEye", CP_LEFT_EYE );
	creaturePartLookUp.insert( "RightEye", CP_RIGHT_EYE );
	creaturePartLookUp.insert( "Heart", CP_HEART );
	creaturePartLookUp.insert( "LeftLung", CP_LEFT_LUNG );
	creaturePartLookUp.insert( "RightLung", CP_RIGHT_LUNG );

	creaturePartLookUp.insert( "Hair", CP_HAIR );
	creaturePartLookUp.insert( "Facial", CP_FACIAL_HAIR );
	creaturePartLookUp.insert( "Clothing", CP_CLOTHING );
	creaturePartLookUp.insert( "Boots", CP_BOOTS );
	creaturePartLookUp.insert( "Hat", CP_HAT );
	creaturePartLookUp.insert( "Back", CP_BACK );

	creaturePartLookUp.insert( "HeadArmor", CP_ARMOR_HEAD );
	creaturePartLookUp.insert( "ChestArmor", CP_ARMOR_TORSO );
	creaturePartLookUp.insert( "LeftArmArmor", CP_ARMOR_LEFT_ARM );
	creaturePartLookUp.insert( "RightArmArmor", CP_ARMOR_RIGHT_ARM );
	creaturePartLookUp.insert( "LeftHandArmor", CP_ARMOR_LEFT_HAND );
	creaturePartLookUp.insert( "RightHandArmor", CP_ARMOR_RIGHT_HAND );
	creaturePartLookUp.insert( "LeftFootArmor", CP_ARMOR_LEFT_FOOT );
	creaturePartLookUp.insert( "RightFootArmor", CP_ARMOR_RIGHT_FOOT );
	creaturePartLookUp.insert( "LeftHandHeld", CP_LEFT_HAND_HELD );
	creaturePartLookUp.insert( "RightHandHeld", CP_RIGHT_HAND_HELD );

	creaturePartLookUp.insert( "ArmArmor", CP_ARMOR_ARM );
	creaturePartLookUp.insert( "HandArmor", CP_ARMOR_HAND );
	creaturePartLookUp.insert( "FootArmor", CP_ARMOR_FOOT );
	creaturePartLookUp.insert( "LegArmor", CP_ARMOR_LEG );

	creaturePartToString.insert( CP_HEAD, "Head" );
	creaturePartToString.insert( CP_TORSO, "Torso" );
	creaturePartToString.insert( CP_LEFT_ARM, "LeftArm" );
	creaturePartToString.insert( CP_RIGHT_ARM, "RightArm" );
	creaturePartToString.insert( CP_LEFT_HAND, "LeftHand" );
	creaturePartToString.insert( CP_RIGHT_HAND, "RightHand" );
	creaturePartToString.insert( CP_LEFT_LEG, "LeftLeg" );
	creaturePartToString.insert( CP_RIGHT_LEG, "LegLeg" );
	creaturePartToString.insert( CP_LEFT_FOOT, "LeftFoot" );
	creaturePartToString.insert( CP_RIGHT_FOOT, "RightFoot" );

	creaturePartToString.insert( CP_LEFT_FRONT_LEG, "LeftFrontLeg" );
	creaturePartToString.insert( CP_RIGHT_FRONT_LEG, "LegFrontLeg" );
	creaturePartToString.insert( CP_LEFT_FRONT_FOOT, "LeftFrontFoot" );
	creaturePartToString.insert( CP_RIGHT_FRONT_FOOT, "RightFrontFoot" );

	creaturePartToString.insert( CP_LEFT_WING, "LeftWing" );
	creaturePartToString.insert( CP_RIGHT_WING, "RightWing" );

	creaturePartToString.insert( CP_BRAIN, "Brain" );
	creaturePartToString.insert( CP_LEFT_EYE, "LeftEye" );
	creaturePartToString.insert( CP_RIGHT_EYE, "RightEye" );
	creaturePartToString.insert( CP_HEART, "Heart" );
	creaturePartToString.insert( CP_LEFT_LUNG, "LeftLung" );
	creaturePartToString.insert( CP_RIGHT_LUNG, "RightLung" );

	creaturePartToString.insert( CP_HAIR, "Hair" );
	creaturePartToString.insert( CP_FACIAL_HAIR, "Facial" );
	creaturePartToString.insert( CP_CLOTHING, "Clothing" );
	creaturePartToString.insert( CP_BOOTS, "Boots" );
	creaturePartToString.insert( CP_HAT, "Hat" );
	creaturePartToString.insert( CP_BACK, "Back" );

	creaturePartToString.insert( CP_ARMOR_HEAD, "HeadArmor" );
	creaturePartToString.insert( CP_ARMOR_TORSO, "ChestArmor" );
	creaturePartToString.insert( CP_ARMOR_LEFT_ARM, "LeftArmArmor" );
	creaturePartToString.insert( CP_ARMOR_RIGHT_ARM, "RightArmArmor" );
	creaturePartToString.insert( CP_ARMOR_LEFT_HAND, "LeftHandArmor" );
	creaturePartToString.insert( CP_ARMOR_RIGHT_HAND, "RightHandArmor" );
	creaturePartToString.insert( CP_ARMOR_LEFT_FOOT, "LeftFootArmor" );
	creaturePartToString.insert( CP_ARMOR_RIGHT_FOOT, "RightFootArmor" );
	creaturePartToString.insert( CP_LEFT_HAND_HELD, "LeftHandHeld" );
	creaturePartToString.insert( CP_RIGHT_HAND_HELD, "RightHandHeld" );

	creaturePartToString.insert( CP_ARMOR_ARM, "ArmArmor" );
	creaturePartToString.insert( CP_ARMOR_HAND, "HandArmor" );
	creaturePartToString.insert( CP_ARMOR_FOOT, "FootArmor" );
	creaturePartToString.insert( CP_ARMOR_LEG, "LegArmor" );

	craftable.clear();
	auto rows = DB::selectRows( "Crafts" );
	for( const auto& row : rows )
	{
		craftable.insert( row.value( "ItemID" ).toString() );
	}
}

/**
 * @brief Returns a reference to the application-wide Logger instance.
 * @return Reference to the static Logger.
 */
Logger& Global::logger()
{
	return m_logger;
}

/*
KeyBindings& Global::keyBindings()
{
	return m_keyBindings;
}
*/

/**
 * @brief Loads all behavior trees from content/ai directory.
 *
 * Queries the AI table for behavior tree XML filenames, parses each one,
 * and stores the root QDomElement in m_behaviorTrees keyed by AI ID.
 * @return True if all trees loaded successfully, false on any parse failure.
 */
bool Global::loadBehaviorTrees()
{
	m_behaviorTrees.clear();

	for ( auto id : DB::ids( "AI" ) )
	{
		QString xmlName = DB::select( "BehaviorTree", "AI", id ).toString();

		QDomDocument xml;
		// Load xml file as raw data
		QFile f( Global::cfg->get( "dataPath" ).toString() + "/ai/" + xmlName );
		if ( !f.open( QIODevice::ReadOnly ) )
		{
			// Error while loading file
			qDebug() << "Error while loading file xmlName";
			return false;
		}
		// Set data into the QDomDocument before processing
		xml.setContent( &f );
		f.close();

		QDomElement root = xml.documentElement();
		m_behaviorTrees.insert( id, root );
	}
	return true;
}

/**
 * @brief Retrieves a loaded behavior tree XML element by ID.
 * @param id The behavior tree identifier (from the AI DB table).
 * @return The QDomElement root of the behavior tree.
 */
QDomElement Global::behaviorTree( QString id )
{
	return m_behaviorTrees.value( id );
}

/**
 * @brief Loads and registers a behavior tree from an XML file path.
 * @param id The identifier to register the tree under.
 * @param path File path to the behavior tree XML.
 * @return True if the tree was loaded successfully.
 */
bool Global::addBehaviorTree( QString id, QString path )
{
	QDomDocument xml;
	// Load xml file as raw data
	QFile f( path );
	if ( !f.open( QIODevice::ReadOnly ) )
	{
		// Error while loading file
		qDebug() << "Error while loading file xmlName";
		return false;
	}
	// Set data into the QDomDocument before processing
	xml.setContent( &f );
	f.close();

	QDomElement root = xml.documentElement();
	m_behaviorTrees.insert( id, root );

	return true;
}
