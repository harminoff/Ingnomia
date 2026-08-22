/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "ShellDataAdapter.h"

#include <algorithm>

namespace ingnomia::ui::shell
{
namespace
{
SettingRow row( const char* id, const char* label, const char* description, SettingControl control,
	SettingValue value, std::optional<SettingValue> minimum = std::nullopt,
	std::optional<SettingValue> maximum = std::nullopt )
{
	return { SettingId{ id }, LocalizationKey{ label }, LocalizationKey{ description }, control,
		SettingApplyMode::Immediate, value, std::move( value ), std::move( minimum ), std::move( maximum ), std::nullopt };
}
}

SettingsState ShellDataAdapter::settings( const UpstreamSettingsSnapshot& source )
{
	SettingsState result;
	result.status = RequestStatus::Ready;
	result.rows = {
		row( "display.fullscreen", "ui.settings.fullscreen", "ui.settings.fullscreen.description",
			SettingControl::Toggle, source.fullscreen ),
		row( "display.follow_monitor_refresh", "ui.settings.follow_monitor_refresh", "ui.settings.follow_monitor_refresh.description",
			SettingControl::Toggle, source.followMonitorRefresh ),
		row( "display.frame_rate_limit", "ui.settings.frame_rate_limit", "ui.settings.frame_rate_limit.description",
			SettingControl::Integer, std::clamp( source.frameRateLimit, std::int32_t{ 30 }, std::int32_t{ 240 } ), SettingValue{ std::int32_t{ 30 } }, SettingValue{ std::int32_t{ 240 } } ),
		row( "interface.ui_scale", "ui.settings.ui_scale", "ui.settings.ui_scale.description",
			SettingControl::Decimal, std::max( 0.5f, source.uiScale ), SettingValue{ 0.5f }, SettingValue{ 2.0f } ),
		row( "camera.keyboard_pan_speed", "ui.settings.keyboard_pan_speed", "ui.settings.keyboard_pan_speed.description",
			SettingControl::Integer, std::clamp( source.keyboardPanSpeed, 0, 200 ), SettingValue{ std::int32_t{ 0 } }, SettingValue{ std::int32_t{ 200 } } ),
		row( "display.minimum_light", "ui.settings.minimum_light", "ui.settings.minimum_light.description",
			SettingControl::Integer, std::clamp( source.minimumLightPercent, 0, 100 ), SettingValue{ std::int32_t{ 0 } }, SettingValue{ std::int32_t{ 100 } } ),
		row( "camera.wheel_changes_level", "ui.settings.wheel_changes_level", "ui.settings.wheel_changes_level.description",
			SettingControl::Toggle, source.wheelChangesLevel )
	};
	return result;
}

LoadGameState ShellDataAdapter::saves( std::vector<SaveKingdomRow> kingdoms, std::vector<SaveSlotRow> saves )
{
	LoadGameState result;
	result.kingdoms = std::move( kingdoms );
	result.saves = std::move( saves );
	result.kingdomsStatus = result.kingdoms.empty() ? RequestStatus::Empty : RequestStatus::Ready;
	result.savesStatus = result.saves.empty() ? RequestStatus::Empty : RequestStatus::Ready;
	if( !result.kingdoms.empty() ) result.selectedKingdom = result.kingdoms.front().id;
	const auto compatible = std::ranges::find_if( result.saves, []( const SaveSlotRow& save ) { return save.compatible; } );
	if( compatible != result.saves.end() ) result.selectedSlot = compatible->id;
	return result;
}

} // namespace ingnomia::ui::shell
