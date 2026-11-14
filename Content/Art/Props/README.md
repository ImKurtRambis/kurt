# Furniture (`/Game/Art/Props/SM_<Name>`)

Real furniture in place of the block placeholders. The game asks for a prop with `HUBuild::Prop` (`Source/HomicideUnit/HUProps.h`);
when the mesh isn't in a build it builds the old blocks instead, so the game still runs without this folder.

| Prop | Source | License |
|---|---|---|
| Desk | Poly Haven `metal_office_desk` | CC0 |
| StackChair | Poly Haven `SchoolChair_01` | CC0 |
| WallClock | Poly Haven `wall_clock` | CC0 |
| CRTTV | Poly Haven `television_02` | CC0 |
| DeskLamp | Poly Haven `desk_lamp_arm_01` | CC0 |
| Locker, LockerOpen, FilingCabinet, WaterCooler, CorkBoard, CorkBoardNotices, CorkBoardWide, DeskPhone, CRTMonitor, Keyboard, OfficeChair, Bench, BenchLong, RollCallBench | made for the game: `Tools/PropKit/make_props.py` (cork and wood-grain textures generated there too) | the project's own |

Poly Haven models (https://polyhaven.com, CC0 - no credit required) were downloaded on 2026-11-14 as glTF with 1K textures
and are kept as received in `Source/PolyHaven/`.

To change or add furniture:
1. edit `Tools/PropKit/make_props.py` (models are built in Unreal's units: cm, +X is the front, pivot on the floor - or on the wall for wall props)
2. `AI\python\python.exe -I Tools\PropKit\make_props.py` writes `Source/<Name>/<Name>.gltf`
3. `UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script="Tools\ImportProps.py"` imports them all
4. use it: `Prop(this, R, Blocks, TEXT("Name"), Center, Size, Rot)` with the same box the blocks used, keeping the blocks as the fallback
