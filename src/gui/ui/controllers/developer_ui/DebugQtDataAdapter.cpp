/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "DebugQtDataAdapter.h"
namespace ingnomia::ui::debug
{
namespace
{
std::string text( const QString& s )
{
	auto b = s.toUtf8();
	return { b.constData(), (std::size_t)b.size() };
}
std::vector<std::string> list( const QStringList& v )
{
	std::vector<std::string> o;
	o.reserve( v.size() );
	for ( const auto& s : v )
		o.push_back( text( s ) );
	return o;
}
} // namespace
void DebugQtDataAdapter::setWorld( WorldEpoch w )
{
	world_    = w;
	revision_ = {};
	catalog_  = {};
}
std::pair<Revision, std::vector<NamedId>> DebugQtDataAdapter::gnomes( const QList<QPair<QString, unsigned int>>& v )
{
	std::vector<NamedId> o;
	o.reserve( v.size() );
	for ( const auto& r : v )
		o.push_back( { text( r.first ), r.second } );
	return { Revision { ++revision_.value }, std::move( o ) };
}
std::pair<Revision, ItemCatalog> DebugQtDataAdapter::groups( const QStringList& v )
{
	catalog_.groups = list( v );
	return { Revision { ++revision_.value }, catalog_ };
}
std::pair<Revision, ItemCatalog> DebugQtDataAdapter::items( const QStringList& v )
{
	catalog_.items = list( v );
	return { Revision { ++revision_.value }, catalog_ };
}
std::pair<Revision, ItemCatalog> DebugQtDataAdapter::materials( int n, const QStringList& a, const QStringList& b )
{
	catalog_.componentCount = n;
	catalog_.materials1     = list( a );
	catalog_.materials2     = list( b );
	return { Revision { ++revision_.value }, catalog_ };
}
} // namespace ingnomia::ui::debug
