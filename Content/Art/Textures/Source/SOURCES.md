# Where the scanned maps come from

The photo-scanned maps here are from [ambientCG](https://ambientcg.com) and licensed CC0 1.0: commercial use and
redistribution allowed, no attribution required ([license](https://ambientcg.com/license)). The asset IDs are kept for
provenance. ChatGPT picked and downloaded them for its precinct branch (`codex/precinct-assets`); each is the asset's
2K JPG colour, DirectX normal and roughness map.

| Maps | ambientCG asset | Surface (`HUSurfaces.h`) |
|---|---|---|
| `M_Plaster_*` | [Plaster001](https://ambientcg.com/view?id=Plaster001) | Plaster: walls |
| `M_Walnut_*` | [Wood051](https://ambientcg.com/view?id=Wood051) | Walnut: desks, trim, doors |
| `T_StoneTile_*` | [Tiles139](https://ambientcg.com/view?id=Tiles139) | StoneTile: the precinct lobby's worn floor |
| `T_Plastic_*` | [Plastic004](https://ambientcg.com/view?id=Plastic004) | Plastic: the time clock, a laptop, light-switch plates |
| `T_Paper_*` | [Paper001](https://ambientcg.com/view?id=Paper001) | Paper: notices, folders, papers in drawers (resized 2048 x 1201 -> 1024 x 1024 so it gets mips) |

The branch also had [Fabric012](https://ambientcg.com/view?id=Fabric012), a navy wool twill for the old block-figure
victims' clothes. It isn't brought in: the victims are MetaHumans now, and a navy base can't be tinted to other cloth.

Every other map here is either drawn by `Tools/Surfaces/make_surfaces.py` (`T_*`) or ChatGPT's own (`M_Linoleum_*`).
