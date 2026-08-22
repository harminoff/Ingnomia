/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "HudQtCommandPort.h"
#include "LegacyToolActionAdapter.h"
#include "../../../aggregatorselection.h"
#include "../../../aggregatorinventory.h"
#include "../../../eventconnector.h"
#include "../../../mainwindow.h"
#include "../../../../base/global.h"
#include "../../../../base/selection.h"
#include <QMetaObject>
namespace ingnomia::ui::hud
{
HudQtCommandPort::HudQtCommandPort( EventConnector* connector, MainWindow* window ) : connector_( connector ), window_( window ) {}
CommandResult HudQtCommandPort::reject( const char* error ) const { return { CommandStatus::Rejected, false, error }; }
CommandResult HudQtCommandPort::queue( std::function<void()> callback ) const
{
	if( !connector_ || !QMetaObject::invokeMethod( connector_, std::move( callback ), Qt::QueuedConnection ) ) return reject( "ui.error.bridge_queue_failed" );
	return { CommandStatus::Accepted, true, {} };
}
void HudQtCommandPort::registerPrompt( PromptInstanceId instance, std::optional<EventResponseTargetId> target )
{
	if( target && *target ) responseTargets_.insert( instance.value, target->value );
}
CommandResult HudQtCommandPort::dispatch( const UiActionEnvelope& action )
{
	if( !connector_ ) return reject( "ui.error.bridge_unavailable" );
	if( action.id.value == "sim.set_paused" )
	{
		const auto* value = std::get_if<SetPausedPayload>( &action.payload ); if( !value ) return reject( "ui.error.invalid_payload" );
		return queue( [target = connector_, paused = value->paused]() { if( target ) { target->onSetPause( paused ); target->onTutorialFact( static_cast<unsigned int>( TutorialFact::PauseResume ) ); } } );
	}
	if( action.id.value == "sim.set_speed" )
	{
		const auto* value = std::get_if<SetSpeedPayload>( &action.payload ); if( !value ) return reject( "ui.error.invalid_payload" );
		const ::GameSpeed speed = value->speed == ui::GameSpeed::Fast ? ::GameSpeed::Fast : ::GameSpeed::Normal;
		return queue( [target = connector_, speed]() { if( target ) { target->onSetGameSpeed( speed ); target->onTutorialFact( static_cast<unsigned int>( TutorialFact::ChangeSpeed ) ); } } );
	}
	if( action.id.value == "tutorial.advance" || action.id.value == "tutorial.skip" || action.id.value == "tutorial.restart" || action.id.value == "tutorial.toggle_hints" || action.id.value == "tutorial.finish" )
	{
		if( !std::holds_alternative<NoPayload>( action.payload ) ) return reject( "ui.error.invalid_payload" );
		return queue( [target = connector_, id = action.id.value]() {
			if( !target ) return;
			if( id == "tutorial.advance" ) target->onTutorialAdvance();
			else if( id == "tutorial.skip" ) target->onTutorialSkip();
			else if( id == "tutorial.restart" ) target->onTutorialRestart();
			else if( id == "tutorial.toggle_hints" ) target->onTutorialToggleHints();
			else target->onTutorialFinish();
		} );
	}
	if( action.id.value == "view.change_level" )
	{
		const auto* value = std::get_if<ChangeLevelPayload>( &action.payload ); if( !value || !window_ ) return reject( "ui.error.invalid_payload" );
		const int level = value->absoluteLevel;
		return queue( [target = connector_, window = window_, level]() { QMetaObject::invokeMethod( window, "onUiSetViewLevel", Qt::QueuedConnection, Q_ARG( int, level ) ); if( target ) target->onTutorialFact( static_cast<unsigned int>( TutorialFact::ChangeLevel ) ); } );
	}
	if( action.id.value == "view.set_overlay" )
	{
		const auto* value = std::get_if<OverlayPayload>( &action.payload ); if( !value ) return reject( "ui.error.invalid_payload" );
		switch( value->overlay ) { case OverlayKind::Designations: overlays_.designations=value->enabled; break; case OverlayKind::Jobs: overlays_.jobs=value->enabled; break; case OverlayKind::LoweredWalls: overlays_.loweredWalls=value->enabled; break; case OverlayKind::Axles: overlays_.axles=value->enabled; break; }
		const auto all = overlays_;
		return queue( [target = connector_, all]() { if( target ) target->onSetRenderOptions( all.designations, all.jobs, all.loweredWalls, all.axles ); } );
	}
	if( action.id.value == "tool.activate" )
	{
		const auto* value = std::get_if<ActivateToolPayload>( &action.payload ); if( !value ) return reject( "ui.error.invalid_payload" );
		const auto legacy = LegacyToolActionAdapter::action( value->tool );
		if( !legacy || value->tool.value == "build" || value->tool.value == "inspect" ) return reject( "ui.error.tool_requires_catalog" );
		const QString command = QString::fromLatin1( legacy->data(), static_cast<qsizetype>( legacy->size() ) );
		return queue( [target = connector_, command]() { if( target ) target->onSetSelectionAction( command ); } );
	}
	if( action.id.value == "tool.cancel" || action.id.value == "tool.rotate" )
	{
		auto* selection = connector_->aggregatorSelection();
		const bool ok = action.id.value == "tool.cancel"
			? QMetaObject::invokeMethod( selection, &AggregatorSelection::onCancelSelection, Qt::QueuedConnection )
			: QMetaObject::invokeMethod( selection, &AggregatorSelection::onRotateSelection, Qt::QueuedConnection );
		if( !ok ) return reject( "ui.error.bridge_queue_failed" );
		if( action.id.value == "tool.rotate" )
			return queue( [target = connector_]() { if( target ) target->onTutorialFact( static_cast<unsigned int>( TutorialFact::Rotate ) ); } );
		return CommandResult{ CommandStatus::Accepted, true, {} };
	}
	if( action.id.value == "tool.choose_build" )
	{
		const auto* value = std::get_if<ChooseBuildPayload>( &action.payload ); if( !value ) return reject( "ui.error.invalid_payload" );
		const ::BuildItemType kind = value->kind == BuildKind::Workshop ? ::BuildItemType::Workshop : value->kind == BuildKind::Terrain ? ::BuildItemType::Terrain : ::BuildItemType::Item;
		QStringList materials; for( const auto& material : value->materials ) materials.push_back( QString::fromStdString( material.value ) );
		const QString item = QString::fromStdString( value->item.value );
		const QString command = value->action == BuildAction::FillHole ? QStringLiteral( "FillHole" ) : value->action == BuildAction::Replace ? QStringLiteral( "Replace" ) : QString{};
		return queue( [target = connector_, kind, command, item, materials]() { if( target ) target->onCmdBuild( kind, command, item, materials ); } );
	}
	if( action.id.value == "tool.set_material" )
	{
		const auto* value = std::get_if<SetBuildMaterialPayload>( &action.payload ); if( !value ) return reject( "ui.error.invalid_payload" );
		const auto material = QString::fromStdString( value->material.value );
		const auto componentIndex = value->componentIndex;
		return queue( [target = connector_, material, componentIndex]() {
			if( !target || !Global::sel ) return;
			QStringList materials = Global::sel->material();
			while( materials.size() <= static_cast<int>( componentIndex ) ) materials.push_back( QString{} );
			materials[static_cast<int>( componentIndex )] = material;
			target->onSetSelectionMaterials( materials );
		} );
	}
	if( action.id.value == "event.respond" )
	{
		const auto* value = std::get_if<EventResponsePayload>( &action.payload ); if( !value ) return reject( "ui.error.invalid_payload" );
		if( value->response == EventResponse::Acknowledge ) { responseTargets_.remove( value->prompt.value ); return { CommandStatus::Accepted, false, {} }; }
		const auto found = responseTargets_.constFind( value->prompt.value ); if( found == responseTargets_.cend() ) return reject( "ui.error.event_target_unavailable" );
		const auto targetId = *found; responseTargets_.erase( found ); const bool answer = value->response == EventResponse::Yes;
		return queue( [target = connector_, targetId, answer]() { if( target ) target->onAnswer( targetId, answer ); } );
	}
	return reject( "ui.error.action_unavailable" );
}
CommandResult HudQtCommandPort::requestBuildItems( BuildSelection selection, std::string_view category )
{
	if( !connector_ ) return reject( "ui.error.bridge_unavailable" );
	const QString requestedCategory = QString::fromUtf8( category.data(), static_cast<qsizetype>( category.size() ) );
	return queue( [target = connector_, selection, requestedCategory]() {
		if( target && target->aggregatorInventory() ) target->aggregatorInventory()->onRequestBuildItems( selection, requestedCategory );
	} );
}
}
