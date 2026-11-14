// The game's surface library: 14 tileable materials with colour, normal and roughness maps. The textures are made by
// Tools/Surfaces/make_surfaces.py (plaster, linoleum and walnut came from ChatGPT's precinct set) and imported by
// Tools/Surfaces/ImportSurfaces.py into /Game/Art/Materials/Surfaces/MI_Surface_<Name>.
//
// Pass one to HUBuild::Block and the block gets that surface, tinted so it still averages to the block's colour: a room
// keeps its palette and gains brick, grain, weave. Strongly coloured surfaces (brick, carpet, wood floor, leather,
// asphalt) keep their own hue and only take the colour's brightness. HUSurfaces::Natural(S) is the texture's own
// colour - pass it to show a surface as it was made. Without the assets, blocks fall back to their flat colour.
// The materials are world-aligned, so they don't stretch on blocks of any size (but they swim on blocks that move).
#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UPrimitiveComponent;

enum class EHUSurface : uint8
{
	None, Plaster, Linoleum, Walnut, Brick, Concrete, Asphalt, Tile, Carpet, WoodFloor, CeilingTile, PaintedMetal, Glass, Fabric, Leather, Count
};

namespace HUSurfaces
{
	HOMICIDEUNIT_API const TCHAR* Name(EHUSurface S);
	// the material instance (loaded once), or null if it isn't in the project
	HOMICIDEUNIT_API UMaterialInterface* Material(EHUSurface S);
	// the texture's own average colour (linear)
	HOMICIDEUNIT_API FLinearColor Natural(EHUSurface S);
	// give C this surface, tinted to average Want; false (and nothing changed) if the surface isn't available
	HOMICIDEUNIT_API bool Apply(UPrimitiveComponent* C, EHUSurface S, const FLinearColor& Want);
}
