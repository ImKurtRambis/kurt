# Homicide Unit — Unreal Engine 5 port

Blueprints + C++. The C++ holds the case rules; Blueprints hold the rooms, meshes, UI and wiring.

## What's here

| File | What it is |
|---|---|
| `Content/Data/Cases.json` | 8 full cases exported from the browser game's generator (victim, suspects, statements, lies, clues, solution) |
| `Source/HomicideUnit/HUCaseTypes.h` | Case data structs (`FHUCase`, `FHUSuspect`, `FHUStatement`, `FHUClue`, `FHUSlide`) |
| `HUCaseSubsystem` | The case in play: loads cases, `StartCase`, `FindClue`, `Challenge` (catch a lie), `Accuse`, briefing slides |
| `HUInteractable` | Interface for anything you press E / Q on |
| `HUInteractComponent` | On the player: looks at things, shows their prompt, sends E / Q |
| `HUBriefingLaptop` | The briefing-room laptop: clicks the projector slides, rolls out on the last one |

## 1. Create the project

1. Epic Games Launcher → Unreal Engine → Launch (5.x).
2. **Games → First Person**, **C++**, starter content on, name it **`HomicideUnit`** (the name matters: the code uses `HOMICIDEUNIT_API`).
3. Close the editor once it opens.

## 2. Drop the code in

1. Copy `Source/HomicideUnit/*.h` and `*.cpp` into your project's `Source/HomicideUnit/` (next to the files Unreal made).
2. Copy `Content/Data/Cases.json` into your project's `Content/Data/`.
3. Open `Source/HomicideUnit/HomicideUnit.Build.cs` and make the dependency line include these:

   ```csharp
   PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Json", "JsonUtilities", "UMG" });
   ```

4. Right-click `HomicideUnit.uproject` → **Generate Visual Studio project files**, then double-click the `.uproject` and say **Yes** to rebuild.
   If it fails, open the `.sln` in Visual Studio, build **Development Editor / Win64**, and send me the first error.

## 3. First slice: the briefing room

**Level** `L_Briefing`: a box room (Modeling Mode or cubes), rows of chairs facing a wall, a podium, a screen on the wall.

**Player** (`BP_FirstPersonCharacter`):
- Add Component → **HUInteract**.
- Input: an `IA_Use` action on **E** → `HUInteract → Use (Secondary = false)`; `IA_UseAlt` on **Q** → `Use (Secondary = true)`.
- HUD widget: a text block bound to `HUInteract → Prompt`.

**Slide widget** `WBP_Slide` (1024×576): a dark blue background, header bar, `Title` text, a vertical box for `Rows`, a `3 / 14` counter. Add a function `SetSlide(Slide, Index, Count)`.

**Laptop** `BP_BriefingLaptop` (parent class **HUBriefingLaptop**):
- Set the `Laptop` mesh (a flattened cube works to start), and give it collision.
- Add a **Widget Component** using `WBP_Slide` for the laptop screen.
- In the level, put a second Widget Component (or a Blueprint actor with one) on the wall screen, size about 300×170.
- Event **OnSlideChanged** → call `SetSlide` on both widgets, and show `Slide.CaptainLine` in a subtitle widget.
- Set `SceneLevel` to your scene level's name.

**Captain**: any mannequin (or MetaHuman) standing at the podium.

Play: walk to the laptop, E/Q flips slides, last slide E opens the scene.

## Next slices

1. Crime scene level: spawn suspects and clue markers from `Current.Suspects` / `Current.Clues` with `MapToWorld`, E on a clue → `FindClue`.
2. Interview widget: a suspect's statements; pick one + a found clue → `Challenge`.
3. Accusation once `CanAccuse`, and the verdict screen.
4. Comic look: a post-process material (outline + halftone + hatching).
