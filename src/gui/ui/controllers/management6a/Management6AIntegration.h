/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../screens/management6a/Management6ARmlBinding.h"
#include "Management6AQtCommandPort.h"

#include <QObject>

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

class EventConnector;
namespace Rml
{
class Context;
}

namespace ingnomia::ui::management6a
{
class Management6AIntegration final : public QObject
{
public:
	using ViewHandler = std::function<void( ManagementView )>;
	Management6AIntegration( EventConnector*, Rml::Context&, QObject* parent = nullptr );
	~Management6AIntegration() override;
	bool initialize();
	void shutdown();
	void beginWorld( WorldEpoch );
	void endWorld();
	void setViewHandler( ViewHandler handler ) { viewHandler_ = std::move( handler ); }
	void setSelectedPosition( std::optional<WorldPosition> position )
	{
		selectedPosition_ = position;
	}
	void loadSelfTestFixture();
	[[nodiscard]] bool activateElement( std::string_view id );
	[[nodiscard]] bool setFormValueForProbe( std::string_view id, std::string_view value );
	[[nodiscard]] bool setStockpileSearchForProbe( std::string_view value );
	[[nodiscard]] bool activateFirstStockpileFilterForProbe( TriState, FilterDepth );
	[[nodiscard]] bool activateStockpileFilterForProbe( std::string_view item, std::string_view material );
	[[nodiscard]] bool dispatchStockpileFilterKeyForProbe( int keyIdentifier );
	[[nodiscard]] Management6AController* controller() const noexcept
	{
		return controller_.get();
	}
	[[nodiscard]] Management6ARmlBinding* binding() const noexcept { return binding_.get(); }

private:
	void connectSignals();
	EventConnector* connector_ {};
	Rml::Context& context_;
	std::unique_ptr<Management6ARmlBinding> binding_;
	std::unique_ptr<Management6AQtCommandPort> commands_;
	std::unique_ptr<Management6AController> controller_;
	std::optional<WorldPosition> selectedPosition_;
	std::vector<AgricultureCatalogRow> plants_, animals_, trees_;
	Revision workshopRevision_, stockpileRevision_, agricultureRevision_;
	bool connected_ {};
	ViewHandler viewHandler_;
};
} // namespace ingnomia::ui::management6a
