/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once

#include "Management6CQtDataAdapter.h"

#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <vector>

class EventConnector;

namespace ingnomia::ui::management6c
{

class Management6CController;

class Management6CQtBridge final : public QObject
{
public:
	explicit Management6CQtBridge( QObject* parent = nullptr );
	~Management6CQtBridge() override;
	bool attach( EventConnector*, Management6CController& );
	void detach();
	void beginWorld( WorldEpoch );
	void endWorld();
	[[nodiscard]] std::size_t connectionCount() const noexcept { return connections_.size(); }

private:
	QPointer<EventConnector> connector_;
	Management6CController* controller_{};
	Management6CQtDataAdapter adapter_;
	std::vector<QMetaObject::Connection> connections_;
};

} // namespace ingnomia::ui::management6c

