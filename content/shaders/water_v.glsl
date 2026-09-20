#version 430 core
#define TF_UNDISCOVERED 0x00000002u
#define WATER_BLOCKING 0x80000000u
#define WATER_FLOOR 0x40000000u
layout(location = 0) in vec3 aPos;
layout(location = 0) noperspective out vec2 vTexCoords;
layout(location = 1) noperspective out float vFluidLevel;
layout(location = 2) noperspective out vec2 vWaterFlow;
layout(location = 3) flat out uvec4 vNeighborLevels;
layout(location = 4) flat out uvec3 vTile;
layout(location = 5) out vec2 vScreenUv;
layout(location = 6) flat out vec3 vBaseNormal;
layout(location = 7) flat out uint vIsSide;
layout(location = 8) noperspective out vec2 vWaterUv;
layout(location = 9) noperspective out float vWaterLight;
layout(location = 10) flat out float vWaterSky;
uniform uvec3 uWorldSize;
uniform mat4 uTransform;
uniform int uWorldRotation;
uniform uvec3 uRenderMin;
uniform uvec3 uRenderMax;
uniform int uWaterFace;
// Must match the 56-byte TileData/compute upload layout.
struct TileData {
    uint flags; uint flags2;
    uint floorSpriteUID; uint wallSpriteUID;
    uint itemSpriteUID; uint creatureSpriteUID;
    uint jobSpriteFloorUID; uint jobSpriteWallUID;
    uint packedLevels;
    int creatureOffsetX; int creatureOffsetY; int creatureOffsetZ;
    uint creatureMotionTick; uint creatureMotionDurationTicks;
};
layout(std430, binding = 0) readonly restrict buffer tileData1 { TileData data[]; } tileData;
uint tileID(ivec3 p) { return uint(p.x + p.y * int(uWorldSize.x) + p.z * int(uWorldSize.x * uWorldSize.y)); }
bool inWorld(ivec3 p) { return all(greaterThanEqual(p, ivec3(0))) && all(lessThan(p, ivec3(uWorldSize))); }
bool blocksSurface(uint index) {
    // Loose items, plant sprites and construction previews are not walls.
    return (tileData.data[index].flags2 & WATER_BLOCKING) != 0u;
}
uint waterLevel(ivec3 p) {
    if (!inWorld(p)) return 0u;
    uint index = tileID(p);
    if (blocksSurface(index) || (tileData.data[index].flags & TF_UNDISCOVERED) != 0u) return 0u;
    return min(tileData.data[index].packedLevels & 0xffu, 10u);
}
vec2 flowAt(ivec3 p) {
    if (waterLevel(p) == 0u) return vec2(0.0);
    uint flow = tileData.data[tileID(p)].packedLevels >> 24;
    return vec2(float((flow & 2u) != 0u) - float((flow & 8u) != 0u),
                float((flow & 4u) != 0u) - float((flow & 1u) != 0u));
}
vec2 rotatePoint(vec2 p) {
    switch (uWorldRotation % 4) {
        case 1: return vec2(float(uWorldSize.y) - p.y - 1.0, p.x);
        case 2: return vec2(uWorldSize.xy) - p - 1.0;
        case 3: return vec2(p.y, float(uWorldSize.x) - p.x - 1.0);
    }
    return p;
}
vec2 unrotateDirection(vec2 p) {
    switch (uWorldRotation % 4) {
        case 1: return vec2(p.y, -p.x);
        case 2: return -p;
        case 3: return vec2(-p.y, p.x);
    }
    return p;
}
// Every face meeting this world corner uses these same samples. Dry/solid
// neighbors do not pull a level shoreline down to a quarter of its height.
float cornerLevel(ivec3 tile, vec2 corner, out vec2 flow, out float light) {
    ivec2 base = tile.xy + ivec2(round(corner)) - ivec2(1);
    float count = 0.0, level = 0.0;
    flow = vec2(0.0);
    light = 0.0;
    for (int y = 0; y <= 1; ++y) for (int x = 0; x <= 1; ++x) {
        ivec3 p = ivec3(base + ivec2(x,y), tile.z);
        uint wet = waterLevel(p);
        if (wet > 0u) {
            level += float(wet); flow += flowAt(p); count += 1.0;
            light += float((tileData.data[tileID(p)].packedLevels >> 8) & 0xffu) / 20.0;
        }
    }
    flow /= max(count, 1.0);
    light /= max(count, 1.0);
    return level / max(count, 1.0);
}
void main() {
    uint id = uint(gl_InstanceID);
    uvec3 volume = uRenderMax - uRenderMin + uvec3(1);
    uvec3 tile = uRenderMin + uvec3(id % volume.x, (id / volume.x) % volume.y, id / (volume.x * volume.y));
    ivec3 p = ivec3(tile);
    uint fluid = waterLevel(p);
    bool boundary = any(equal(tile, uvec3(0))) || any(greaterThanEqual(tile + uvec3(1), uWorldSize));
    bool hasWater = fluid > 0u && !boundary;
    ivec3 above = p + ivec3(0,0,1);
    bool covered = tile.z < uRenderMax.z && (waterLevel(above) > 0u ||
                   (tileData.data[tileID(above)].flags2 & WATER_FLOOR) != 0u || blocksSurface(tileID(above)));
    vTile = tile;
    vWaterSky = (tileData.data[tileID(p)].flags & 0x01000004u) != 0u ? 1.0 : 0.0;
    vNeighborLevels = uvec4(waterLevel(p + ivec3(0,-1,0)), waterLevel(p + ivec3(1,0,0)),
                           waterLevel(p + ivec3(0,1,0)), waterLevel(p + ivec3(-1,0,0)));
    vBaseNormal = vec3(0,0,1);
    vIsSide = uint(uWaterFace != 0);
    vec2 corner;
    float level;
    bool visible = hasWater && !covered;
    if (uWaterFace == 0) {
        // The shared VAO holds sprite rectangles. Convert the four corners
        // into an actual isometric diamond instead of overlapping rectangles.
        corner = vec2(aPos.x, clamp((aPos.y - 0.2) / 0.3, 0.0, 1.0));
        level = cornerLevel(p, corner, vWaterFlow, vWaterLight);
        vTexCoords = corner;
    } else {
        // Two camera-facing edges, sharing top endpoints with the surface.
        vec2 cameraEdge = uWaterFace == 1 ? vec2(1,0) : vec2(0,1);
        ivec2 edge = ivec2(unrotateDirection(cameraEdge));
        ivec3 neighbor = p + ivec3(edge,0);
        uint neighborFluid = waterLevel(neighbor);
        bool solidNeighbor = inWorld(neighbor) && blocksSurface(tileID(neighbor));
        visible = hasWater && neighborFluid == 0u && !solidNeighbor;
        corner = edge.x != 0 ? vec2(edge.x > 0 ? 1.0 : 0.0, aPos.x)
                             : vec2(aPos.x, edge.y > 0 ? 1.0 : 0.0);
        float top = cornerLevel(p, corner, vWaterFlow, vWaterLight);
        float vertical = clamp((aPos.y - 0.2) / 0.6, 0.0, 1.0);
        level = mix(0.0, top, vertical);
        vTexCoords = vec2(aPos.x, vertical);
        vBaseNormal = normalize(vec3(vec2(edge), 0.35));
    }
    vFluidLevel = level;
    vec2 worldXY = vec2(tile.xy) + corner - 0.5;
    vWaterUv = worldXY * 0.045;
    vec2 rotated = rotatePoint(worldXY);
    // Terrain sprites use a constant painter depth per tile (floor +0,
    // wall/creature +0.5). A depth plane varying across the water diamond
    // intersects those billboards and cuts triangular holes into the shore.
    // Keep geometry/UVs continuous, but sort the whole face in the same tile
    // layer convention, between its bed and the wall/creature layer.
    vec2 depthTile = rotatePoint(vec2(tile.xy));
    vec3 worldPos = vec3(16.0 + 16.0 * (rotated.x - rotated.y),
        12.0 - 8.0 * (rotated.x + rotated.y) - (float(uRenderMax.z) - float(tile.z)) * 20.0 + level * 0.3,
        float(tile.z) + depthTile.x + depthTile.y + 0.25);
    gl_Position = uTransform * vec4(worldPos, 1.0);
    vScreenUv = gl_Position.xy / gl_Position.w * 0.5 + 0.5;
    if (!visible) gl_Position = vec4(2.0,2.0,2.0,1.0);
}
