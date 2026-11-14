# Surface library — Unreal 5.8

Seventeen seamless surfaces, each with a BaseColor (sRGB), a Normal (tangent space, Unreal's DirectX convention) and a
Roughness map: eleven drawn for the game, five photo-scans (ambientCG, CC0 - see `Source/SOURCES.md`) and ChatGPT's
linoleum. The game uses them through `Source/HomicideUnit/HUSurfaces.h`: pass a surface to `HUBuild::Block` and the
block gets that material, tinted so it still averages to the block's colour.

| Surface | Source maps | Tile (cm) | Used for |
|---|---|---|---|
| Plaster | `M_Plaster_*` (scan: Plaster001) | 200 | interior walls, ceilings of plain rooms |
| Linoleum | `M_Linoleum_*` (ChatGPT's) | 100 | roll call, the corridor, most crime-scene floors |
| Walnut | `M_Walnut_*` (scan: Wood051) | 120 | trim, wainscot, desks, doors, lecterns |
| StoneTile | `T_StoneTile_*` (scan: Tiles139) | 180 | the precinct lobby's worn floor |
| Plastic | `T_Plastic_*` (scan: Plastic004) | 100 | roll call's time clock and laptop, light-switch plates |
| Paper | `T_Paper_*` (scan: Paper001) | 60 | notices, folders, papers on desks and in drawers |
| Brick | `T_Brick_*` | 110 | warehouse / pier / stables walls, hotel exterior, the buildings across the street |
| Concrete | `T_Concrete_*` | 300 | sidewalks, the motor pool, basements, warehouse floors |
| Asphalt | `T_Asphalt_*` | 300 | the street |
| Tile | `T_Tile_*` | 120 | hospital, bank and salon floors |
| Carpet | `T_Carpet_*` | 150 | hotel, theatre, ballroom and lounge floors |
| WoodFloor | `T_WoodFloor_*` | 100 | bowling alley and lodge floors, roll call benches, warehouse racks |
| CeilingTile | `T_CeilingTile_*` | 120 | roll call, the briefing room and corridor, offices and diners |
| PaintedMetal | `T_PaintedMetal_*` | 200 | lockers, roll-up doors, tool chests, the hotel's boiler and air-con |
| Glass | `T_Glass_*` | 150 | door panels (see-through: its own parent, `M_HU_SurfaceGlass`) |
| Fabric | `T_Fabric_*` | 60 | the briefing room's carpet and chairs, hotel beds, awnings |
| Leather | `T_Leather_*` | 80 | the Captain's couch (ready for chairs, booths, coats) |

The precinct (`HUStation.cpp`) uses them room by room: linoleum corridors, a worn stone-tile lobby, a tiled locker room, the Captain's
red carpet, carpeted bullpen, concrete interview rooms and holding, old boards in records, plaster walls with walnut
skirting and door frames, an acoustic ceiling, painted-steel lockers, shelving, cells and cabinets, and glass at the
front desk. The personal offices (`HUOffice.cpp`) paint plaster and weave carpet in whatever colours the owner picks.

The `T_*` maps are drawn by `Tools/Surfaces/make_surfaces.py` from seeded noise (1024 × 1024, same output every run),
which also writes `Source/surface_averages.json`, each base colour's average: the reference the game tints against
(copied into `HUSurfaces.cpp`). `Tools/Surfaces/ImportSurfaces.py` (Unreal's Python, see its header) imports them into
`/Game/Art/Materials/Textures` with the right settings per map, builds the two parents `M_HU_Surface` (opaque) and
`M_HU_SurfaceGlass` in `/Game/Art/Materials/Surfaces` — world-aligned, so they don't stretch on blocks of any size, with
`Tint`, `TileSizeCm` and `Metallic` parameters — and one instance per surface, `MI_Surface_<Name>`.

To change a surface: edit its function in `make_surfaces.py` (or drop in new scanned maps), run `ImportSurfaces.py`
with that surface's name, and update its average in `HUSurfaces.cpp` (the mean of the base colour in linear colour).
To add one: its maps here (`.png` or `.jpg`), an entry in `ImportSurfaces.py`'s `SURFACES`, and a line in
`HUSurfaces.h` / `.cpp`. Keep maps square and a power of two, or they get no mips.

ChatGPT's original instances (`/Game/Art/Materials/Precinct/MI_HU_*`, `M_HU_WorldAlignedSurface`) are still there for
anything that references them; the game itself now uses the library. These are source maps, not baked lighting.
