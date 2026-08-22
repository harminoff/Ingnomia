/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/management6b/Management6BController.h"
#include <QPointer>
#include <functional>
class EventConnector;
namespace ingnomia::ui::management6b
{
class Management6BQtCommandPort final : public CommandPort
{
public:
	explicit Management6BQtCommandPort( EventConnector* );
	CommandResult dispatch( const UiActionEnvelope& ) override;
	void setWorld( WorldEpoch w, bool accepts ) { world_=w;accepts_=accepts; }
private:
	CommandResult reject( const char* ) const;
	CommandResult queue( std::function<void()> ) const;
	QPointer<EventConnector> connector_;
	WorldEpoch world_;
	bool accepts_{};
};
} // namespace ingnomia::ui::management6b
