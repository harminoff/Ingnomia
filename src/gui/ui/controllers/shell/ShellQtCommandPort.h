/*
 * This file is part of Ingnomia https://github.com/rschurade/Ingnomia
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include "../../screens/shell/ShellController.h"

#include <QHash>
#include <QPointer>
#include <QString>

class EventConnector;

namespace ingnomia::ui::shell
{

// GUI-thread adapter only. It queues existing EventConnector/AggregatorSettings slots and
// privately resolves safe relative save IDs; no Game pointer crosses or is retained here.
class ShellQtCommandPort final : public ShellCommandPort
{
public:
	explicit ShellQtCommandPort( EventConnector* connector );
	CommandResult dispatch( const UiActionEnvelope& action ) override;

	void rememberKingdomPath( const SaveKingdomId& id, QString absolutePath );
	void rememberSavePath( const SaveSlotId& id, QString absolutePath );
	void clearPrivatePaths();

private:
	[[nodiscard]] CommandResult reject( const char* localizationKey ) const;
	[[nodiscard]] CommandResult queued() const { return { CommandStatus::Accepted, std::nullopt, true }; }
	[[nodiscard]] CommandResult complete() const { return { CommandStatus::Accepted, std::nullopt, false }; }

	QPointer<EventConnector> connector_;
	QHash<QString, QString> kingdomPaths_;
	QHash<QString, QString> savePaths_;
};

} // namespace ingnomia::ui::shell
