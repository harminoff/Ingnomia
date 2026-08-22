#version 430 core

#define TF_WATER       0x00008000u
#define TF_OCCUPIED    0x04000000u

#define WF_NORTH       0x01u
#define WF_EAST        0x02u
#define WF_SOUTH       0x04u
#define WF_WEST        0x08u

layout(location = 0) in vec3 aPos;

layout(location = 0) noperspective out vec2 vTexCoords;
layout(location = 1) flat out uint vFluidLevel;
layout(location = 2) flat out uint vWaterFlow;
layout(location = 3) flat out uvec4 vNeighborLevels;
layout(location = 4) flat out uvec3 vTile;
layout(location = 5) out vec2 vScreenUv;
layout(location = 6) flat out vec3 vBaseNormal;
layout(location = 7) flat out uint vIsSide;
// A single world-space coordinate for both sides of every adjacent surface
// quad.  The old pass rebuilt UVs from vTile + vTexCoords in the fragment
// shader, which reset the pattern at every tile and made a river read as a
// collection of animated squares.
layout(location = 8) noperspective out vec2 vWaterUv;

uniform uvec3 uWorldSize;
uniform mat4 uTransform;
uniform int uWorldRotation;
uniform uvec3 uRenderMin;
uniform uvec3 uRenderMax;
uniform int uWaterFace;

// DO NOT CHANGE, must match game internal layout.
struct TileData {
	uint flags;
	uint flags2;
	uint floorSpriteUID;
	uint wallSpriteUID;
	uint itemSpriteUID;
	uint creatureSpriteUID;
	uint jobSpriteFloorUID;
	uint jobSpriteWallUID;
	uint packedLevels;
	int creatureOffsetX;
	int creatureOffsetY;
	int creatureOffsetZ;
	uint creatureMotionTick;
	uint creatureMotionDurationTicks;
};

layout(std430, binding = 0) readonly restrict buffer tileData1
{
	TileData data[];
} tileData;

uvec3 rotate(uvec3 pos)
{
	uvec3 ret = uvec3(0, 0, pos.z);
	switch ( uWorldRotation % 4 )
	{
		case 0: ret.xy = pos.xy; break;
		case 1: ret.x = uWorldSize.y - pos.y - 1; ret.y = pos.x; break;
		case 2: ret.x = uWorldSize.x - pos.x - 1; ret.y = uWorldSize.y - pos.y - 1; break;
		case 3: ret.x = pos.y; ret.y = uWorldSize.x - pos.x - 1; break;
	}
	return ret;
}

uint tileID(uvec3 pos)
{
	return pos.x + pos.y * uWorldSize.x + pos.z * uWorldSize.x * uWorldSize.y;
}

uint tileID(ivec3 pos)
{
	return uint(pos.x + pos.y * int(uWorldSize.x) + pos.z * int(uWorldSize.x * uWorldSize.y));
}

bool inWorld(ivec3 pos)
{
	const ivec3 size = ivec3(uWorldSize);
	return all(greaterThanEqual(pos, ivec3(0))) && all(lessThan(pos, size));
}

bool blocksSurface(uint index)
{
	const TileData tile = tileData.data[index];
	return tile.itemSpriteUID != 0u || tile.wallSpriteUID != 0u ||
		tile.jobSpriteWallUID != 0u || (tile.flags & TF_OCCUPIED) != 0u;
}

uint waterLevel(ivec3 pos)
{
	if (!inWorld(pos))
		return 0u;
	const uint index = tileID(pos);
	const TileData tile = tileData.data[index];
	// A solid object occupies the tile above the floor.  Do not let its stale
	// fluid byte create a false shoreline or a sheet of water over the object.
	if (blocksSurface(index))
		return 0u;
	// Fluid mass is authoritative. TF_WATER is a derived compatibility marker
	// and older saves may contain a non-zero level without that bit.
	return tile.packedLevels & 0xffu;
}

ivec3 visibleSideOffset(int face)
{
	const int rotation = uWorldRotation % 4;
	if (rotation == 0) return face == 1 ? ivec3(0, 1, 0) : ivec3(1, 0, 0);
	if (rotation == 1) return face == 1 ? ivec3(0, 1, 0) : ivec3(-1, 0, 0);
	if (rotation == 2) return face == 1 ? ivec3(0, -1, 0) : ivec3(-1, 0, 0);
	return face == 1 ? ivec3(0, -1, 0) : ivec3(1, 0, 0);
}

vec3 project(uvec3 pos, vec2 offset)
{
	float z = float(pos.z + pos.x + pos.y);
	return vec3(
		offset.x * 32.0 + 16.0 * float(pos.x) - 16.0 * float(pos.y),
		offset.y * 64.0 - (8.0 * float(pos.y) + 8.0 * float(pos.x)) -
			(float(uRenderMax.z) - float(pos.z)) * 20.0 - 12.0,
		z);
}

void main()
{
	uint id = gl_InstanceID;
	const uvec3 renderVolume = uRenderMax - uRenderMin + uvec3(1);
	const uint pitchZ = renderVolume.x * renderVolume.y;
	const uint pitchY = renderVolume.x;
	uvec3 localIndex;
	localIndex.z = id / pitchZ;
	id = id % pitchZ;
	localIndex.y = id / pitchY;
	id = id % pitchY;
	localIndex.x = id;

	// Water is a visual surface, so do not reverse the instance order used by
	// the opaque/transparent terrain passes.
	const uvec3 tile = uRenderMin + localIndex;
	const uint index = tileID(tile);
	const TileData current = tileData.data[index];
	const uint fluid = current.packedLevels & 0xffu;
	const bool boundary = tile.x == 0u || tile.y == 0u || tile.z == 0u ||
		tile.x + 1u >= uWorldSize.x || tile.y + 1u >= uWorldSize.y || tile.z + 1u >= uWorldSize.z;
	// Do not require the legacy marker here. A cell with fluid mass must render
	// even when it came from a save generated before TF_WATER was persisted.
	// Objects own their tile surface and must remain in front of/above water.
	const bool hasWater = fluid > 0u && !boundary && !blocksSurface(index);

	const ivec3 currentTile = ivec3(tile);
	const ivec3 aboveTile = currentTile + ivec3(0, 0, 1);
	const uint aboveIndex = inWorld(aboveTile) ? tileID(aboveTile) : index;
	const bool aboveBlocked = inWorld(aboveTile) && blocksSurface(aboveIndex);
	const uint aboveFluid = inWorld(aboveTile) ? (tileData.data[aboveIndex].packedLevels & 0xffu) : 0u;
	const bool exposed = hasWater && !aboveBlocked && aboveFluid == 0u;

	const ivec3 northTile = currentTile + ivec3(0, -1, 0);
	const ivec3 eastTile = currentTile + ivec3(1, 0, 0);
	const ivec3 southTile = currentTile + ivec3(0, 1, 0);
	const ivec3 westTile = currentTile + ivec3(-1, 0, 0);
	const uint north = inWorld(northTile) ? (tileData.data[tileID(northTile)].packedLevels & 0xffu) : 0u;
	const uint east = inWorld(eastTile) ? (tileData.data[tileID(eastTile)].packedLevels & 0xffu) : 0u;
	const uint south = inWorld(southTile) ? (tileData.data[tileID(southTile)].packedLevels & 0xffu) : 0u;
	const uint west = inWorld(westTile) ? (tileData.data[tileID(westTile)].packedLevels & 0xffu) : 0u;

	vTexCoords = vec2(aPos.x, 1.0 - aPos.y);
	vFluidLevel = fluid;
	vWaterFlow = (current.packedLevels >> 24) & 0xffu;
	vNeighborLevels = uvec4(north, east, south, west);
	vTile = tile;
	vIsSide = uint(uWaterFace != 0);
	vBaseNormal = vec3(0.0, 0.0, 1.0);

	vec3 worldPos;
	vec2 surfaceUv;
	bool visible = exposed;
	if (uWaterFace == 0)
	{
		const bool eastCorner = aPos.x > 0.5;
		const bool northCorner = aPos.y > 0.35;
		const ivec3 xOffset = eastCorner ? ivec3(1, 0, 0) : ivec3(-1, 0, 0);
		const ivec3 yOffset = northCorner ? ivec3(0, -1, 0) : ivec3(0, 1, 0);
		const float cornerFluid = 0.25 * float(fluid + waterLevel(currentTile + xOffset) +
			waterLevel(currentTile + yOffset) + waterLevel(currentTile + xOffset + yOffset));
		worldPos = project(rotate(tile), aPos.xy);
		worldPos.y += (cornerFluid / 10.0 - 1.0) * 3.0;
		// The floor vertices use an isometric y range of 0.2..0.5. Normalize
		// that range before adding the tile coordinate so neighboring quads
		// share exactly the same water texture coordinate at their seam.
		surfaceUv = vec2(aPos.x, clamp((aPos.y - 0.2) / 0.3, 0.0, 1.0));
	}
	else
	{
		const ivec3 sideOffset = visibleSideOffset(uWaterFace);
		const uint neighborFluid = waterLevel(currentTile + sideOffset);
		// A connected water body should not become a stack of little walls just
		// because the solver has a small per-cell level difference.  Keep side
		// geometry for the actual shoreline and for a large drop (waterfall),
		// while the top surface interpolates ordinary wet-to-wet differences.
		const bool dryShore = neighborFluid == 0u;
		const bool largeDrop = fluid > neighborFluid + 4u;
		visible = hasWater && fluid > neighborFluid && (dryShore || largeDrop);
		const float vertical = clamp((aPos.y - 0.2) / 0.6, 0.0, 1.0);
		worldPos = project(rotate(tile), vec2(aPos.x, 0.2));
		const float topOffset = (float(fluid) / 10.0 - 1.0) * 3.0;
		const float dropPixels = 4.0 + 12.0 * clamp(float(fluid - neighborFluid) / 10.0, 0.0, 1.0);
		worldPos.y += topOffset - (1.0 - vertical) * dropPixels;
		vTexCoords = vec2(aPos.x, vertical);
		surfaceUv = vec2(aPos.x, vertical);
		vBaseNormal = normalize(vec3(float(sideOffset.x), float(sideOffset.y), 0.35));
	}
	// Keep the DuDv and normal maps in world space.  This is the important
	// continuity fix: time can move the surface, but the pattern cannot restart
	// at each tile boundary.
	vWaterUv = (vec2(tile.xy) + surfaceUv) * 0.045;
	vTexCoords = surfaceUv;
	worldPos.z += 0.01;
	gl_Position = uTransform * vec4(worldPos, 1.0);
	vScreenUv = gl_Position.xy / gl_Position.w * 0.5 + 0.5;

	if (!visible)
		gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
}
