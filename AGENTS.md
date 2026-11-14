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
| Score the suspects' AI (~6 min; see `Tools\InterviewBench\README.md`) | `powershell -ExecutionPolicy Bypass -File Tools\InterviewBench.ps1` → `Saved\InterviewBench\<date>\report.md` |
| Build the cast of MetaHumans (full editor, signed in to Epic; minutes per person; skips anyone built) | `UnrealEditor.exe HomicideUnit.uproject -ExecutePythonScript="Tools/MakeCast.py" -unattended` → `Content/MetaHumans` (not in git) |
| Photograph everyone (MetaHumans) in a level | add `-HUPeopleShots` (and `-HUCrime=homicide` on `L_Scene` for a body) → `Saved\Screenshots` |
| Photograph every way a body moves (idle, walk, talk, flinch, cuffed, crouch, seated at a table...) | `L_Precinct` with `-HUBodyTest` → `Saved\Screenshots\...\Body_*.png` |

- **Close the game/editor before building** (a running `UnrealEditor.exe` locks the DLL). `Build.ps1` does this for you.
- A build is done when it prints `Result: Succeeded`. Fix every `error C####` it prints.
- After C++ changes, always build, then `TestLevel.ps1` the levels you touched.
- **Version:** every build is stamped from git before it compiles (`Tools\StampVersion.ps1`, a pre-build step in `Source\*.Target.cs`, writes the git-ignored `HUBuildInfo.h`): "build N (hash)", N = commits on the branch, "+local" if uncommitted changes went in. It's in the menu's corner, the log's first lines, crash reports (game data `HUBuild`) and `Packaged\Windows\Version.txt` (`HUVersion`).
- After any change to the suspects' model, prompt (`UHUBrainSubsystem`) or interrogation rules, run `Tools\InterviewBench.ps1` and compare with the baseline; don't ship a drop in a score you can't explain.

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
| Usable props (drawers, filing cabinets, lockers, phones, seats, light switches, TV, lamps, things to read) | `HUUsables.*`: `MarkProp` marks real furniture where `HUBuild::Prop` places it; `Mark(Block(...), EHUUse::...)` marks a block; `AddSwitch` puts a room's light switch on a wall. The room's own `GetPrompt` wins over a mark. Room-by-room list and what's still to do: `Docs/UsableProps.md`. `-HUUseTest` on `L_Precinct`/`L_Lobby` uses one of each and screenshots it |
| Tools & concealment | `HUTools.h` |
| Interrogation (pressure, moods, confession) | `HUInterrogation.h`, `UHUCaseSubsystem::Confront/Press/RaiseRecord`, interview UI in `AHUSuspectActor` + `HUPromptWidget` |
| Voices (ElevenLabs v3 / OpenAI TTS, phone filter) | `HUVoiceSubsystem.*` — key from `ElevenLabsKey.txt` (git-ignored; **never commit or print keys**) |
| The local AI: suspects' brain, free offline voices, hearing the detective | `HUBrainSubsystem.*` (llama.cpp), `HUVoiceSubsystem.*` + `Tools\VoiceServer.py`, `HUListenSubsystem.*` (sherpa-onnx), all started through `HUProcess`. Files in `AI\` (`AI\README.txt`); what runs on which PC: **The local AI** below |
| Placeholder geometry | `HUBuild.*` (`Block`, `WallWithDoor`, `Clear`) |
| Real furniture (desks, chairs, CRTs, phones, lockers, filing cabinets, benches, water cooler, clocks, cork boards) | `HUProps.h` (`Prop(..., Name, Center, Size, Rot)` - same box as the blocks, returns null without the art so you build the blocks), meshes in `/Game/Art/Props` (`Content/Art/Props/README.md`, `Tools/PropKit/`, `Tools/ImportProps.py`) |
| Screenshot tour of any level | `-HUShots=X,Y,Yaw,Pitch\|...` `-HUShotsName=Prefix` (`HUShotTour.*`); add `-HUNoProps` for the same views with blocks |

## Rules that keep the game working
| What things are made of (brick, carpet, tile... 14 textured surfaces, pass one to `Block`) | `HUSurfaces.*`, `Content/Art/Textures/README.md`, `Tools/Surfaces/` |
| The renderer and the camera | `Config/DefaultEngine.ini` (Lumen light and reflections, Nanite, virtual shadow maps, TSR; nothing is baked). `HUFilmCamera.*`: one level-wide post-process volume on every machine - eye adaptation kept near the lights' fixed exposure, focus on the centre of the view (a view can call `FocusOn(cm)`, as the desk menu does), grain, vignette. Console: `HU.Film.Fstop`, `HU.Film.Grain`, `HU.Film.Off 1`. The colour grade is EA-08's, not here |

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

## The local AI: what runs on which PC

Everything in `AI\` runs on the player's own PC, on any maker's graphics card. The game picks where each part runs by itself; nothing needs an NVIDIA card.

| Part | Where it runs | How it's picked | Force it |
|---|---|---|---|
| Suspects' brain: `UHUBrainSubsystem`, llama.cpp's **Vulkan** build in `AI\llama` | The graphics card (NVIDIA, AMD or Intel). The processor when no card can run it. | `WarmUp` asks `llama-server --list-devices`, starts on the card, and uses it only once a test answer comes back at 6 tokens a second or better. A server that dies, stays silent for 120 s or writes slower than that hands over to the processor (`--device none`). | `-HUBrain=card`, `-HUBrain=cpu` |
| Voices: `Tools\VoiceServer.py` | An NVIDIA card with 12 GB or more: Orpheus + CSM on the card (PyTorch CUDA). Any other PC: Supertonic on the processor (`AI\voice\supertonic`, run by `AI\voice\bin\sherpa-onnx-offline-tts.exe`). | `pick_voices()` asks the NVIDIA driver (`nvcuda.dll`) how much video memory there is. Supertonic also stands in when a big voice won't load or fails a line. | `-HUVoices=cuda`, `-HUVoices=cpu` (`HU_VOICES` when you run the server by hand) |
| Hearing the detective: `UHUListenSubsystem`, sherpa-onnx | The processor, always. | | |

- **Test a path on any PC by forcing it**, without a window. Brain: `UnrealEditor.exe HomicideUnit.uproject /Game/Maps/L_Menu -game -nullrhi -nosound -unattended -HUNoVoice -HUInterviewBench -HUBenchCases=1 -HUBrain=cpu -abslog=<log>`. The log says where the model runs (`HomicideUnit brain: ready on the processor after 2.7 s`) and what every answer took (`answered in 5.2 s (35 tokens at 17 a second, on the processor)`). `-HUBrainBreakCard` makes the card fail and `-HUBrainMinSpeed=9999` calls it too slow, to test both hand-overs. Voices: `... -game -nullrhi -unattended -HUNoBrain -HUVoiceTest -HUVoices=cpu` speaks a 911 call and four suspect lines twice and logs each (`HUVoiceTest: pass 2, suspect line, voice 1: 1.8 s of speech in 1.22 s`).
- **Measured on one PC only** (RTX 5070 Ti, Ryzen 7 7800X3D); the numbers are in `SYSTEM_REQUIREMENTS.md`. The AMD and Intel paths are the same code, forced on that NVIDIA card. No AMD or Intel card has run the game yet.
- **Keep it that way.** A new AI part must run without CUDA (Vulkan, or ONNX on the processor) or have a fallback that does, and `Package.ps1` must ship what the fallback needs. In `VoiceServer.py` every line is spoken on its one voice thread (`on_voice_thread`): compiled CSM breaks when a second thread runs it.

## Not done yet (good next tasks)

- Interrogation AI: a local LLM (llama.cpp) that acts out the suspect from their facts + pressure state (`HUInterrogation.h`, `UHUCaseSubsystem::Confront/Press/RaiseRecord`); offline voices (Kokoro / Chatterbox) whose emotion follows pressure. See the zero-cost plan: no per-use API calls in the final build.
- Real characters: the suspects, the uniforms at the tape, the Captain and a homicide's body are MetaHumans (`HUPeople`; built by `Tools/MakeCast.py`, reusing the existing cast when dedicated role models are absent). How they move: `HUPersonAnimation` - the engine's MetaHuman idle/walk/run clips (`HUPeople::Motion`) with sitting, crouching, cuffs, talking (`HUPeople::Talk`), flinching (`HUPeople::Flinch`), crossed/raised arms and head turns solved on top with IK; set its fields, never bone rotations by hand. Still to do: Motion Matching, real uniforms and coats (only the default garment exists), dedicated child meshes (the existing cast is scaled for those roles). Prop library for Fab/Megascans meshes, comic/noir post-process.
- Offline voices (Kokoro via NNE) to replace paid TTS.

## Briefing and motor pool

`Docs/BriefingRework.md` covers the lectern, projector dust, department corridor and three working sectional garage shutters. Press E on a shutter or its wall control. The centre cruiser is the squad car. Regenerate their meshes with `Tools/PropKit/make_briefing_props.py` and `Tools/ImportBriefingProps.py`; validate on `L_Briefing` with `-HUGarageTest`. Existing saved construction geometry refreshes at play start.
