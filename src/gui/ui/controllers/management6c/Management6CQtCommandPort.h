/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "../../screens/management6c/Management6CController.h"

#include <QPointer>

#include <functional>

class EventConnector;

namespace ingnomia::ui::management6c
{

class Management6CQtCommandPort final : public CommandPort
{
public:
	explicit Management6CQtCommandPort( EventConnector* );
	CommandResult dispatch( const UiActionEnvelope&, DispatchOrigin ) override;
	void setWorld( WorldEpoch world, bool accepts ) { world_ = world; accepts_ = accepts; }

private:
	CommandResult reject( const char* ) const;
	CommandResult queue( std::function<void()> ) const;
	QPointer<EventConnector> connector_;
	WorldEpoch world_;
	bool accepts_{};
};

} // namespace ingnomia::ui::management6c

