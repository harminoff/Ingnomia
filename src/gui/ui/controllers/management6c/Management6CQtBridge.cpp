/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "Management6CQtBridge.h"

#include "../../screens/management6c/Management6CController.h"
#include "../../../aggregatormilitary.h"
#include "../../../aggregatorneighbors.h"
#include "../../../eventconnector.h"

#include <QMetaObject>

namespace ingnomia::ui::management6c
{

Management6CQtBridge::Management6CQtBridge( QObject* parent ) : QObject( parent ) {}
Management6CQtBridge::~Management6CQtBridge() { detach(); }

bool Management6CQtBridge::attach( EventConnector* connector, Management6CController& controller )
{
	detach();
	if( !connector || !connector->aggregatorMilitary() || !connector->aggregatorNeighbors() ) return false;
	connector_ = connector;
	controller_ = &controller;
	auto* military = connector->aggregatorMilitary();
	auto* diplomacy = connector->aggregatorNeighbors();

	connections_.push_back( QObject::connect( military, &AggregatorMilitary::signalSquads, this,
		[this]( const QList<GuiSquad>& values ) {
			if( !controller_ ) return;
			const auto snapshot = adapter_.military( values );
			if( snapshot ) controller_->applyMilitary( *snapshot );
			else controller_->setMilitaryError( "ui.error.military_payload_invalid" );
		}, Qt::QueuedConnection ) );
	connections_.push_back( QObject::connect( military, &AggregatorMilitary::signalPriorities, this,
		[this]( unsigned int squad, const QList<GuiTargetPriority>& values ) {
			if( !controller_ ) return;
			const auto patch = adapter_.priorities( squad, values );
			if( patch ) controller_->applyPriorityPatch( *patch );
			else controller_->setMilitaryError( "ui.error.military_priority_payload_invalid" );
		}, Qt::QueuedConnection ) );
	connections_.push_back( QObject::connect( military, &AggregatorMilitary::signalRoles, this,
		[this]( const QList<GuiMilRole>& values ) {
			if( !controller_ ) return;
			const auto snapshot = adapter_.roles( values );
			if( snapshot ) controller_->applyRoles( *snapshot );
			else controller_->setMilitaryError( "ui.error.military_role_payload_invalid" );
		}, Qt::QueuedConnection ) );
	connections_.push_back( QObject::connect( military, &AggregatorMilitary::signalPossibleMaterials, this,
		[this]( unsigned int role, const QString& slot, const QStringList& values ) {
			// onSetArmorType emits this subset before the command port requests the refreshed role/type
			// snapshot. Defer one GUI turn so the subset's base revision is that authoritative snapshot.
			QMetaObject::invokeMethod( this, [this, role, slot, values] {
				if( !controller_ ) return;
				const auto patch = adapter_.materials( role, slot, values );
				if( patch ) controller_->applyMaterialOptions( *patch );
				else controller_->setMilitaryError( "ui.error.military_material_payload_invalid" );
			}, Qt::QueuedConnection );
		}, Qt::QueuedConnection ) );

	connections_.push_back( QObject::connect( diplomacy, &AggregatorNeighbors::signalNeighborsUpdate, this,
		[this]( const QList<GuiNeighborInfo>& values ) {
			if( controller_ ) controller_->applyNeighbors( adapter_.neighbors( values ) );
		}, Qt::QueuedConnection ) );
	connections_.push_back( QObject::connect( diplomacy, &AggregatorNeighbors::signalAvailableGnomes, this,
		[this]( const QList<GuiAvailableGnome>& values ) {
			if( controller_ ) controller_->applyAvailableGnomes( adapter_.availableGnomes( values ) );
		}, Qt::QueuedConnection ) );
	connections_.push_back( QObject::connect( diplomacy, &AggregatorNeighbors::signalMissions, this,
		[this]( const QList<Mission>& values ) {
			if( !controller_ ) return;
			const auto snapshot = adapter_.missions( values );
			if( snapshot ) controller_->applyMissions( *snapshot );
			else controller_->setDiplomacyError( "ui.error.mission_payload_invalid" );
		}, Qt::QueuedConnection ) );
	connections_.push_back( QObject::connect( diplomacy, &AggregatorNeighbors::signalUpdateMission, this,
		[this]( const Mission& value ) {
			if( !controller_ ) return;
			const auto patch = adapter_.mission( value );
			if( patch ) controller_->applyMissionPatch( *patch );
			else controller_->setDiplomacyError( "ui.error.mission_payload_invalid" );
		}, Qt::QueuedConnection ) );
	return true;
}

void Management6CQtBridge::detach()
{
	for( const auto& connection : connections_ ) QObject::disconnect( connection );
	connections_.clear();
	connector_.clear();
	controller_ = nullptr;
}

void Management6CQtBridge::beginWorld( WorldEpoch world )
{
	adapter_.setWorld( world );
	if( controller_ ) controller_->beginWorld( world );
}

void Management6CQtBridge::endWorld()
{
	adapter_.setWorld( {} );
	if( controller_ ) controller_->endWorld();
}

} // namespace ingnomia::ui::management6c
