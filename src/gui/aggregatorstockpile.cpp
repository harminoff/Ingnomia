/*
	This file is part of Ingnomia https://github.com/rschurade/Ingnomia
    Copyright (C) 2017-2020  Ralph Schurade, Ingnomia Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
/** @file aggregatorstockpile.cpp
 *  @brief AggregatorStockpile implementation: fills GuiStockpileInfo (basic settings plus the
 *         per-entry content summary) and routes GUI edits back to the stockpile manager.
 */
#include "aggregatorstockpile.h"

#include "../base/counter.h"
#include "../base/db.h"
#include "../base/dbhelper.h"
#include "../base/global.h"
#include "../base/io.h"
#include "../game/game.h"
#include "../game/inventory.h"
#include "../game/stockpilemanager.h"
#include "../game/tutorialmanager.h"
#include "../game/world.h"
#include "../gui/strings.h"

#include <QJsonDocument>

namespace
{
QString stockpileTemplatePath()
{
	return IO::getDataFolder() + "/settings/stockpile-filter-templates.json";
}

QVariantList loadStockpileTemplates()
{
	QJsonDocument document;
	return IO::loadFile( stockpileTemplatePath(), document ) ? document.toVariant().toList() : QVariantList {};
}

bool saveStockpileTemplates( const QVariantList& templates )
{
	return IO::saveFile( stockpileTemplatePath(), QJsonDocument::fromVariant( templates ) );
}
}

/// @brief Constructs the AggregatorStockpile and registers GuiStockpileInfo as a metatype.
/// @param parent Qt parent object.
AggregatorStockpile::AggregatorStockpile( QObject* parent ) :
	QObject(parent)
{
	qRegisterMetaType<GuiStockpileInfo>();
}

/// @brief Destructor.
AggregatorStockpile::~AggregatorStockpile()
{
}

/// @brief Binds the aggregator to a Game instance.
/// @param game Game to bind to.
void AggregatorStockpile::init( Game* game )
{
	g = game;
}

/// @brief Opens the stockpile window for whichever stockpile owns @p tileID.
/// @param tileID Integer tile key (Position::toInt()).
void AggregatorStockpile::onOpenStockpileInfoOnTile( unsigned int tileID )
{
	if( !g ) return;
	Position pos( tileID );
	auto sp = g->spm()->getStockpileAtPos( pos );
	if ( sp )
	{
		emit signalOpenStockpileWindow( sp->id() );
		onUpdateStockpileInfo( sp->id() );
	}
}

/// @brief Opens the stockpile window for a given stockpile UID and pushes a fresh info payload.
/// @param stockpileID Stockpile UID.
void AggregatorStockpile::onOpenStockpileInfo( unsigned int stockpileID )
{
	if( !g ) return;
	emit signalOpenStockpileWindow( stockpileID );
	onUpdateStockpileInfo( stockpileID );
}

/// @brief Re-aggregates and emits signalUpdateInfo for the given stockpile, if it exists.
/// @param stockpileID Stockpile UID.
void AggregatorStockpile::onUpdateStockpileInfo( unsigned int stockpileID )
{
	if( !g ) return;
	if ( aggregate( stockpileID ) )
	{
		emit signalUpdateInfo( m_info );
	}
    else emit signalStockpileRejected(stockpileID,"This stockpile no longer exists.");
}

/// @brief Fills m_info with the current stockpile state (basic fields + per-entry summary).
/// @param stockpileID Stockpile UID.
/// @return true if the stockpile exists, false otherwise.
bool AggregatorStockpile::aggregate( unsigned int stockpileID )
{
	if( !g ) return false;
	auto sp = g->spm()->getStockpile( stockpileID );
	if ( sp )
	{
		m_info.stockpileID       = stockpileID;
		m_info.name              = sp->name();
		m_info.priority          = sp->priority();
		m_info.maxPriority       = g->spm()->maxPriority();
		m_info.suspended         = !sp->active();
		m_info.allowPullFromHere = sp->allowsPull();
		m_info.pullFromOthers    = sp->pullsOthers();

		m_info.filter = sp->filter();
		m_info.capacity = 0;
		m_info.itemCount = 0;
		m_info.reserved = 0;
			m_info.summary.clear();
			m_info.templateNames.clear();
			for ( const auto& value : loadStockpileTemplates() )
			{
				const auto name = value.toMap().value( "Name" ).toString().trimmed();
				if ( !name.isEmpty() ) m_info.templateNames.push_back( name );
			}
			std::sort( m_info.templateNames.begin(), m_info.templateNames.end(), []( const QString& a, const QString& b )
				{ return QString::compare( a, b, Qt::CaseInsensitive ) < 0; } );
		QMap<QPair<QString, QString>, int> counts;
		for ( const auto& field : sp->getFields() )
		{
			if ( !field )
				continue;
			m_info.capacity += field->capacity;
			m_info.itemCount += field->items.size();
			m_info.reserved += field->reservedItems.size();
			for ( const auto itemID : field->items )
			{
				if ( g->inv()->itemExists( itemID ) )
					++counts[{ g->inv()->itemSID( itemID ), g->inv()->materialSID( itemID ) }];
			}
		}
		for ( auto it = counts.cbegin(); it != counts.cend(); ++it )
		{
			ItemsSummary is;
			is.itemSID      = it.key().first;
			is.materialSID  = it.key().second;
			is.itemName     = S::s( "$ItemName_" + is.itemSID );
			is.materialName = S::s( "$MaterialName_" + is.materialSID );
				is.count        = it.value();
				is.total        = static_cast<int>( g->inv()->itemCountDetailed( is.itemSID, is.materialSID ).total );
				m_info.summary.append( is );
		}

		return true;
	}
	return false;
}

/// @brief Marks the content summary dirty if the open stockpile matches; the next tick hook
///        will actually re-emit it.
/// @param stockpileID Stockpile UID whose content changed.
void AggregatorStockpile::onUpdateStockpileContent( unsigned int stockpileID )
{
	if( !g ) return;
	if ( m_info.stockpileID == stockpileID )
	{
		m_contentDirty = true;
	}
}

/// @brief Post-tick hook: if the open stockpile's content is dirty, rebuilds the summary
///        rows and emits signalUpdateContent. Throttled to once per tick to avoid spam.
void AggregatorStockpile::onUpdateAfterTick()
{
	if( !g ) return;
	if ( m_info.stockpileID && m_contentDirty )
	{
		if ( aggregate( m_info.stockpileID ) )
		{
			emit signalUpdateContent( m_info );
			m_contentDirty = false;
		}
		else
			m_info.stockpileID = 0;
	}
}

/// @brief Applies basic stockpile edits (name, priority, suspended, pull, allow-pull) from the GUI.
/// @param stockpileID Stockpile UID.
/// @param name        New display name.
/// @param priority    New priority index.
/// @param suspended   New suspended flag.
/// @param pull        Pull-from-others flag.
/// @param allowPull   Allow-others-to-pull flag.
void AggregatorStockpile::onSetBasicOptions( unsigned int stockpileID, QString name, int priority, bool suspended, bool pull, bool allowPull )
{
	if( !g ) return;
	auto sp = g->spm()->getStockpile( stockpileID );
	if ( sp )
	{
		//qDebug() << stockpileID << name << priority << suspended << pull << allowPull;
		sp->setName( g->spm()->uniqueName(name,stockpileID) );
		g->spm()->setPriority( stockpileID, priority );
		sp->setActive( !suspended );
		sp->setAllowPull( allowPull );
		sp->setPullOthers( pull );
	}
}

/// @brief Toggles a filter entry in the stockpile's item filter tree at the specified level
///        (category / group / item / material) and pushes a fresh info payload.
/// @param stockpileID Stockpile UID.
/// @param active      True to enable, false to disable.
/// @param category    Category key.
/// @param group       Group key (empty to apply to whole category).
/// @param item        Item key (empty to apply to whole group).
/// @param material    Material key (empty to apply to whole item).
void AggregatorStockpile::onSetActive( unsigned int stockpileID, bool active, QString category, QString group, QString item, QString material )
{
	onSetActiveBatch( stockpileID, active, { QStringList { std::move( category ), std::move( group ), std::move( item ), std::move( material ) } } );
}

void AggregatorStockpile::onSetActiveBatch( unsigned int stockpileID, bool active, const QList<QStringList>& paths )
{
	if( !g || paths.empty() ) return;
    if(!g->spm()->getStockpile(stockpileID)) {emit signalStockpileRejected(stockpileID,"This stockpile no longer exists.");return;}
	auto sp = g->spm()->getStockpile( stockpileID );
	if ( sp )
	{
		auto filter = sp->pFilter();
		if ( filter )
		{
			for ( const auto& path : paths )
			{
				if ( path.size() != 4 || path[0].isEmpty() ) continue;
				if ( path[1].isEmpty() )
					filter->setCheckState( path[0], active );
				else if ( path[2].isEmpty() )
					filter->setCheckState( path[0], path[1], active );
				else if ( path[3].isEmpty() )
					filter->setCheckState( path[0], path[1], path[2], active );
				else
					filter->setCheckState( path[0], path[1], path[2], path[3], active );
			}
		}
		if( g->tutorial() ) g->tutorial()->observeStockpileAllowRule( stockpileID );
		onUpdateStockpileInfo( stockpileID );
	}
}

void AggregatorStockpile::onSaveFilterTemplate( unsigned int stockpileID, QString name, bool replaceExisting )
{
	if ( !g ) return;
	name = name.trimmed();
	if ( name.isEmpty() || name.size() > 48 ) {emit signalStockpileRejected(stockpileID,"Enter a template name of 1 to 48 characters.");return;}
	const auto stockpile = g->spm()->getStockpile( stockpileID );
	if ( !stockpile ) {emit signalStockpileRejected(stockpileID,"This stockpile no longer exists.");return;}
	auto templates = loadStockpileTemplates();
	QVariantMap saved { { "Name", name }, { "Filter", stockpile->filter().serialize() } };
	bool replaced = false;
	for ( auto& value : templates )
		if ( value.toMap().value( "Name" ).toString().compare( name, Qt::CaseInsensitive ) == 0 )
		{
			if(!replaceExisting) {emit signalStockpileRejected(stockpileID,"That template name already exists. Use Update existing.");return;}
            value = saved;
			replaced = true;
			break;
		}
	if(!replaced && replaceExisting) {emit signalStockpileRejected(stockpileID,"The saved template no longer exists.");return;}
    if ( !replaced ) templates.push_back( saved );
	if(!saveStockpileTemplates( templates )) {emit signalStockpileRejected(stockpileID,"Could not save the template. Your current rules are unchanged.");return;}
	onUpdateStockpileInfo( stockpileID );
}

void AggregatorStockpile::onApplyFilterTemplate( unsigned int stockpileID, QString name )
{
	if ( !g ) return;
	name = name.trimmed();
	const auto stockpile = g->spm()->getStockpile( stockpileID );
	if(!stockpile) {emit signalStockpileRejected(stockpileID,"This stockpile no longer exists.");return;}
    if(name.isEmpty())return;
	for ( const auto& value : loadStockpileTemplates() )
	{
		const auto saved = value.toMap();
		if ( saved.value( "Name" ).toString().compare( name, Qt::CaseInsensitive ) != 0 ) continue;
		*stockpile->pFilter() = Filter( saved.value( "Filter" ).toMap() );
		if( g->tutorial() ) g->tutorial()->observeStockpileAllowRule( stockpileID );
		onUpdateStockpileInfo( stockpileID );
		return;
	}
}

/// @brief Clears the currently open stockpile when the GUI closes the window.
void AggregatorStockpile::onCloseWindow()
{
	m_info.stockpileID = 0;
}
