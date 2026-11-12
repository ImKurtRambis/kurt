# Precinct surface set — Unreal 5.8

Three original, seamless surface sets for the 2000-era precinct look:

- `M_Plaster_*`: painted gray-green plaster for interior walls.
- `M_Linoleum_*`: worn olive / warm-gray checkered vinyl for floors.
- `M_Walnut_*`: dark walnut veneer for desks and trim.

Each surface has BaseColor, Normal (tangent-space, Unreal DirectX convention), and Roughness PNG maps. The import script places them in `/Game/Art/Materials/Textures`, with sRGB enabled only for BaseColor, normal-map compression for Normal, and mask compression for Roughness.

The material parent `M_HU_WorldAlignedSurface` and instances `MI_HU_Plaster`, `MI_HU_Linoleum`, and `MI_HU_Walnut` are in `/Game/Art/Materials/Precinct`. The parent uses Unreal's world-aligned texture functions and exposes `TileSizeCm`; instances start at 200 cm for plaster/walnut and 100 cm for linoleum. This avoids UV stretching on runtime-scaled block geometry.

`AHUCrimeScene` and the precinct/roll-call room load the three instances for floors, painted walls, and wood desks or trim. Map-level overrides still work. The precinct room also adds restrained cool grading and a light vignette; it does not add film grain or static. To reimport source maps and restore their texture settings, run `Tools/ImportPrecinctTextureMaps.py` through Unreal's Python commandlet; to recreate the material assets, run `Tools/CreatePrecinctMaterials.py`.

The palette is tuned for a working mid-2000s police department: commercial VCT flooring, painted institutional walls, older wood desks, cool fluorescent overhead light, and a slightly heightened noir contrast. It keeps the period contemporary instead of borrowing 1940s set dressing.

These are source maps, not baked lighting. Keep scene lighting separate so the textures respond to Unreal lighting.
