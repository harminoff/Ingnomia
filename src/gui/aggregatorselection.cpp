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
/** @file aggregatorselection.cpp
 *  @brief AggregatorSelection implementation: mouse-to-tile ray casting, cursor preview grid
 *         construction, and click/rotate dispatch to Global::sel (the active selection action).
 */
#include "aggregatorselection.h"

#include "../base/db.h"
#include "../base/gamestate.h"
#include "../base/global.h"
#include "../base/selection.h"
#include "../base/util.h"
#include "../game/creaturemanager.h"
#include "../game/game.h"
#include "../game/gnomemanager.h"
#include "../game/world.h"
#include "../gfx/spritefactory.h"
#include "eventconnector.h"
#include "aggregatortileinfo.h"
#include "isometricplacement.h"

#include <QDebug>

static bool hasVisibleWallTop( const Position& pos );

/// @brief Constructs the AggregatorSelection and registers SelectionData as a metatype.
/// @param parent Qt parent object.
AggregatorSelection::AggregatorSelection( QObject* parent ) :
	QObject( parent )
{
	qRegisterMetaType<SelectionData>();
}

/// @brief Destructor.
AggregatorSelection::~AggregatorSelection()
{
}

/// @brief Relays an action-string change to the GUI (e.g. "DigFloor").
/// @param action New action string.
void AggregatorSelection::onActionChanged( const QString action )
{
	if ( !action.isEmpty() && m_inspectionActive ) onSetInspection( false );
	emit signalAction( action );

	// Selection::setAction() clears the current tile list before publishing the
	// new action. The old path only forwarded this signal to the UI, so the
	// renderer stayed empty until a later mouse move happened. Re-evaluate the
	// cursor immediately when we have a pointer sample, which keeps tool
	// activation and the world preview in sync even with a stationary cursor.
	if ( action.isEmpty() )
	{
		m_selectionData.clear();
		emit signalUpdateSelection( m_selectionData, false );
	}
	else if ( m_hasMousePosition )
	{
		onMouse( m_mouseX, m_mouseY, m_mouseShift, m_mouseCtrl );
	}
}

/// @brief Relays the current cursor position string to the GUI.
/// @param pos Formatted cursor position.
void AggregatorSelection::onUpdateCursorPos( const QString pos )
{
	emit signalCursorPos( pos );
}

/// @brief Relays the first-click anchor position (start of a drag rect) to the GUI.
/// @param pos Formatted anchor position.
void AggregatorSelection::onUpdateFirstClick( const QString pos )
{
	emit signalFirstClick( pos );
}

/// @brief Relays the current drag rect size to the GUI.
/// @param size Formatted size string.
void AggregatorSelection::onUpdateSize( const QString size )
{
	emit signalSize( size );
}

/// @brief Caches the renderer's current view parameters so the cursor raycaster can project
///        screen coordinates into world tiles.
/// @param width    Viewport width in pixels.
/// @param height   Viewport height in pixels.
/// @param moveX    Camera X offset.
/// @param moveY    Camera Y offset.
/// @param scale    Camera zoom.
/// @param rotation Camera rotation index (0–3).
void AggregatorSelection::onRenderParams( int width, int height, int moveX, int moveY, float scale, int rotation )
{
	m_width    = width;
	m_height   = height;
	m_moveX    = moveX;
	m_moveY    = moveY;
	m_scale    = scale;
	m_rotation = rotation;
}

/// @brief Handles mouse move: recomputes the cursor tile, updates the active Selection,
///        and refreshes the preview grid.
/// @param mouseX Viewport-relative mouse X.
/// @param mouseY Viewport-relative mouse Y.
/// @param shift  True if Shift is held (locks Z to view level).
/// @param ctrl   True if Ctrl is held (selection modifier).
void AggregatorSelection::onMouse( int mouseX, int mouseY, bool shift, bool ctrl )
{
	m_mouseX = mouseX;
	m_mouseY = mouseY;
	m_mouseShift = shift;
	m_mouseCtrl = ctrl;
	m_hasMousePosition = true;

	if ( Global::sel )
	{
		m_cursorPos = calcCursor( mouseX, mouseY, Global::sel->isFloor(), shift );
		onUpdateCursorPos( m_cursorPos.toString() );
		if ( m_inspectionActive ) return;
		Global::sel->updateSelection( m_cursorPos, shift, ctrl );
		Global::sel->setControlActive( ctrl );
		updateSelection();
	}
}

void AggregatorSelection::onSetInspection( bool active )
{
	if ( m_inspectionActive == active ) return;
	if ( active && Global::sel ) Global::sel->clear();
	m_inspectionActive = active;
	if ( Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->setLiveInspection( active );
	m_inspectedTile = 0;
	m_selectionData.clear();
	if ( !active && Global::eventConnector ) Global::eventConnector->aggregatorTileInfo()->clearSelection();
	emit signalUpdateSelection( m_selectionData, false );
	emit signalInspectionChanged( active );
}

void AggregatorSelection::updateInspection()
{
	auto* game = Global::eventConnector ? Global::eventConnector->game() : nullptr;
	if ( !game || !m_inspectionActive ) return;
	const bool wallTop = hasVisibleWallTop( m_cursorPos );
	SelectionData cell;
	cell.pos = m_cursorPos;
	cell.spriteID = game->sf()->createSprite( wallTop ? "SelectionWallTop" : "SelectionFloorTop", { "None" } )->uID;
	cell.isFloor = !wallTop;
	cell.valid = true;
	m_selectionData.clear();
	m_selectionData.insert( posToInt( cell.pos, m_rotation ), cell );
	emit signalUpdateSelection( m_selectionData, false );
	if ( m_inspectedTile != m_cursorPos.toInt() )
	{
		m_inspectedTile = m_cursorPos.toInt();
		emit signalInspectTile( m_inspectedTile );
	}
}

/// @brief Handles left-click: either commits the active Selection's action on the current
///        cursor tile or emits a plain tile-selected signal for info windows.
/// @param shift True if Shift is held.
/// @param ctrl  True if Ctrl is held.
void AggregatorSelection::onLeftClick( bool shift, bool ctrl )
{
	if ( m_inspectionActive ) { updateInspection(); return; }
	if ( Global::sel )
	{
		if ( Global::sel->hasAction() )
		{
			Global::sel->leftClick( m_cursorPos, shift, ctrl );
			Global::sel->updateSelection( m_cursorPos, shift, ctrl );
			updateSelection();
		}
		else
		{
			unsigned int tileID = m_cursorPos.toInt();
			if ( Global::eventConnector && Global::eventConnector->game() )
			{
				auto* game = Global::eventConnector->game();
				if ( const auto gnomes = game->gm()->gnomesAtPosition( m_cursorPos ); !gnomes.isEmpty() )
				{
					emit signalSelectCreature( gnomes.front()->id() );
					return;
				}
				if ( const auto animals = game->cm()->animalsAtPosition( m_cursorPos ); !animals.isEmpty() )
				{
					emit signalSelectCreature( animals.front()->id() );
					return;
				}
				if ( const auto monsters = game->cm()->monstersAtPosition( m_cursorPos ); !monsters.isEmpty() )
				{
					emit signalSelectCreature( monsters.front()->id() );
					return;
				}
			}
			emit signalSelectTile( tileID );
		}
	}
}

/// @brief Handles right-click: cancels the active Selection or removes the last anchor.
void AggregatorSelection::onRightClick()
{
	if ( m_inspectionActive ) { onSetInspection( false ); return; }
	if ( Global::sel )
	{
		Global::sel->rightClick( m_cursorPos );
		updateSelection();
    }
}

/// @brief Fully clears a UI-requested tool cancel without changing world right-click anchor semantics.
void AggregatorSelection::onCancelSelection()
{
    if ( Global::sel )
    {
        Global::sel->clear();
        updateSelection();
    }
}

/// @brief Rotates the active Selection 90° and refreshes the preview grid.
void AggregatorSelection::onRotateSelection()
{
	if ( Global::sel )
	{
		Global::sel->rotate();
		updateSelection();
	}
}

/// @brief Returns whether the tile at @p pos has a wall that the cursor ray caster should
///        snap to (solid wall or a wall-overlay job).
/// @param pos World position.
/// @return true if the tile exposes a selectable wall.
static bool isSelectableWall( const Position& pos )
{
	const auto w     = Global::eventConnector->game()->w();
	const auto& tile = w->getTile( pos );
	if ( tile.wallType & WallType::WT_SOLIDWALL )
	{
		return true;
	}
	if ( Global::showJobs && w->jobSprite( pos ).contains( "Wall" ) )
	{
		return true;
	}
	return false;
}

/// @brief Returns whether the tile at @p pos exposes a selectable floor: a real floor, an
///        overlay floor job, or (if @p snapToWallBelow is set) a wall on the tile below.
/// @param pos              World position.
/// @param snapToWallBelow  If true, accept walls on the tile directly below as floors.
/// @return true if the tile exposes a selectable floor.
static bool isSelectableFloor( const Position& pos, bool snapToWallBelow )
{
	const auto w     = Global::eventConnector->game()->w();
	const auto& tile = w->getTile( pos );
	if ( tile.floorType != FloorType::FT_NOFLOOR )
	{
		return true;
	}
	const auto& tileBelow = w->getTile( pos.belowOf() );
	if ( snapToWallBelow )
	{
		if ( isSelectableWall( pos.belowOf() ) )
		{
			return true;
		}
	}
	if ( Global::showJobs && w->jobSprite( pos ).contains( "Floor" ) )
	{
		return true;
	}
	return false;
}

// A solid underground tile is presented as a cube, not its obscured floor.
// This predicate is shared by picking and excavation preview construction.
static bool hasVisibleWallTop( const Position& pos )
{
	return !Global::wallsLowered
		&& ( Global::eventConnector->game()->w()->getTile( pos ).wallType & WT_SOLIDWALL );
}

/// @brief Projects a screen-space mouse position into a world tile using the current camera
///        rotation/zoom/offset. Walks down through z-levels until a selectable floor is found
///        (or @p useViewLevel forces the cursor to stay at the current view level). Adjusts
///        for tile occlusion by walls when walls-lowered mode is off.
/// @param mouseX       Viewport X in pixels.
/// @param mouseY       Viewport Y in pixels.
/// @param isFloor      True if the current action targets floors (affects snapping).
/// @param useViewLevel True to force cursor Z to the current view level (Shift held).
/// @return World-space tile position under the mouse.
Position AggregatorSelection::calcCursor( int mouseX, int mouseY, bool isFloor, bool useViewLevel ) const
{
	if ( !Global::sel || !Global::eventConnector || !Global::eventConnector->game() || !Global::eventConnector->game()->w() )
	{
		return Position( 0, 0, 0 );
	}

	int dim = Global::dimX;
	if ( dim == 0 )
	{
		return Position( 0, 0, 0 );
	}
	int viewLevel = GameState::viewLevel;
	const double scale = qMax( 0.0001, static_cast<double>( m_scale ) );
	const double halfWidth = static_cast<double>( m_width ) / ( 2.0 * scale );
	const double halfHeight = static_cast<double>( m_height ) / ( 2.0 * scale );
	const double screenOriginX = static_cast<double>( m_moveX ) + halfWidth;
	const double screenOriginY = static_cast<double>( m_moveY ) + halfHeight;
	const int renderedWidth = ( m_rotation == 1 || m_rotation == 3 ) ? Global::dimY : Global::dimX;
	const int renderedHeight = ( m_rotation == 1 || m_rotation == 3 ) ? Global::dimX : Global::dimY;

	// At each depth, test the visible wall-top diamond before the floor plane.
	// Picking a solid tile by its hidden floor selects the cube one diagonal
	// tile behind the pointer, even when a floor-shaped ghost appears aligned.
	const double renderedMouseX = static_cast<double>( mouseX ) / scale;
	const double renderedMouseY = static_cast<double>( mouseY ) / scale;

	bool zFloorFound = false;
	int origViewLevel = viewLevel;
	Position cursorPos;
	int zDiff = 0;
	while ( !zFloorFound && zDiff < 20 )
	{
		zDiff = origViewLevel - viewLevel;

		auto pickSurface = [&]( double surfaceHeight ) {
			const auto renderedTile = ingnomia::ui::nearestIsometricTile(
				renderedMouseX, renderedMouseY, screenOriginX, screenOriginY,
				zDiff, renderedWidth, renderedHeight, surfaceHeight );
			const int bestRenderedX = renderedTile.x;
			const int bestRenderedY = renderedTile.y;

			const int candidateZ = qBound( 0, viewLevel, Global::dimZ - 1 );
			switch ( m_rotation )
			{
				case 1:
					cursorPos = Position( bestRenderedY, Global::dimY - bestRenderedX - 1, candidateZ );
					break;
				case 2:
					cursorPos = Position( Global::dimX - bestRenderedX - 1, Global::dimY - bestRenderedY - 1, candidateZ );
					break;
				case 3:
					cursorPos = Position( Global::dimX - bestRenderedY - 1, bestRenderedX, candidateZ );
					break;
				default:
					cursorPos = Position( bestRenderedX, bestRenderedY, candidateZ );
					break;
			}
			cursorPos.x = qBound( 0, cursorPos.x, Global::dimX - 1 );
			cursorPos.y = qBound( 0, cursorPos.y, Global::dimY - 1 );
		};

		pickSurface( 28.0 );
		if ( hasVisibleWallTop( cursorPos ) )
		{
			zFloorFound = true;
			break;
		}
		pickSurface( 12.0 );

		if ( cursorPos.z > 0 )
		{
			if ( isSelectableFloor(cursorPos, isFloor ) )
			{
				zFloorFound = true;
			}
			else if ( useViewLevel )
			{
				zFloorFound = true;
			}
			else
			{
				--viewLevel;
				if ( viewLevel == 1 )
				{
					zFloorFound = true;
				}
			}
		}
		else
		{
			zFloorFound = true;
		}
	}

	if ( useViewLevel )
	{
		cursorPos.z = viewLevel;
	}
	return cursorPos;
}

/// @brief Rebuilds the preview sprite grid for the active action: picks sprites per action
///        kind (construction, workshop, furniture, or generic tile action), applies the
///        current rotation to their offsets, and marks each tile as valid/invalid based on
///        the Selection's per-tile hit test. Emits signalUpdateSelection to the renderer.
void AggregatorSelection::updateSelection()
{
	if ( Global::sel->changed() )
	{
		//DebugScope s( "update selection" );

		QString action = Global::sel->action();
		m_selectionData.clear();
		if ( !action.isEmpty() )
		{
			bool isFloor = Global::sel->isFloor();

			QList<QPair<Position, bool>> selection = Global::sel->getSelection();

			QList<QPair<Sprite*, QPair<Position, unsigned char>>> sprites;
			QList<QPair<Sprite*, QPair<Position, unsigned char>>> spritesInv;

			int rotation = Global::sel->rotation();

			QList<QVariantMap> spriteIDs;

			if ( action == "BuildWall" || action == "BuildFancyWall" || action == "BuildFloor" || action == "BuildFancyFloor" || action == "BuildRamp" || action == "BuildRampCorner" || action == "BuildStairs" )
			{
				spriteIDs = DB::selectRows( "Constructions_Sprites", "ID", Global::sel->itemID() );
			}
			else if ( action == "BuildWorkshop" )
			{
				spriteIDs = DB::selectRows( "Workshops_Components", "ID", Global::sel->itemID() );
			}
			else if ( action == "BuildItem" )
			{
				QVariantMap sprite;
				sprite.insert( "SpriteID", DBH::spriteID( Global::sel->itemID() ) );
				sprite.insert( "Offset", "0 0 0" );
				sprite.insert( "Type", "Furniture" );
				sprite.insert( "Material", Global::sel->material() );
				spriteIDs.push_back( sprite );
			}
			else
			{
				spriteIDs = DB::selectRows( "Actions_Tiles", "ID", action );
			}
			for ( auto asi : spriteIDs )
			{
				QVariantMap entry = asi;
				if ( !entry.value( "SpriteID" ).toString().isEmpty() )
				{
					if ( entry.value( "SpriteID" ).toString() == "none" )
					{
						continue;
					}
					if ( !entry.value( "SpriteIDOverride" ).toString().isEmpty() )
					{
						entry.insert( "SpriteID", entry.value( "SpriteIDOverride" ).toString() );
					}

					// TODO repair rot
					unsigned char localRot = Global::util->rotString2Char( entry.value( "WallRotation" ).toString() );

					Sprite* addSpriteValid = nullptr;

					QStringList mats;
					for ( auto mv : entry.value( "Material" ).toList() )
					{
						mats.push_back( mv.toString() );
					}

					if ( entry.contains( "Material" ) )
					{
						addSpriteValid = Global::eventConnector->game()->sf()->createSprite( entry["SpriteID"].toString(), mats );
					}
					else
					{
						addSpriteValid = Global::eventConnector->game()->sf()->createSprite( entry["SpriteID"].toString(), { "None" } );
					}
					Position offset( 0, 0, 0 );
					if ( entry.contains( "Offset" ) )
					{
						QString os      = entry["Offset"].toString();
						QStringList osl = os.split( " " );

						if ( osl.size() == 3 )
						{
							offset.x = osl[0].toInt();
							offset.y = osl[1].toInt();
							offset.z = osl[2].toInt();
						}
						int rotX = offset.x;
						int rotY = offset.y;
						switch ( rotation )
						{
							case 1:
								offset.x = -1 * rotY;
								offset.y = rotX;
								break;
							case 2:
								offset.x = -1 * rotX;
								offset.y = -1 * rotY;
								break;
							case 3:
								offset.x = rotY;
								offset.y = -1 * rotX;
								break;
						}
					}
					sprites.push_back( QPair<Sprite*, QPair<Position, unsigned char>>( addSpriteValid, { offset, localRot } ) );
					spritesInv.push_back( QPair<Sprite*, QPair<Position, unsigned char>>( addSpriteValid, { offset, localRot } ) );
				}
				else
				{
					QString os      = entry["Offset"].toString();
					QStringList osl = os.split( " " );
					Position offset( 0, 0, 0 );
					if ( osl.size() == 3 )
					{
						offset.x = osl[0].toInt();
						offset.y = osl[1].toInt();
						offset.z = osl[2].toInt();
					}
					int rotX = offset.x;
					int rotY = offset.y;
					switch ( rotation )
					{
						case 1:
							offset.x = -1 * rotY;
							offset.y = rotX;
							break;
						case 2:
							offset.x = -1 * rotX;
							offset.y = -1 * rotY;
							break;
						case 3:
							offset.x = rotY;
							offset.y = -1 * rotX;
							break;
					}
					sprites.push_back( QPair<Sprite*, QPair<Position, unsigned char>>( Global::eventConnector->game()->sf()->createSprite( "SolidSelectionFloor", { "None" } ), { offset, 0 } ) );
					spritesInv.push_back( QPair<Sprite*, QPair<Position, unsigned char>>( Global::eventConnector->game()->sf()->createSprite( "SolidSelectionFloor", { "None" } ), { offset, 0 } ) );
				}
			}
			unsigned int tileID = 0;
			const bool downwardExcavation = action == "DigStairsDown" || action == "DigRampDown" || action == "DigHole";
			for ( auto p : selection )
			{
				if ( downwardExcavation )
				{
					// Keep the entry diamond at the confirmed target and align the
					// item ghost's top face with it. Do not move the job or ray pick.
					const bool wallTop = hasVisibleWallTop( p.first );
					if ( action == "DigStairsDown" || action == "DigRampDown" )
					{
						for ( const auto& component : sprites )
						{
							const Position offset = component.second.first;
							if ( !component.first || offset.z >= 0 )
								continue;
							SelectionData ghost;
							ghost.pos = p.first + offset;
							ghost.pos.setToBounds();
							ghost.spriteID = component.first->uID;
							ghost.localRot = ( rotation + component.second.second ) % 4;
							ghost.valid = p.second;
							ghost.previewYOffset = ingnomia::ui::excavationPreviewLift( wallTop, ghost.pos.z - p.first.z );
							m_selectionData.insert( posToInt( ghost.pos, m_rotation ), ghost );
						}
					}
					SelectionData sd;
					sd.pos = p.first;
					sd.spriteID = Global::eventConnector->game()->sf()->createSprite(
						wallTop ? "SelectionWallTop" : "SelectionFloorTop", { "None" } )->uID;
					sd.isFloor = !wallTop;
					sd.valid = p.second;
					m_selectionData.insert( posToInt( sd.pos, m_rotation ), sd );
					continue;
				}
				if ( p.second )
				{
					for ( auto as : sprites )
					{
						if ( as.first )
						{
							SelectionData sd;
							sd.spriteID = as.first->uID;
							sd.localRot = ( ( rotation + as.second.second ) % 4 );
							sd.pos      = Position( p.first + as.second.first );
							sd.pos.setToBounds();
							sd.isFloor = isFloor;
							sd.valid   = true;
							m_selectionData.insert( posToInt( sd.pos, m_rotation ), sd );
						}
					}
				}
				else
				{
					for ( auto as : spritesInv )
					{
						if ( as.first )
						{
							SelectionData sd;
							sd.spriteID = as.first->uID;
							sd.localRot = ( ( rotation + as.second.second ) % 4 );
							sd.pos      = Position( p.first + as.second.first );
							sd.pos.setToBounds();
							sd.isFloor = isFloor;
							sd.valid   = false;
							m_selectionData.insert( posToInt( sd.pos, m_rotation ), sd );
						}
					}
				}
			}
		}
		bool noDepthTest = ( action == "DigStairsDown" || action == "DigRampDown" );
		emit signalUpdateSelection( m_selectionData, noDepthTest );
	}
}

/// @brief Encodes a world position into a unique integer key that also reflects the camera
///        rotation, so the preview grid is addressable even when rotated.
/// @param pos      World position.
/// @param rotation Camera rotation index (0–3).
/// @return Encoded tile key.
unsigned int AggregatorSelection::posToInt( Position pos, quint8 rotation )
{
	//return x + Global::dimX * y + Global::dimX * Global::dimX * z;

	switch ( rotation )
	{
		case 0:
			return pos.toInt();
			break;
		case 1:
			return pos.x + Global::dimX * ( Global::dimX - pos.y ) + Global::dimX * Global::dimX * pos.z;
			break;
		case 2:
			return ( Global::dimX - pos.x ) + Global::dimX * ( Global::dimX - pos.y ) + Global::dimX * Global::dimX * pos.z;
			break;
		case 3:
			return ( Global::dimX - pos.x ) + Global::dimX * pos.y + Global::dimX * Global::dimX * pos.z;
			break;
	}
	return 0;
}
