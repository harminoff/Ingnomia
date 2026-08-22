/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/developer_ui/DebugController.h"

class EventConnector;
namespace ingnomia::ui::debug
{
class DebugQtCommandPort final : public CommandPort
{
public:
	explicit DebugQtCommandPort( EventConnector* );
	bool dispatch( const DebugAction& ) override;

private:
	EventConnector* connector_{};
};
} // namespace ingnomia::ui::debug
