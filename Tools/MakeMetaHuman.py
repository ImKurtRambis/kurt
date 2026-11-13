# Make one suspect MetaHuman: create the asset, rig it and fetch textures (Epic cloud), add hair and clothes, build it.
# Run: UnrealEditor-Cmd HomicideUnit.uproject -run=pythonscript -script="Tools/MakeMetaHuman.py" -unattended
import unreal, sys

NAME = "MH_Suspect01"
FOLDER = "/Game/Characters/MetaHumans"
path = f"{FOLDER}/{NAME}.{NAME}"

def log(m): unreal.log_warning("MHMAKE " + m)

ch = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if not ch:
    ch = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.MetaHumanCharacter, unreal.new_object(type=unreal.MetaHumanCharacterFactoryNew))
    log("created")

def add(slot, item):
    w = unreal.load_asset(item)
    if not w: log("missing " + item); return
    k = ch.internal_collection.try_add_item_from_wardrobe_item(slot, w)
    ch.internal_collection.default_instance.try_add_slot_selection(unreal.MetaHumanPipelineSlotSelection(slot_name=slot, selected_item=k))

add("Hair", "/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/WI_Hair_S_Casual.WI_Hair_S_Casual")
add("Outfits", "/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment.WI_DefaultGarment")

sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)
if not sub.try_add_object_to_edit(ch): log("cannot edit"); sys.exit(1)
try:
    r = unreal.MetaHumanCharacterAutoRiggingRequestParams(); r.blocking = True; r.report_progress = False
    r.rig_type = unreal.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES
    sub.request_auto_rigging(ch, r); log("rig requested")
    t = unreal.MetaHumanCharacterTextureRequestParams(); t.blocking = True; t.report_progress = False
    sub.request_texture_sources(ch, t); log("textures requested")
    b = unreal.MetaHumanCharacterEditorBuildParameters()
    b.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
    b.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
    b.absolute_build_path = "/Game/MetaHumans"; b.common_folder_path = "/Game/MetaHumans/Common"
    b.enable_wardrobe_item_validation = False
    try: sub.build_meta_human(character=ch, params=b); log("built")
    except RuntimeError as e: log("built with warnings: %s" % str(e)[:200])
finally:
    if sub.is_object_added_for_editing(ch): sub.remove_object_to_edit(ch)
unreal.EditorAssetLibrary.save_directory("/Game/Characters"); unreal.EditorAssetLibrary.save_directory("/Game/MetaHumans")
log("done")

unreal.SystemLibrary.quit_editor()
