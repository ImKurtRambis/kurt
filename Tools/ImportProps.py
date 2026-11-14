"""Imports the furniture made by Tools/PropKit/make_props.py into /Game/Art/Props/SM_<Name> (meshes) with their
materials and textures under /Game/Art/Props/Imported/<Name>/. Re-running replaces them.

  UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script="Tools\\ImportProps.py"

The game picks a prop up by that path (HUBuild::Prop) and falls back to its old blocks when it isn't there.
"""
import os
import unreal

SOURCE = os.path.join(unreal.Paths.project_content_dir(), "Art", "Props", "Source")
DEST = "/Game/Art/Props"
EAL = unreal.EditorAssetLibrary
# the editor subsystem isn't there when this runs as a commandlet; the older library is
MESHES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or getattr(unreal, "EditorStaticMeshLibrary", None)


def nanite(mesh):
    """Nanite on every prop (the glTF importer does it by default; this keeps it so if that ever changes): full detail
    close up, no LODs to pop, and virtual shadow maps cache Nanite meshes' shadows - see DefaultEngine.ini"""
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    mesh.set_editor_property("nanite_settings", settings)
    return "on ({} tris)".format(mesh.get_num_triangles(0))


def box_collision(mesh):
    """one box: the player and the camera can't pass through furniture"""
    if MESHES:
        MESHES.remove_collisions(mesh)
        if MESHES.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX) >= 0: return "box"
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    return "its own triangles"

names = sorted(d for d in os.listdir(SOURCE) if d != "PolyHaven" and os.path.isfile(os.path.join(SOURCE, d, d + ".gltf")))
for name in names:
    folder = DEST + "/Imported/" + name
    if EAL.does_directory_exist(folder): EAL.delete_directory(folder)
    target = DEST + "/SM_" + name
    if EAL.does_asset_exist(target): EAL.delete_asset(target)

    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE, name, name + ".gltf")
    task.destination_path = folder
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    meshes = [p for p in task.imported_object_paths if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    if len(meshes) != 1:
        unreal.log_error("ImportProps: {} gave {} static meshes ({})".format(name, len(meshes), list(task.imported_object_paths)))
        continue
    src = meshes[0].split(".")[0]
    if not EAL.rename_asset(src, target):
        unreal.log_error("ImportProps: couldn't move {} to {}".format(src, target)); continue
    mesh = unreal.load_asset(target)
    collision = box_collision(mesh)
    detail = nanite(mesh)
    EAL.save_asset(target)
    b = mesh.get_bounding_box()
    unreal.log("ImportProps: {:<18} X {:6.1f}..{:6.1f}  Y {:6.1f}..{:6.1f}  Z {:6.1f}..{:6.1f}  ({} materials, collision: {}, Nanite: {})".format(
        "SM_" + name, b.min.x, b.max.x, b.min.y, b.max.y, b.min.z, b.max.z, len(mesh.static_materials), collision, detail))

# moving the meshes leaves redirectors behind in the import folders
redirectors = [a for a in EAL.list_assets(DEST, recursive=True) if unreal.load_asset(a).__class__.__name__ == "ObjectRedirector"]
if redirectors:
    unreal.AssetToolsHelpers.get_asset_tools().fixup_referencers([unreal.load_asset(r) for r in redirectors])
    for r in redirectors:
        if EAL.does_asset_exist(r): EAL.delete_asset(r)
unreal.log("ImportProps: done, {} props".format(len(names)))
