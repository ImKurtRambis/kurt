# Imports Tools/Sounds/*.wav (made by MakeSounds.py) into /Game/Audio. Loops (AMB_*, MUS_*) are set to loop.
# Run: UnrealEditor-Cmd HomicideUnit.uproject -run=pythonscript -script="Tools/ImportSounds.py"
import unreal, os, glob
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Sounds")
tasks = []
for f in sorted(glob.glob(os.path.join(SRC, "*.wav"))):
    t = unreal.AssetImportTask(); t.filename = f; t.destination_path = "/Game/Audio"
    t.automated = True; t.replace_existing = True; t.save = True; tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    name = os.path.splitext(os.path.basename(t.filename))[0]
    w = unreal.load_asset("/Game/Audio/" + name)
    if not w: unreal.log_warning("HUSOUND failed " + name); continue
    if name.startswith(("AMB_", "MUS_")): w.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_loaded_asset(w, False)
    unreal.log_warning("HUSOUND imported " + name)
