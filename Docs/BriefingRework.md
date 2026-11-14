# Briefing, department corridor and motor pool

`L_Briefing` keeps the existing briefing, 911 call, seating, supply locker and drive-to-scene flow.

- Forty real stacking chairs face a walnut lectern with a reading shelf, sloped top, microphone and notes.
- The ceiling projector casts a soft blue frustum onto the screen. Seventy-two small dust motes drift inside it without collision or shadows. Room lighting is dimmer than the departmental corridor.
- The corridor has framed walnut doors marked Records, Evidence, Traffic and Detective Bureau, wired vision panels, kick plates, lever handles and garage wayfinding. Department doors are scenery; the route to the garage remains clear.
- The garage has three period fleet sedans, irregular oil darkening, tire tracks, worn parking paint, maintenance benches with vises and pegboards, a drawer chest and tire storage. The centre cruiser is the squad's car; the other two are patrol cars.
- Look at a shutter or its wall control and press E to open/close that bay. Each of its twelve sections follows a vertical track, a curved elbow and an overhead rail. Collision moves with the sections. A closing door pauses while a pawn occupies its threshold. There is a small exterior apron with amber light beyond the shutters.

## Implementation

`HUBriefingRoom` builds the room, corridor, projection and dust. `HUMotorPool` builds the garage and implements shutter interactions through the normal squad replay interface. `HUCruiser` uses the fleet mesh with the original block fallback.

These actors refresh their construction geometry in `BeginPlay`: existing maps and saved child-actor templates otherwise retain the old solid garage wall and cars even after the C++ changes. The back-wall cap rail is split around the exit rather than bridging the doorway.

The art uses the existing precinct mesh kit; no external downloads:

```
AI\python\python.exe -I Tools\PropKit\make_briefing_props.py
UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script=Tools\ImportBriefingProps.py
```

This replaces only the three generated props (`BriefingPodium`, `FleetCruiser`, `ProjectorFrustum`) and their import folders. The beam uses an additive, unlit, depth-faded material. `/Game/Art` is already always cooked. Keep the generated assets in git alongside the scripts.

## Review

- `-HUGarageTest -HUNoVoice -HUNoBrain` on `L_Briefing` checks the corridor at capsule height, opens all bays, traces through each opening, then closes and checks collision again. Look for three `HUGarageTest: PASSED` lines. Allow time for map startup plus twelve seconds of game time.
- `-HUGarageOpen` starts the centre bay opening for visual review.
- Existing `-HUShots` can photograph the room and route, e.g. `-HUShots=-600,-550,20,5|-1050,0,180,0|-1570,0,180,-8|-2870,0,0,0|-2050,650,60,-12 -HUShotsName=BriefingRework`.

Validation results are recorded in the change's commit description. Multiplayer interactions use the existing replay mechanism; a multi-PC session still needs a separate playtest.
