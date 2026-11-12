import unreal

SOURCE_DIR = unreal.Paths.project_content_dir() + "Art/Textures/Source/"
DESTINATION = "/Game/Art/Materials/Textures"

ASSETS = [
    "M_Plaster_BaseColor.jpg", "M_Plaster_Normal.jpg", "M_Plaster_Roughness.jpg",
    "M_Linoleum_BaseColor.jpg", "M_Linoleum_Normal.jpg", "M_Linoleum_Roughness.jpg",
    "M_Walnut_BaseColor.jpg", "M_Walnut_Normal.jpg", "M_Walnut_Roughness.jpg",
    "M_ComputerPlastic_BaseColor.jpg", "M_ComputerPlastic_Normal.jpg", "M_ComputerPlastic_Roughness.jpg",
    "M_ManilaFolder_BaseColor.jpg", "M_ManilaFolder_Normal.jpg", "M_ManilaFolder_Roughness.jpg",
    "M_VictimWool_BaseColor.jpg", "M_VictimWool_Normal.jpg", "M_VictimWool_Roughness.jpg",
]

tasks = []
for filename in ASSETS:
    name = filename[:-4]
    task = unreal.AssetImportTask()
    task.filename = SOURCE_DIR + filename
    task.destination_path = DESTINATION
    task.destination_name = name
    task.automated = True
    task.save = True
    task.replace_existing = True
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
