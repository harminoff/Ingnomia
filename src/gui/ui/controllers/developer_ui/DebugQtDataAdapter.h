/* SPDX-License-Identifier: AGPL-3.0-or-later */
#pragma once
#include "../../screens/developer_ui/DebugState.h"

#include <QList>
#include <QPair>
#include <QStringList>
namespace ingnomia::ui::debug
{
class DebugQtDataAdapter
{
public:
	void setWorld( WorldEpoch );
	WorldEpoch world() const
	{
		return world_;
	}
	std::pair<Revision, std::vector<NamedId>> gnomes( const QList<QPair<QString, unsigned int>>& );
	std::pair<Revision, ItemCatalog> groups( const QStringList& );
	std::pair<Revision, ItemCatalog> items( const QStringList& );
	std::pair<Revision, ItemCatalog> materials( int, const QStringList&, const QStringList& );

private:
	WorldEpoch world_;
	Revision revision_;
	ItemCatalog catalog_;
};
} // namespace ingnomia::ui::debug
