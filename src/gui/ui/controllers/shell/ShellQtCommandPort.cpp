/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "ShellQtCommandPort.h"

#include "../../../../base/enums.h"
#include "../../../aggregatorloadgame.h"
#include "../../../aggregatorsettings.h"
#include "../../../eventconnector.h"

#include <QMetaObject>

#include <type_traits>

namespace ingnomia::ui::shell
{
namespace
{
QString text( const std::string& value ) { return QString::fromUtf8( value.data(), static_cast<qsizetype>( value.size() ) ); }

}

ShellQtCommandPort::ShellQtCommandPort( EventConnector* connector ) : connector_( connector ) {}

CommandResult ShellQtCommandPort::reject( const char* localizationKey ) const
{
	return { CommandStatus::Rejected, ShellError{ Message{ LocalizationKey{ localizationKey }, {} }, false, std::nullopt }, false };
}

void ShellQtCommandPort::rememberKingdomPath( const SaveKingdomId& id, QString absolutePath )
{
	if( !id.relativeKey.empty() && !absolutePath.isEmpty() ) kingdomPaths_.insert( text( id.relativeKey ), std::move( absolutePath ) );
}

void ShellQtCommandPort::rememberSavePath( const SaveSlotId& id, QString absolutePath )
{
	if( !id.relativeKey.empty() && !absolutePath.isEmpty() ) savePaths_.insert( text( id.relativeKey ), std::move( absolutePath ) );
}

void ShellQtCommandPort::clearPrivatePaths()
{
	kingdomPaths_.clear();
	savePaths_.clear();
}

CommandResult ShellQtCommandPort::dispatch( const UiActionEnvelope& action )
{
	if( !connector_ ) return reject( "ui.error.bridge_unavailable" );
	const auto queueConnector = [&]( auto callback ) {
		return QMetaObject::invokeMethod( connector_, std::move( callback ), Qt::QueuedConnection ) ? queued()
			: reject( "ui.error.bridge_queue_failed" );
	};

	if( action.id.value == "nav.open" || action.id.value == "nav.back" || action.id.value == "nav.close" ) return complete();
	if( action.id.value == "app.exit" ) return queueConnector( [target = connector_]() { if( target ) target->onExit(); } );
	if( action.id.value == "app.continue_last_game" )
		return queueConnector( [target = connector_]() { if( target ) target->onContinueLastGame(); } );
	if( action.id.value == "app.start_tutorial" )
		return queueConnector( [target = connector_]() { if( target ) target->onStartTutorial(); } );
	if( action.id.value == "app.start_new_game" )
	{
		const auto* payload = std::get_if<StartNewGamePayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		// Field edits are applied in-order through EventConnector before this
		// queued start request.  The payload remains part of the typed contract,
		// while the authoritative NewGameSettings object owns the values.
		return queueConnector( [target = connector_]() { if( target ) target->onStartNewGame(); } );
	}
	if( action.id.value == "new_game.set_field" )
	{
		const auto* payload = std::get_if<SetNewGameFieldPayload>( &action.payload );
		if( !payload || payload->field.value.empty() ) return reject( "ui.error.invalid_payload" );
		QVariant value;
		std::visit( [&value]( const auto& source ) {
			using Value = std::decay_t<decltype( source )>;
			if constexpr( std::is_same_v<Value, CatalogId> ) value = QString::fromUtf8( source.value.data(), static_cast<qsizetype>( source.value.size() ) );
			else if constexpr( std::is_same_v<Value, std::string> ) value = QString::fromUtf8( source.data(), static_cast<qsizetype>( source.size() ) );
			else value = QVariant::fromValue( source );
		}, payload->value );
		const QString field = text( payload->field.value );
		return queueConnector( [target = connector_, field, value]() {
			if( target ) target->onSetNewGameField( field, value );
		} );
	}
	if( action.id.value == "new_game.randomize_name" )
		return queueConnector( [target = connector_]() { if( target ) target->onRandomizeNewGameName(); } );
	if( action.id.value == "new_game.randomize_seed" )
		return queueConnector( [target = connector_]() { if( target ) target->onRandomizeNewGameSeed(); } );
	if( action.id.value == "app.load_game" )
	{
		const auto* payload = std::get_if<LoadGamePayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		const auto found = savePaths_.constFind( text( payload->slot.relativeKey ) );
		if( found == savePaths_.cend() ) return reject( "ui.error.save_target_unavailable" );
		const QString path = *found;
		return queueConnector( [target = connector_, path]() { if( target ) target->onLoadGame( path ); } );
	}
	if( action.id.value == "app.save_game" )
		return queueConnector( [target = connector_]() { if( target ) target->onSaveGame(); } );
	if( action.id.value == "app.end_world" )
		return queueConnector( [target = connector_]() { if( target ) target->onEndGame(); } );
	if( action.id.value == "sim.set_paused" )
	{
		const auto* payload = std::get_if<SetPausedPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		return queueConnector( [target = connector_, paused = payload->paused]() { if( target ) target->onSetPause( paused ); } );
	}
	if( action.id.value == "load.refresh" )
	{
		auto* aggregator = connector_->aggregatorLoadGame();
		return QMetaObject::invokeMethod( aggregator, &AggregatorLoadGame::onRequestKingdoms, Qt::QueuedConnection ) ? queued()
			: reject( "ui.error.bridge_queue_failed" );
	}
	if( action.id.value == "load.select_kingdom" )
	{
		const auto* payload = std::get_if<SelectKingdomPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		const auto found = kingdomPaths_.constFind( text( payload->kingdom.relativeKey ) );
		if( found == kingdomPaths_.cend() ) return reject( "ui.error.save_target_unavailable" );
		auto* aggregator = connector_->aggregatorLoadGame();
		const QString path = *found;
		return QMetaObject::invokeMethod( aggregator, [aggregator, path]() { aggregator->onRequestSaveGames( path ); }, Qt::QueuedConnection )
			? queued() : reject( "ui.error.bridge_queue_failed" );
	}
	if( action.id.value == "settings.set_draft" )
	{
		const auto* payload = std::get_if<SetSettingDraftPayload>( &action.payload );
		if( !payload ) return reject( "ui.error.invalid_payload" );
		auto* settings = connector_->aggregatorSettings();
		if( payload->setting.value == "display.fullscreen" && std::holds_alternative<bool>( payload->value ) )
			return QMetaObject::invokeMethod( settings, [settings, value = std::get<bool>( payload->value )]() { settings->onSetFullScreen( value ); }, Qt::QueuedConnection ) ? complete() : reject( "ui.error.bridge_queue_failed" );
		if( payload->setting.value == "display.follow_monitor_refresh" && std::holds_alternative<bool>( payload->value ) )
			return QMetaObject::invokeMethod( settings, [settings, value = std::get<bool>( payload->value )]() { settings->onSetFollowMonitorRefresh( value ); }, Qt::QueuedConnection ) ? complete() : reject( "ui.error.bridge_queue_failed" );
		if( payload->setting.value == "display.frame_rate_limit" && std::holds_alternative<std::int32_t>( payload->value ) )
			return QMetaObject::invokeMethod( settings, [settings, value = std::get<std::int32_t>( payload->value )]() { settings->onSetFrameRateLimit( value ); }, Qt::QueuedConnection ) ? complete() : reject( "ui.error.bridge_queue_failed" );
		if( payload->setting.value == "interface.ui_scale" && std::holds_alternative<float>( payload->value ) )
			return QMetaObject::invokeMethod( settings, [settings, value = std::get<float>( payload->value )]() { settings->onSetUIScale( value ); }, Qt::QueuedConnection ) ? complete() : reject( "ui.error.bridge_queue_failed" );
		if( payload->setting.value == "camera.keyboard_pan_speed" && std::holds_alternative<std::int32_t>( payload->value ) )
			return QMetaObject::invokeMethod( settings, [settings, value = std::get<std::int32_t>( payload->value )]() { settings->onSetKeyboardSpeed( value ); }, Qt::QueuedConnection ) ? complete() : reject( "ui.error.bridge_queue_failed" );
		if( payload->setting.value == "display.minimum_light" && std::holds_alternative<std::int32_t>( payload->value ) )
			return QMetaObject::invokeMethod( settings, [settings, value = std::get<std::int32_t>( payload->value )]() { settings->onSetLightMin( value ); }, Qt::QueuedConnection ) ? complete() : reject( "ui.error.bridge_queue_failed" );
		if( payload->setting.value == "camera.wheel_changes_level" && std::holds_alternative<bool>( payload->value ) )
			return QMetaObject::invokeMethod( settings, [settings, value = std::get<bool>( payload->value )]() { settings->onSetToggleMouseWheel( value ); }, Qt::QueuedConnection ) ? complete() : reject( "ui.error.bridge_queue_failed" );
		return reject( "ui.error.setting_unsupported" );
	}
	if( action.id.value == "settings.apply" ) return complete(); // All exposed rows are immediate.
	if( action.id.value == "settings.revert" )
		return QMetaObject::invokeMethod( connector_->aggregatorSettings(), &AggregatorSettings::onRequestSettings, Qt::QueuedConnection )
			? complete() : reject( "ui.error.bridge_queue_failed" );
	if( action.id.value == "settings.reset" )
		return QMetaObject::invokeMethod( connector_->aggregatorSettings(), &AggregatorSettings::onResetSupportedSettings, Qt::QueuedConnection )
			? complete() : reject( "ui.error.bridge_queue_failed" );
	return reject( "ui.error.action_unavailable" );
}

} // namespace ingnomia::ui::shell
