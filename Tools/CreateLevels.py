# Creates the game's levels (only the ones that don't exist yet):
#   /Game/Maps/L_Menu      - the desk menu (HU Desk Menu, menu game mode: no body, just the desk camera)
#   /Game/Maps/L_Briefing  - the briefing room, corridor and motor pool (HU Briefing Room)
#   /Game/Maps/L_Scene     - the crime scene (HU Crime Scene builds the case's map when play starts)
# Run from the command line (editor closed):
#   UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script="<path>\Tools\CreateLevels.py"
# or inside the editor: Tools > Execute Python Script.
import unreal

LEVELS = [
    ("/Game/Maps/L_Menu", "/Script/HomicideUnit.HUDeskMenu", "/Script/HomicideUnit.HUMenuGameMode"),
    ("/Game/Maps/L_Lobby", "/Script/HomicideUnit.HUSquadRoom", None),
    ("/Game/Maps/L_Briefing", "/Script/HomicideUnit.HUBriefingRoom", None),
    ("/Game/Maps/L_Scene", "/Script/HomicideUnit.HUCrimeScene", None),
]

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

for path, actor_class, game_mode in LEVELS:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log("HomicideUnit: %s already exists, leaving it alone" % path)
        continue
    if not level_editor.new_level(path):
        unreal.log_error("HomicideUnit: couldn't create %s" % path)
        continue
    cls = unreal.load_class(None, actor_class)
    actors.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
    # a player start (the game moves you to the right spot anyway)
    for i, (x, y) in enumerate([(0, 0), (-150, -120), (-150, 120), (150, 0)]):
        actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(x, y, 120))
    # a little sky light so nothing outside the rooms is pure black
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800))
    sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("intensity", 0.15)
    if game_mode:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, game_mode))
    level_editor.save_current_level()
    unreal.log("HomicideUnit: created %s" % path)
