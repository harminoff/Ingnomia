/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/management6a/Management6AController.h"

#include <QHash>
#include <QPointer>
#include <functional>

class EventConnector;

namespace ingnomia::ui::management6a
{
class Management6AQtCommandPort final : public CommandPort
{
public:
	explicit Management6AQtCommandPort( EventConnector*, ManagementView = ManagementView::Workshop );
	CommandResult dispatch( const UiActionEnvelope&, DispatchOrigin ) override;
	void setWorld( WorldEpoch world, bool acceptsActions ) { activeWorld_ = world; acceptsActions_ = acceptsActions; }
	void rememberWorkshopLink( WorkshopId id, bool linked ) { workshopLinks_[id.value] = linked; }
	void rememberTradeOffer( const TradeRowId&, std::uint32_t );

private:
	CommandResult reject( const char* ) const;
	CommandResult queue( std::function<void()> ) const;
	[[nodiscard]] std::uint32_t rememberedTradeOffer( const TradeRowId& ) const;
	QPointer<EventConnector> connector_;
	ManagementView view_;
	QHash<unsigned int,bool> workshopLinks_;
	struct RememberedTrade { TradeRowId id; std::uint32_t offered{}; };
	std::vector<RememberedTrade> tradeOffers_;
	WorldEpoch activeWorld_;
	bool acceptsActions_{};
};
} // namespace ingnomia::ui::management6a
