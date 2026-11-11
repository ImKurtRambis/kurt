import unreal

SOURCE_DIR = unreal.Paths.project_content_dir() + "Art/Textures/Source/"
DESTINATION = "/Game/Art/Materials/Textures"

ASSETS = [
    "M_Plaster_BaseColor.png", "M_Plaster_Normal.png", "M_Plaster_Roughness.png",
    "M_Linoleum_BaseColor.png", "M_Linoleum_Normal.png", "M_Linoleum_Roughness.png",
    "M_Walnut_BaseColor.png", "M_Walnut_Normal.png", "M_Walnut_Roughness.png",
]

tasks = []
for filename in ASSETS:
    name = filename[:-4]
    if unreal.EditorAssetLibrary.load_asset(DESTINATION + "/" + name):
        continue
    task = unreal.AssetImportTask()
    task.filename = SOURCE_DIR + filename
    task.destination_path = DESTINATION
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = False
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for task in tasks:
    unreal.log("Precinct texture import: {} -> {}".format(task.filename, task.imported_object_paths))

for filename in ASSETS:
    name = filename[:-4]
    texture = unreal.EditorAssetLibrary.load_asset(DESTINATION + "/" + name)
    if not texture:
        raise RuntimeError("Imported texture is missing: " + name)
    is_color = name.endswith("_BaseColor")
    is_normal = name.endswith("_Normal")
    texture.set_editor_property("srgb", is_color)
    if is_normal:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("flip_green_channel", False)
    elif not is_color:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)
