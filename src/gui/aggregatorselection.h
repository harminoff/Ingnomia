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
/** @file aggregatorselection.h
 *  @brief Aggregator handling the placement cursor and rectangular selections: tracks mouse
 *         hover, click, size, and rotation, and emits SelectionData previews to the renderer.
 */
#pragma once

#include "aggregatorrenderer.h"

#include "../base/position.h"

#include <QObject>

/// @brief Converts mouse input and selection parameters into world-space tile previews for
///        designation and build commands. Holds a grid of preview SelectionData that the
///        renderer draws as the placement cursor.
class AggregatorSelection : public QObject
{
	Q_OBJECT

public:
	AggregatorSelection( QObject* parent = nullptr );
	~AggregatorSelection();

private:
	Position calcCursor( int mouseX, int mouseY, bool isFloor, bool useViewLevel ) const;
    void updateSelection();
    void updateInspection();
    bool m_inspectionActive = false;
    unsigned int m_inspectedTile = 0;
    unsigned int posToInt( Position pos, quint8 rotation );

    int m_width = 0;       ///< Viewport width in pixels.
    int m_height = 0;      ///< Viewport height in pixels.
    int m_moveX = 0;       ///< Camera X offset in pixels.
    int m_moveY = 0;       ///< Camera Y offset in pixels.
    float m_scale = 1.0;   ///< Camera zoom factor.
    int m_rotation = 0;    ///< Camera rotation index (0–3).

    // Keep the latest pointer sample so a newly selected tool can immediately
    // rebuild its placement preview. Without this, selecting Mine/Build after
    // the mouse has stopped moving leaves the renderer with an empty preview
    // until another mouse event arrives.
    bool m_hasMousePosition = false;
    int m_mouseX = 0;
    int m_mouseY = 0;
    bool m_mouseShift = false;
    bool m_mouseCtrl = false;

    Position m_cursorPos;  ///< Current world-space cursor tile.

    QMap<unsigned int, SelectionData> m_selectionData; ///< Per-tile preview grid keyed by encoded tile+rot ID.

public slots:
    void onSetInspection( bool active );
    void onActionChanged( const QString action );
    void onUpdateCursorPos( const QString pos );
    void onUpdateFirstClick( const QString pos );
    void onUpdateSize( const QString size );

    void onRenderParams( int width, int height, int moveX, int moveY, float scale, int rotation );
    void onMouse( int mouseX, int mouseY, bool shift, bool ctrl );
    void onLeftClick( bool shift, bool ctrl );
    void onRightClick();
    void onCancelSelection();
    void onRotateSelection();

signals:
    void signalInspectionChanged( bool active );
    void signalInspectTile( unsigned int tileID );
    void signalAction( const QString action );
    void signalCursorPos( const QString pos );
    void signalFirstClick( const QString pos );
    void signalSize( const QString size );

	void signalSelectTile( unsigned int );
	void signalSelectCreature( unsigned int );

    void signalUpdateSelection( const QMap<unsigned int, SelectionData>& data, bool noDepthTest );
};
