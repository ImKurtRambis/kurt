# Homicide Unit — guide for AI coding assistants (ChatGPT/Codex, Claude, Copilot…)

A noir detective co-op game in **Unreal Engine 5.8**, **C++ first**. Almost everything is built in code — there are
almost no Blueprint or art assets yet: rooms, props and people are made of engine basic shapes at runtime
(`HUBuild::Block`), so you can change the game by editing `.h/.cpp` files only.

**Project folder:** `C:\Users\luism\Unreal Projects\HomicideUnit`  ·  **Engine:** `C:\Program Files\Epic Games\UE_5.8`
**All game code:** `Source/HomicideUnit/` (one module, everything prefixed `HU`).

## Build, run, test (PowerShell, from the project folder)

| What | Command |
|---|---|
| Build (the editor/game DLL) | `powershell -ExecutionPolicy Bypass -File Tools\Build.ps1` |
| Play in a window | `powershell -ExecutionPolicy Bypass -File Tools\Run.ps1` |
| Smoke-test a level headless (loads it, scans the log for errors) | `powershell -ExecutionPolicy Bypass -File Tools\TestLevel.ps1 -Map L_Scene` |
| Two-player test on one PC (host + client over LAN) | `powershell -ExecutionPolicy Bypass -File Tools\TestSquad.ps1` |
| Create any missing levels | `UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script="Tools\CreateLevels.py"` |
| Package a standalone game (Shipping, ~5 min; `-NoAI` leaves out the 14 GB local AI) | `powershell -ExecutionPolicy Bypass -File Tools\Package.ps1` → `Packaged\Windows\HomicideUnit.exe` |

- **Close the game/editor before building** (a running `UnrealEditor.exe` locks the DLL). `Build.ps1` does this for you.
- A build is done when it prints `Result: Succeeded`. Fix every `error C####` it prints.
- After C++ changes, always build, then `TestLevel.ps1` the levels you touched.

## The game loop and the levels (`Content/Maps`)

1. **L_Menu** — `AHUDeskMenu`: you sit at your desk; objects on it are the menu (case files = start, cell phone = join a squad by 6-digit code, computer = settings, etc.). Widget: `UHUMenuWidget`.
2. **L_Lobby** — `AHUSquadRoom` (roll call): where a shift starts. Rows of benches (E sits) facing the chalkboard with the squad code + roster, the sergeant's lectern, a bank of lockers, the duty roster by the doors (the squad's names are on it) and two notice boards. Laptop (location) and folder (difficulty) for the host, a time clock to ready up (Y). All ready → countdown → `UHUSquadSubsystem::StartShift()` server-travels everyone. `-HURollCallShots` on `L_Lobby` tours the room and saves screenshots to `Saved/Screenshots`.
   **L_Precinct** — `AHUPrecinct`: the squad's base for every case: desks and computers, holding, booking, the Captain's in-tray, and the **case board** (`AHUCaseBoard`): suspect mugshots, the scene, the weapon and every bagged piece of evidence; the squad pins evidence to suspects with red string and marks suspects prime/cleared (state in `UHUCaseSubsystem::BoardPins/BoardMarks`). `-HUBoardTest` works the board by script and saves screenshots.
3. **L_Briefing** — `AHUBriefingRoom` (+ `AHUBriefingLaptop`, `AHUMotorPool`, `AHUCruiser`, `AHUSupplyLocker`): Captain's slides on a projector, a 911 call plays (`Play911`), corridor to the motor pool, cruiser drives the squad to the scene.
4. **L_Scene** — `AHUCrimeScene`: generates the whole crime scene from the case (rooms, lofts, ladders, crawlspace, street outside, patrol, prisoner van, suspects, victim, evidence). Your cruiser drives back to L_Precinct, where `ResolveArrests()` judges the case once it's submitted. Until then the squad can go back out: the **MOTOR POOL door** in the precinct (`AHUMotorPoolDoor`, a cruiser with `bReturnTrip`) drives everyone back to the same scene; evidence already bagged destroys itself in `AHUEvidence::BeginPlay` and suspects in holding hide in `AHUSuspectActor::BeginPlay`. `-HUReturnTest` on `L_Precinct` scripts that round trip.

## Where things live

| Area | Files |
|---|---|
| Case data structs | `HUCaseTypes.h` |
| Case generator (incident × hidden offender profile × place) | `HUCaseGen.*`, profiles in `HUProfiles.h` |
| The case in play, money, kit, save, rules, verdict | `HUCaseSubsystem.*`, `HUSave.h` |
| Squads / online (Steam or LAN sessions, 6-digit codes) | `HUSquadSubsystem.*`, `HUPlayerState.*`, `HUGameModes.*` |
| The player | `HUCharacter.*` (movement, sitting), `HUInteractComponent.*` (look-at, E/Q, hotbar 1–7, flashlight/UV/scanner, hand viewmodel, hover glow, notifications, **squad interaction replay**) |
| HUD & menus (drawn in C++, no UMG assets) | `HUPromptWidget.*`, `HUMenuWidget.*`, shared kit `HUDraw.h` |
| Interactable things | `HUInteractable.h` (interface), `HUEvidence`, `HUPushable`, `HUSuspectActor`, `HUVictim`, `HUPoliceVan`, `HUOfficer`, `HULadder`, `HUCruiser`, `HUSupplyLocker`, `HUCaseBoard` |
| Tools & concealment | `HUTools.h` |
| Interrogation (pressure, moods, confession) | `HUInterrogation.h`, `UHUCaseSubsystem::Confront/Press/RaiseRecord`, interview UI in `AHUSuspectActor` + `HUPromptWidget` |
| Voices (ElevenLabs v3 / OpenAI TTS, phone filter) | `HUVoiceSubsystem.*` — key from `ElevenLabsKey.txt` (git-ignored; **never commit or print keys**) |
| Placeholder geometry | `HUBuild.*` (`Block`, `WallWithDoor`, `Clear`) |
| Real furniture (desks, chairs, CRTs, phones, lockers, filing cabinets, benches, water cooler, clocks, cork boards) | `HUProps.h` (`Prop(..., Name, Center, Size, Rot)` - same box as the blocks, returns null without the art so you build the blocks), meshes in `/Game/Art/Props` (`Content/Art/Props/README.md`, `Tools/PropKit/`, `Tools/ImportProps.py`) |
| Screenshot tour of any level | `-HUShots=X,Y,Yaw,Pitch\|...` `-HUShotsName=Prefix` (`HUShotTour.*`); add `-HUNoProps` for the same views with blocks |

## Rules that keep the game working
| What things are made of (brick, carpet, tile... 14 textured surfaces, pass one to `Block`) | `HUSurfaces.*`, `Content/Art/Textures/README.md`, `Tools/Surfaces/` |

1. **Interactables** implement `IHUInteractable` (`GetPrompt`, `Interact`; optional `HoverParts`, `Highlights`). Show text with `UHUInteractComponent::Say()` / `Notify()`.
2. **Multiplayer = replay.** Every machine builds the *same* scene from the squad code. `UHUInteractComponent::Use()` sends an interaction to the host, which multicasts it; **every machine runs `Interact` on its own copy**. So `Interact` must be **deterministic** and must not assume the user is the local player:
   - Personal things (shop, own ready state, clipboard) → guard with `User->IsLocallyControlled()`.
   - Host-only things (travel, case choice) → also check `GetNetMode() != NM_Client`.
3. **Spawn scene actors with fixed names** (`AHUCrimeScene::SpawnNamed<T>`), never auto-named — squad replay finds actors by path name.
4. **Randomness:** layout uses the case's `MapSeed`; evidence placement uses `SquadSeed` in a squad (random solo). Don't use `FMath::Rand` for anything that must match across players.
5. **No assets required:** if you add a mesh/material slot, keep the block fallback so the game runs without art.
   **Assets loaded by path** (`LoadObject`, `LoadClass`, soft paths) aren't found by the cooker: add their folder to `+DirectoriesToAlwaysCook` in `Config/DefaultGame.ini`, or the packaged game silently runs without them. Shipping ignores a map on the command line, so test packaged-only problems with `Package.ps1 -Config Development`.
6. **No hover glow/spotlight in the game** (only the dark desk menu has that). In-game feedback is the crosshair ticks + the prompt.
7. **Working alongside another AI:** pull before you start (git pull), commit small focused changes, push when a change builds. Don't reformat files you aren't changing.
   **Helper programs** (AI servers, tools in `AI\`) start with `HUProcess::Launch`, never `FPlatformProcess::CreateProc`: it ties them to the game so they die with it, even on a crash. Players have no Visual Studio, Python or CUDA toolkit: anything in `AI\` must run with only what `Package.ps1` ships.
8. Keep the style of the existing code: short functions, comments that say *why*, `HU` prefix, UPROPERTYs for anything Blueprint-tweakable.

## Not done yet (good next tasks)

- Interrogation AI: a local LLM (llama.cpp) that acts out the suspect from their facts + pressure state (`HUInterrogation.h`, `UHUCaseSubsystem::Confront/Press/RaiseRecord`); offline voices (Kokoro / Chatterbox) whose emotion follows pressure. See the zero-cost plan: no per-use API calls in the final build.
- Real characters (MetaHuman + Motion Matching), prop library for Fab/Megascans meshes, comic/noir post-process.
- Offline voices (Kokoro via NNE) to replace paid TTS.
