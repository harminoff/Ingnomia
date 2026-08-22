/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/inspector/InspectorController.h"
#include <QHash>
#include <QPointer>
#include <functional>
class EventConnector;
namespace ingnomia::ui::inspector
{
class InspectorQtCommandPort final : public InspectorCommandPort
{
public:
	explicit InspectorQtCommandPort( EventConnector* );
	CommandResult dispatch( const UiActionEnvelope& ) override;
	void setWorld( WorldEpoch world, bool acceptsActions ) { activeWorld_ = world; acceptsActions_ = acceptsActions; }
	void rememberWorkshopLink( WorkshopId id, bool linked ) { workshopLinks_[id.value] = linked; }
private:
	CommandResult reject( const char* ) const;
	CommandResult queue( std::function<void()> ) const;
	QPointer<EventConnector> connector_;
	QHash<unsigned int,bool> workshopLinks_;
	WorldEpoch activeWorld_;
	bool acceptsActions_{};
};
} // namespace ingnomia::ui::inspector
