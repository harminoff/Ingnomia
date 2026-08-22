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
/** @file aggregatorcreatureinfo.h
 *  @brief Data types and aggregator feeding the Creature Info RmlUi window with per-gnome
 *         attributes, needs, profession, activity, and equipment icons.
 */
#pragma once

#include "../game/creature.h"

#include <QObject>
#include <QElapsedTimer>

#include <array>

#include "../game/gnome.h"
#include "../game/militarymanager.h"

class Game;

/// @brief Creature-info payload sent to the GUI for display on the selected gnome.
struct GuiCreatureInfo
{
	QString name;           ///< Gnome display name.
	unsigned int id = 0;    ///< Creature UID.
	QString position;       ///< Current world position used to anchor inspection actions.
	QString profession;     ///< Current profession name.
	int str = 0;            ///< Strength attribute.
	int dex = 0;            ///< Dexterity attribute.
	int con = 0;            ///< Constitution attribute.
	int intel = 0;          ///< Intelligence attribute.
	int wis = 0;            ///< Wisdom attribute.
	int cha = 0;            ///< Charisma attribute.
	int hunger = 0;         ///< Current hunger level.
	int thirst = 0;         ///< Current thirst level.
	int sleep = 0;          ///< Current sleep level.
	int happiness = 0;      ///< Current happiness level.
	std::array<bool, 4> needsReported{}; ///< Authoritative flags for Hunger, Thirst, Sleep, Happiness.

	QString activity;       ///< Short description of what the gnome is doing right now.
	struct Skill
	{
		QString id;
		QString name;
		int level{};
		bool active{};
	};
	QList<Skill> skills;    ///< Authoritative DB-defined gnome skills.

	Uniform uniform;        ///< Current military uniform (empty if unassigned).
	Equipment equipment;    ///< Currently worn equipment.
	QStringList inventory;  ///< Authoritative carried inventory designations.
	bool inventoryReported = false; ///< True when the creature inventory was queried.

};
Q_DECLARE_METATYPE( GuiCreatureInfo )


/// @brief Aggregates live creature state for the Creature Info RmlUi window.
class AggregatorCreatureInfo : public QObject
{
	Q_OBJECT

public:
	AggregatorCreatureInfo( QObject* parent = nullptr );

	void init( Game* game );

	void update();

private:
	QPointer<Game> g;                                     ///< Game instance (weak ownership).

	GuiCreatureInfo m_info;                               ///< Cached payload for the currently viewed creature.

	unsigned int m_currentID = 0;                         ///< Creature currently shown in the GUI.
	unsigned int m_previousID = 0;                        ///< Previously shown creature (unused but reserved).
	QStringList m_skillIds;                                ///< DB-defined skills in population order.
	QElapsedTimer m_lastUpdate;                           ///< Prevents full detail rebuilds every game tick.


public slots:
	void onRequestCreatureUpdate( unsigned int creatureID );
	void onRequestProfessionList();
	void onSetProfession( unsigned int gnomeID, QString profession );


signals:
	void signalCreatureUpdate( const GuiCreatureInfo& info );
	void signalCreatureCleared();
	void signalProfessionList( const QStringList& profs );


};
