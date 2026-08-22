/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "DebugQtCommandPort.h"

#if defined(INGNOMIA_DEVELOPER_UI)
#include "../../../aggregatordebug.h"
#include "../../../eventconnector.h"
#include <QMetaObject>
#endif
namespace ingnomia::ui::debug
{
#if defined(INGNOMIA_DEVELOPER_UI)
namespace
{
QString q( const std::string& s )
{
	return QString::fromUtf8( s.data(), (qsizetype)s.size() );
}
} // namespace
#endif
DebugQtCommandPort::DebugQtCommandPort( EventConnector* c ) :
	connector_( c )
{
}
bool DebugQtCommandPort::dispatch( const DebugAction& a )
{
#if !defined( INGNOMIA_DEVELOPER_UI )
	(void)a;
	return false;
#else
	if ( !connector_ )
		return false;
	auto* agg = connector_->aggregatorDebug();
	if ( !agg )
		return false;
	return QMetaObject::invokeMethod( connector_, [agg, a]
									  {switch(a.kind){case ActionKind::RequestGnomes:agg->onRequestGnomeList();break;case ActionKind::RequestGroups:agg->onRequestItemGroups();break;case ActionKind::RequestItems:agg->onRequestItems(q(a.primary));break;case ActionKind::RequestMaterials:agg->onRequestMaterials(q(a.primary));break;case ActionKind::SpawnCreature:agg->onSpawnCreature(q(a.primary));break;case ActionKind::SetNeed:agg->onSetNeed(a.gnome,q(a.primary),a.value);break;case ActionKind::KillGnome:agg->onKillGnome(a.gnome);break;case ActionKind::SpawnItem:{QStringList materials;for(const auto&m:a.materials)materials.push_back(q(m));if(materials.size()>1)agg->onSpawnCompositeItem(q(a.primary),materials,a.count,a.x,a.y,a.z);else agg->onSpawnItem(q(a.primary),materials.empty()?q(a.secondary):materials.front(),a.count,a.x,a.y,a.z);break;}case ActionKind::SetWindowSize:agg->onSetWindowSize(a.width,a.height);break;case ActionKind::SetNeedDecayMultiplier:agg->onSetNeedDecayMultiplier(a.value);break;case ActionKind::SetDisableNeedDecay:agg->onSetDisableNeedDecay(q(a.primary),a.enabled);break;} }, Qt::QueuedConnection );
#endif
}
} // namespace ingnomia::ui::debug
