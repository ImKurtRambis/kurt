# Builds the cast (the suspects, and the uniforms, the Captain and the victims): each MetaHuman starts from one of Epic's preset people (face, body, skin), gets hair and
# clothing in noir colours, is rigged and textured by Epic's cloud, then built to /Game/MetaHumans/MH_<Name>.
# Must run in the FULL editor (texture baking needs rendered frames):
#   UnrealEditor.exe HomicideUnit.uproject -ExecutePythonScript="Tools/MakeCast.py" -unattended
# Already-built characters are skipped; delete /Game/MetaHumans/MH_<Name> to rebuild one.
# The game (HUSuspectActor::SpawnLook) casts these by suspect gender: names alternate man/woman in HUCaseGen.
import unreal

HAIR = "/MetaHumanCharacter/Optional/Grooms/Bindings/Hair/"
# preset, hair, shirt colour, trouser colour      (men first, then women - keep in step with HUSuspectActor.cpp)
CAST = [
    ("Walter",  "WI_Hair_S_BaldingStubble", (0.35, 0.33, 0.30), (0.05, 0.05, 0.06)),
    ("Bruce",   "WI_Hair_S_Clean",          (0.10, 0.12, 0.18), (0.08, 0.07, 0.06)),
    ("Victor",  "WI_Hair_S_BrushCut",       (0.40, 0.36, 0.28), (0.12, 0.10, 0.08)),
    ("Lorenzo", "WI_Hair_S_Casual",         (0.06, 0.06, 0.07), (0.04, 0.04, 0.05)),
    ("Vivian",  "WI_Hair_M_BobSlick",       (0.30, 0.04, 0.05), (0.04, 0.04, 0.05)),
    ("Grace",   "WI_Hair_S_LowPonytail",    (0.12, 0.16, 0.12), (0.10, 0.09, 0.08)),
    ("Celeste", "WI_Hair_M_BobCurly",       (0.45, 0.42, 0.36), (0.06, 0.06, 0.08)),
    ("Etta",    "WI_Hair_M_Layered",        (0.08, 0.08, 0.16), (0.05, 0.05, 0.05)),
]
# everyone else at a case, on faces no suspect uses (every case seats all eight suspect faces): the uniforms at the tape
# (AHUOfficer), the Captain (AHUBriefingRoom) and the victims (AHUVictim, by the victim's gender) - see HUPeople.h
# (no comments at the ends of these rows: FixCastClothing.py reads the colours from them line by line)
EXTRAS = [
    # the uniforms: navy shirt and trousers
    ("Mateo",   "WI_Hair_S_BuzzCut",        (0.03, 0.05, 0.12), (0.02, 0.03, 0.07)),
    ("Kelvin",  "WI_Hair_S_CoilBuzzCut",    (0.03, 0.05, 0.12), (0.02, 0.03, 0.07)),
    ("Zuri",    "WI_Hair_S_PulledBack",     (0.03, 0.05, 0.12), (0.02, 0.03, 0.07)),
    # the Captain: white shirt
    ("Orlando", "WI_Hair_S_SlickBack",      (0.80, 0.80, 0.77), (0.03, 0.03, 0.04)),
    # the victims
    ("Dominic", "WI_Hair_S_Messy",          (0.30, 0.26, 0.20), (0.05, 0.05, 0.06)),
    ("Jorge",   "WI_Hair_S_RecedeMessy",    (0.12, 0.10, 0.09), (0.06, 0.05, 0.05)),
    ("Jelena",  "WI_Hair_M_BobStraight",    (0.35, 0.10, 0.10), (0.05, 0.05, 0.06)),
    ("Sunita",  "WI_Hair_S_Updo",           (0.15, 0.20, 0.28), (0.05, 0.05, 0.06)),
]

def log(m): unreal.log_warning("MHCAST " + m)
EAL = unreal.EditorAssetLibrary
sub = unreal.get_editor_subsystem(unreal.MetaHumanCharacterEditorSubsystem)

def build(name, hair, shirt, pants):
    out = "/Game/MetaHumans/MH_" + name
    if EAL.does_asset_exist(out + "/BP_MH_" + name): log(name + " already built"); return
    src = "/Game/Characters/MetaHumans/MH_" + name
    if not EAL.does_asset_exist(src):
        EAL.duplicate_asset("/MetaHumanCharacter/Optional/Presets/%s.%s" % (name, name), src)
    ch = unreal.load_asset(src)
    col = ch.internal_collection
    def add(slot, path):
        w = unreal.load_asset(path)
        if not w: log("missing " + path); return None
        k = col.try_add_item_from_wardrobe_item(slot, w)
        col.default_instance.try_add_slot_selection(unreal.MetaHumanPipelineSlotSelection(slot_name=slot, selected_item=k))
        return k
    hk = add("Hair", HAIR + hair + "." + hair)
    ok = add("Outfits", "/MetaHumanCharacter/Optional/Clothing/WI_DefaultGarment.WI_DefaultGarment")
    if not sub.try_add_object_to_edit(ch): log(name + ": cannot edit"); return
    try:
        sub.assemble_for_preview(character=ch)
        if ok:
            for p in col.default_instance.get_instance_parameters(item_path=unreal.MetaHumanPaletteItemPath(item_key=ok)):
                n = str(p.name)
                c = shirt if ("Shirt" in n or "Top" in n) else pants if ("Pant" in n or "Short" in n or "Bottom" in n) else None
                if c and "Color" in n: p.set_color(value=unreal.LinearColor(c[0], c[1], c[2], 1)); log("  %s %s" % (name, n))
        r = unreal.MetaHumanCharacterAutoRiggingRequestParams(); r.blocking = True; r.report_progress = False
        r.rig_type = unreal.MetaHumanRigType.JOINTS_AND_BLEND_SHAPES
        sub.request_auto_rigging(ch, r)
        t = unreal.MetaHumanCharacterTextureRequestParams(); t.blocking = True; t.report_progress = False
        sub.request_texture_sources(ch, t)
        b = unreal.MetaHumanCharacterEditorBuildParameters()
        b.pipeline_type = unreal.MetaHumanDefaultPipelineType.OPTIMIZED
        b.pipeline_quality = unreal.MetaHumanQualityLevel.HIGH
        b.absolute_build_path = "/Game/MetaHumans"; b.common_folder_path = "/Game/MetaHumans/Common"
        b.enable_wardrobe_item_validation = False
        # the face control rigs log harmless "cannot break link" errors, which Python raises: build anyway
        try: sub.build_meta_human(character=ch, params=b)
        except RuntimeError as e: log(name + " built with rig warnings")
    finally:
        if sub.is_object_added_for_editing(ch): sub.remove_object_to_edit(ch)
    # the builder also puts shared pieces in Common (clothing materials, groom assets for hair, brows, beards):
    # unsaved, they exist only in this editor's memory and every character loads without them
    EAL.save_directory(src.rsplit("/", 1)[0]); EAL.save_directory(out); EAL.save_directory("/Game/MetaHumans/Common")
    log(name + (" built" if EAL.does_asset_exist(out + "/BP_MH_" + name) else " FAILED"))

for row in CAST + EXTRAS:
    try: build(*row)
    except Exception as e: log("%s error: %s" % (row[0], str(e)[:300]))
# the builder leaves most body shapes' clothing without a base material (grey checker): repair it, apply colours.
# (A parent that's only in memory looks fine here and fails in the game: run FixCastClothing.py again headless after.)
import os; exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "FixCastClothing.py"), encoding="utf-8").read())
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "FixCastClothing.py"), encoding="utf-8").read())  # second pass: colours need the parent
log("done")
unreal.SystemLibrary.quit_editor()
