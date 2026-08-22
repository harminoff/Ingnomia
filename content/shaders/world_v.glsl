#version 430 core

#define TF_NONE                 0x00000000u
#define TF_WALKABLE             0x00000001u
#define TF_UNDISCOVERED         0x00000002u
#define TF_SUNLIGHT             0x00000004u
#define TF_WET                  0x00000008u
#define TF_GRASS                0x00000010u
#define TF_NOPASS               0x00000020u
#define TF_BLOCKED              0x00000040u
#define TF_DOOR                 0x00000080u
#define TF_STOCKPILE            0x00000100u
#define TF_GROVE                0x00000200u
#define TF_FARM                 0x00000400u
#define TF_TILLED               0x00000800u
#define TF_WORKSHOP             0x00001000u
#define TF_ROOM                 0x00002000u
#define TF_LAVA                 0x00004000u
#define TF_WATER                0x00008000u
#define TF_OCCUPIED             0x04000000u
#define TF_JOB_FLOOR            0x00010000u
#define TF_JOB_WALL             0x00020000u
#define TF_JOB_BUSY_FLOOR       0x00040000u
#define TF_JOB_BUSY_WALL        0x00080000u
#define TF_MOUSEOVER            0x00100000u
#define TF_WALKABLEANIMALS      0x00200000u
#define TF_WALKABLEMONSTERS     0x00400000u
#define TF_PASTURE              0x00800000u
#define TF_INDIRECT_SUNLIGHT    0x01000000u
#define TF_TRANSPARENT          0x40000000u
#define TF_OVERSIZE             0x80000000u

layout(location = 0) in vec3 aPos;

layout(location = 0) noperspective out vec2 vTexCoords;
layout(location = 1) flat out uvec4  block1;
layout(location = 2) flat out uvec4  block2;
layout(location = 3) flat out uvec4  block3;

uniform uvec3 uWorldSize;
uniform mat4 uTransform;
uniform int uWorldRotation;
uniform uvec3 uRenderMin;
uniform uvec3 uRenderMax;

// DO NOT CHANGE, must match game internal layout
struct TileData {
	// TF_ flags 0:32
	uint flags;
	// TF_ flags 32:64
	uint flags2;

	// Sprites are all:
	// spriteID=0:16, spriteFlags=16:32
	uint floorSpriteUID ;
	uint wallSpriteUID;
	
	uint itemSpriteUID;
	uint creatureSpriteUID;

	uint jobSpriteFloorUID;
	uint jobSpriteWallUID;

	// fluidLevel=0:8, lightLevel=8:16, vegetationLevel=16:24, waterFlow=24:32
	uint packedLevels;

	// Render-only previous creature position delta, in world tiles.
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

uniform bool uWallsLowered;
uniform bool uPaintFrontToBack;
uniform bool uCreatureOnly;
uniform float uCreatureInterpolation;
uniform float uCreatureRenderTick;

uvec3 rotate(uvec3 pos)
{
	uvec3 ret = uvec3(0, 0, pos.z);
	switch ( uWorldRotation % 4 )
	{
		case 0:
			ret.xy = pos.xy;
		break;
		case 1:
			ret.x = uWorldSize.y - pos.y - 1;
			ret.y = pos.x;
		break;
		case 2:
			ret.x = uWorldSize.x - pos.x - 1;
			ret.y = uWorldSize.y - pos.y - 1;
		break;
		case 3:
			ret.x = pos.y;
			ret.y = uWorldSize.x - pos.x - 1;
		break;
	}
	return ret;
}

ivec3 rotateOffset( ivec3 offset )
{
	switch ( uWorldRotation % 4 )
	{
		default:
			return offset;
		case 1:
			return ivec3( offset.y, -offset.x, offset.z );
		case 2:
			return ivec3( -offset.x, -offset.y, offset.z );
		case 3:
			return ivec3( -offset.y, offset.x, offset.z );
	}
}

vec3 rotateCreatureDelta( vec3 delta )
{
	switch ( uWorldRotation % 4 )
	{
		default:
			return delta;
		case 1:
			return vec3( -delta.y, delta.x, delta.z );
		case 2:
			return vec3( -delta.x, -delta.y, delta.z );
		case 3:
			return vec3( delta.y, -delta.x, delta.z );
	}
}

vec3 project(uvec3 pos, vec2 offset, bool isWall)
{
	float z = pos.z + pos.x + pos.y;
	return vec3(
		offset.x * 32 + 16 * pos.x - 16 * pos.y,
		offset.y * 64 - ( 8 * pos.y + 8 * pos.x ) - ( uRenderMax.z - pos.z ) * 20 - 12,
		isWall ? z + 0.5 : z
	);
}

uint tileID(uvec3 pos)
{
	return pos.x + pos.y * uWorldSize.x + pos.z * uWorldSize.x * uWorldSize.y;
}

uint tileID( ivec3 pos )
{
	return uint( pos.x + pos.y * int( uWorldSize.x ) + pos.z * int( uWorldSize.x * uWorldSize.y ) );
}

bool inWorld( ivec3 pos )
{
	const ivec3 worldSize = ivec3( uWorldSize );
	return all( greaterThanEqual( pos, ivec3( 0 ) ) ) && all( lessThan( pos, worldSize ) );
}

void main()
{
	uint id = gl_InstanceID;

	const bool uIsWall = aPos.z != 0;

	// Min and max are inclusive in volume
	const uvec3 renderVolume = uRenderMax - uRenderMin + uvec3(1, 1, 1);

	uint pitchZ = renderVolume.x * renderVolume.y;
	uint pitchY = renderVolume.x;

	uvec3 localIndex;
	localIndex.z = id / pitchZ;
	id = id % pitchZ;
	localIndex.y = id / pitchY;
	id = id % pitchY;
	localIndex.x = id;

	bool reverseZ = uPaintFrontToBack;
	bool reverseY = (uWorldRotation == 1 || uWorldRotation == 2) ^^ uPaintFrontToBack;
	bool reverseX = (uWorldRotation == 2 || uWorldRotation == 3) ^^ uPaintFrontToBack;

	uvec3 tile;
	tile.z = reverseZ ? uRenderMax.z - localIndex.z : uRenderMin.z + localIndex.z;
	tile.y = reverseY ? uRenderMax.y - localIndex.y : uRenderMin.y + localIndex.y;
	tile.x = reverseX ? uRenderMax.x - localIndex.x: uRenderMin.x + localIndex.x;

	const uint index = tileID(tile);

	uint vFlags = tileData.data[index].flags;
	uint vFlags2 = tileData.data[index].flags2;
	uint vLightLevel = ( tileData.data[index].packedLevels >> 8 ) & 0xff;
	uint vVegetationLevel = ( tileData.data[index].packedLevels >> 16 ) & 0xff;

	uint floorSprite = 0;
	uint wallSprite = 0;
	uint itemSprite = 0;
	uint jobFloorSprite = 0;
	uint jobWallSprite = 0;
	uint creatureSprite = 0;
	bool containsTransparency;
	if( uIsWall )
	{
		containsTransparency = ( vFlags & TF_TRANSPARENT ) != 0;
	}
	else
	{
		containsTransparency = ( vFlags & TF_TRANSPARENT ) != 0;
	}
	const bool renderingEnabled = uCreatureOnly ? uIsWall : ( uPaintFrontToBack ^^ containsTransparency );
	if( renderingEnabled )
	{
		if ( uCreatureOnly )
		{
			creatureSprite = tileData.data[index].creatureSpriteUID;
		}
		else
		{
			floorSprite = tileData.data[index].floorSpriteUID;
			wallSprite = tileData.data[index].wallSpriteUID;
			itemSprite = tileData.data[index].itemSpriteUID;
			jobFloorSprite = tileData.data[index].jobSpriteFloorUID;
			jobWallSprite = tileData.data[index].jobSpriteWallUID;
		}
	}
	
	uint vFluidLevelPacked1 = 0;
	uint vFluidFlags = 0;

	// Check if rendering is applicable at all for the current tile and rendering pass...
	if(
		((vFlags & TF_UNDISCOVERED) != 0 && uWallsLowered)
		|| (
			!uIsWall
			&& floorSprite == 0
			&& jobFloorSprite == 0
		)
		|| (
			uIsWall
			&& wallSprite == 0
			&& jobWallSprite == 0
			&& itemSprite == 0
			&& creatureSprite == 0
		)
	)
	{
		// ... if not, then skip projection and cull it
		const float NaN = 1 / 0;
		gl_Position = vec4(NaN, NaN, NaN, NaN);
	}
	else
	{
		vec2 vVertexCoords;
		if( ( vFlags & TF_OVERSIZE ) != 0 )
		{
			// Round to full sprite extent, for sprites which are not adhering to regular tile bounding boxes
			vVertexCoords.x = floor(aPos.x + 0.5);
			vVertexCoords.y = floor(aPos.y + 0.5);
		}
		else
		{
			vVertexCoords = aPos.xy;
		}

		vTexCoords = vec2( vVertexCoords.x, 1.0 - vVertexCoords.y );
		block1 = uvec4(floorSprite, jobFloorSprite, wallSprite, jobWallSprite);
		block2 = uvec4(itemSprite, creatureSprite, vFluidLevelPacked1, uIsWall);
		block3 = uvec4(vFlags, vFlags2, vLightLevel, vVegetationLevel);

		vec3 worldPos = project( rotate( tile ), vVertexCoords.xy, uIsWall );
		if ( uCreatureOnly && creatureSprite != 0 )
		{
			vec3 creatureDelta = vec3(
				float( tileData.data[index].creatureOffsetX ),
				float( tileData.data[index].creatureOffsetY ),
				float( tileData.data[index].creatureOffsetZ )
			);
			const float motionAge = uCreatureRenderTick - float( tileData.data[index].creatureMotionTick );
			const float motionDuration = max( 1.0, float( tileData.data[index].creatureMotionDurationTicks ) );
			const float motionProgress = clamp( motionAge / motionDuration, 0.0, 1.0 );
			creatureDelta *= 1.0 - motionProgress;
			creatureDelta = rotateCreatureDelta( creatureDelta );
			worldPos.x += 16.0 * ( creatureDelta.x - creatureDelta.y );
			worldPos.y += -8.0 * ( creatureDelta.x + creatureDelta.y ) + 20.0 * creatureDelta.z;
			worldPos.z += creatureDelta.x + creatureDelta.y + creatureDelta.z;
		}
		gl_Position = uTransform * vec4( worldPos, 1.0 );
	}
}
