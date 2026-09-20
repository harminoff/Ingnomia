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

#define CAT(x, y) CAT_(x, y)
#define CAT_(x, y) x ## y
#define UNPACKSPRITE(alias, src) const uint CAT(alias, ID) = src & 0xffff; const uint CAT(alias, Flags) = src >> 16;
layout(location = 0) noperspective in vec2 vTexCoords;
layout(location = 1) flat in uvec4  block1;
layout(location = 2) flat in uvec4  block2;
layout(location = 3) flat in uvec4  block3;

layout(location = 0) out vec4 fColor;

uniform sampler2DArray uTexture[32];

uniform int uTickNumber;

uniform int uUndiscoveredTex;

uniform int uWorldRotation;
uniform bool uOverlay;
uniform bool uDebug;
uniform bool uWallsLowered;
#include "lighting.glsl"
uniform bool uPaintFrontToBack;

uniform bool uShowJobs;

const vec3 perceivedBrightness = vec3(0.299, 0.587, 0.114);

vec4 getTexel( uint spriteID, uint rot, uint animFrame )
{
	uint absoluteId = ( spriteID + animFrame ) * 4;
	uint tex = absoluteId / 2048;
	uint localBaseId = absoluteId % 2048;
	uint localID = localBaseId + rot;
	
	ivec3 samplePos = ivec3( vTexCoords.x * 32, vTexCoords.y * 64, localID);

	// Need to unroll each access to texelFetch with a different element from uTexture into a distinct instruction
	// Otherwise we are triggering a bug on AMD GPUs, where threads start sampling from the wrong texture
	#define B(X) case X: return texelFetch( uTexture[X], samplePos, 0);
	#define C(X) B(X) B(X+1) B(X+2) B(X+3)
	#define D(X) C(X) C(X+4) C(X+8) C(X+12)
	switch(tex)
	{
		D(0)
		D(16)
	}
	#undef D
	#undef C
	#undef B
}

void main()
{
	vec4 texel = vec4( 0,  0,  0, 0 );
	
	uint rot = 0;
	uint spriteID = 0;
	uint animFrame = 0;
	
	UNPACKSPRITE(floorSprite, block1.x);
	UNPACKSPRITE(jobFloorSprite, block1.y);
	UNPACKSPRITE(wallSprite, block1.z);
	UNPACKSPRITE(jobWallSprite, block1.w);
	UNPACKSPRITE(itemSprite, block2.x);
	UNPACKSPRITE(creatureSprite, block2.y);

	const uint vFluidLevelPacked1 = block2.z;
	const bool uIsWall = ( block2.w != 0 );

	const uint vFlags = block3.x;
	const uint vFlags2 = block3.y;

	const uint vLightLevel = block3.z;
	const uint vVegetationLevel = block3.w;

	uint vFluidLevel = (vFluidLevelPacked1 >> 0) & 0xff;
	uint vFluidLevelLeft = (vFluidLevelPacked1 >> 8) & 0xff;
	uint vFluidLevelRight = (vFluidLevelPacked1 >> 16) & 0xff;
	uint vFluidFlags = (vFluidLevelPacked1 >> 24) & 0xff;

	
	if( !uIsWall )
	{
		if( ( vFlags & TF_UNDISCOVERED ) != 0 && !uDebug )
		{
			if( !uWallsLowered )
			{
				vec4 tmpTexel = getTexel( uUndiscoveredTex / 4 + 2, 0, 0 );

				texel.rgb = mix( texel.rgb, tmpTexel.rgb, tmpTexel.a );
				texel.a = max(texel.a , tmpTexel.a);
			}
		}
		else
		{
			spriteID = floorSpriteID;
			if( spriteID != 0 )
			{
				rot = floorSpriteFlags & 3;
				rot = ( rot + uWorldRotation ) % 4;
				
				if( ( floorSpriteFlags & 4 ) == 4 )
				{
					animFrame = ( uTickNumber / 10 ) % 4;
				}
				
				vec4 tmpTexel = getTexel( spriteID, rot, animFrame );
				
				if( ( vFlags & TF_GRASS ) != 0 )
				{
					vec4 roughFloor = getTexel( uUndiscoveredTex / 4 + 3, 0, 0 );
					float interpol = 1.0 - ( float( vVegetationLevel ) / 100. );
					tmpTexel.rgb = mix( tmpTexel.rgb, roughFloor.rgb, interpol * roughFloor.a );
					texel.a = max(tmpTexel.a , roughFloor.a);
				}

				texel.rgb = mix( texel.rgb, tmpTexel.rgb, tmpTexel.a );
				texel.a = max(texel.a , tmpTexel.a);
			}
			
			spriteID = jobFloorSpriteID;
			animFrame = 0;
			if( uShowJobs && ( spriteID != 0 )  )
			{
				rot = jobFloorSpriteFlags & 3;
				rot = ( rot + uWorldRotation ) % 4;
				
				vec4 tmpTexel = getTexel( spriteID, rot, animFrame );
				
				if( ( vFlags & TF_JOB_BUSY_FLOOR ) != 0 )
				{
					tmpTexel.r *= 0.3;
					tmpTexel.g *= 0.7;
					tmpTexel.b *= 0.3;
				}
				else
				{
					tmpTexel.r *= 0.7;
					tmpTexel.g *= 0.7;
					tmpTexel.b *= 0.3;
				}

				texel.rgba = tmpTexel.rgba;
			}

			if( uOverlay && 0 != ( vFlags & ( TF_STOCKPILE | TF_FARM | TF_GROVE | TF_PASTURE | TF_WORKSHOP | TF_ROOM | TF_NOPASS ) ) )
			{
				vec3 roomColor = vec3( 0.0 );
			
				if( ( vFlags & TF_STOCKPILE ) != 0 ) //stockpile
				{
					roomColor = vec3(1, 1, 0);
				}
				
				else if( ( vFlags & TF_FARM ) != 0 ) //farm
				{
					roomColor = vec3(0.5, 0, 1);
				}
				else if( ( vFlags & TF_GROVE ) != 0 ) //grove
				{
					roomColor = vec3(0, 1, 0.5);
				}
				else if( ( vFlags & TF_PASTURE ) != 0 ) 
				{
					roomColor = vec3(0, 0.9, 0.9);
				}
				else if( ( vFlags & TF_WORKSHOP ) != 0 ) //workshop
				{
					if( ( vFlags & TF_BLOCKED ) != 0 )
					{
						roomColor = vec3(1, 0, 0);
					}
					else
					{
						roomColor = vec3(1, 1, 0);
					}
				}
				else if( ( vFlags & TF_ROOM ) != 0 ) //room
				{
					roomColor = vec3(0, 0, 1);
				}
				else if( ( vFlags & TF_NOPASS ) != 0 ) //room
				{
					roomColor = vec3(1, 0, 0);
				}
				if( texel.a != 0 )
				{
					float brightness = dot(texel.rgb, perceivedBrightness.xyz);
					// Preserve perceived brightness of original pixel during tinting, but drop saturation partially
					texel.rgb = mix( roomColor, mix( texel.rgb, vec3(1,1,1) * brightness, 0.7), 0.7);
				}
				
			}
		}
	}
	else
	{
		if( ( vFlags & TF_UNDISCOVERED ) != 0 && !uDebug )
		{
			if( !uWallsLowered )
			{
				vec4 tmpTexel = getTexel( uUndiscoveredTex / 4, 0, 0 );

				texel.rgb = mix( texel.rgb, tmpTexel.rgb, tmpTexel.a );
				texel.a = max(texel.a , tmpTexel.a);
			}
		}
		else
		{
			spriteID = wallSpriteID;
			if( spriteID != 0 )
			{
				rot = wallSpriteFlags & 3;
				rot = ( rot + uWorldRotation ) % 4;

				if( ( wallSpriteFlags & 4 ) == 4 )
				{
					animFrame = ( uTickNumber / 3 ) % 4;
				}
				if( uWallsLowered )
				{
					animFrame = 0;
					if( ( wallSpriteFlags & 8 ) == 8 )
					{
						spriteID = spriteID + 1;
					}
				}
				vec4 tmpTexel = getTexel( spriteID, rot, animFrame );
				
				texel.rgb = mix( texel.rgb, tmpTexel.rgb, tmpTexel.a );
				texel.a = max(texel.a , tmpTexel.a);
			}
			
			spriteID = itemSpriteID;
			animFrame = 0;
			if( spriteID != 0 )
			{
				rot = itemSpriteID & 3;
				rot = ( rot + uWorldRotation ) % 4;
				
				vec4 tmpTexel = getTexel( spriteID, rot, animFrame );
				
				texel.rgb = mix( texel.rgb, tmpTexel.rgb, tmpTexel.a );
				texel.a = max(texel.a , tmpTexel.a);
			}
		}
	
		spriteID = jobWallSpriteID;
		animFrame = 0;
		if( uShowJobs && spriteID != 0 )
		{
			rot = jobWallSpriteFlags & 3;
			rot = ( rot + uWorldRotation ) % 4;
			
			vec4 tmpTexel = getTexel( spriteID, rot, animFrame );
			
			if( ( vFlags & TF_JOB_BUSY_WALL ) != 0 )
			{
				tmpTexel.r *= 0.3;
				tmpTexel.g *= 0.7;
				tmpTexel.b *= 0.3;
			}
			else
			{
				tmpTexel.r *= 0.7;
				tmpTexel.g *= 0.7;
				tmpTexel.b *= 0.3;
			}

			texel.rgba = tmpTexel.rgba;
		}

	
		spriteID = creatureSpriteID;
		animFrame = 0;
		if( spriteID != 0 )
		{
			rot = creatureSpriteFlags & 3;
			rot = ( rot + uWorldRotation ) % 4;
			
			vec4 tmpTexel = getTexel( spriteID, rot, animFrame );

			texel.rgb = mix( texel.rgb, tmpTexel.rgb, tmpTexel.a );
			texel.a = max(texel.a , tmpTexel.a);
		}

	}

	if( texel.a <= 0 )
	{
		discard;
	}
	else if(uPaintFrontToBack)
	{
		// Flush to 1 in case of front-to-back rendering
		texel.a = 1;
	}
	
	
	if( !uDebug )
	{
        float sky = (vFlags & (TF_SUNLIGHT | TF_INDIRECT_SUNLIGHT)) != 0u ? 1.0 : 0.0;
        texel.rgb = shadeWorldColor(texel.rgb, sky, float(vLightLevel) / 20.0,
                                    (vFlags & TF_UNDISCOVERED) == 0u);
	}
	fColor = texel;
}
