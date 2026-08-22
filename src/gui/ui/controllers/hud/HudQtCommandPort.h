/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/hud/HudController.h"
#include <QHash>
#include <QPointer>
#include <functional>
class EventConnector;
class MainWindow;
namespace ingnomia::ui::hud
{
class HudQtCommandPort final : public HudCommandPort
{
public:
	HudQtCommandPort( EventConnector* connector, MainWindow* window );
	CommandResult dispatch( const UiActionEnvelope& action ) override;
	CommandResult requestBuildItems( BuildSelection selection, std::string_view category ) override;
	void registerPrompt( PromptInstanceId instance, std::optional<EventResponseTargetId> target ) override;
	void setAuthoritativeOverlays( RenderOverlayState state ) { overlays_ = state; }
private:
	CommandResult reject( const char* error ) const;
	CommandResult queue( std::function<void()> callback ) const;
	QPointer<EventConnector> connector_;
	QPointer<MainWindow> window_;
	QHash<qulonglong, unsigned int> responseTargets_;
	RenderOverlayState overlays_;
};
}
