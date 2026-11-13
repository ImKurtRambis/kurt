# Repairs clothing on built MetaHumans. The MetaHuman builder copies the default garment's base materials into
# /Game/MetaHumans/Common only for the first body shape it builds; characters with other body shapes get material
# instances with no parent, which render as the grey checker. This points each orphan back at the plugin's material.
# Run (headless is fine): UnrealEditor-Cmd HomicideUnit.uproject -run=pythonscript -script="Tools/FixCastClothing.py"
import unreal, re, os, ast
# the cast's colours: read from MakeCast.py's CAST table (preset, hair, shirt rgb, trouser rgb)
_src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "MakeCast.py"), encoding="utf-8").read()
CAST = {}
for _m in re.finditer(r'^\s*\(\s*"(\w+)".*\),?\s*$', _src, re.M):
    _r = ast.literal_eval(_m.group(0).strip().rstrip(","))
    CAST[_r[0]] = _r
EAL = unreal.EditorAssetLibrary
PLUGIN = "/MetaHumanCharacter/Optional/Clothing/DefaultGarment/ClothAssets/%s/Materials/M_DG_%s_%s"
fixed = 0
for path in EAL.list_assets("/Game/MetaHumans", recursive=True, include_folder=False):
    if "/Clothing/MI_" not in path: continue
    mi = unreal.load_asset(path)
    if mi: mi.modify()
    if not isinstance(mi, unreal.MaterialInstanceConstant): continue
    who = re.search(r"/MH_(\w+?)/", path)
    row = CAST.get(who.group(1)) if who else None
    if row:
        c = row[2] if "_Shirt" in path else row[3]
        for n in ("diffuse_color_1", "diffuse_color_2"):
            unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, n, unreal.LinearColor(c[0], c[1], c[2], 1))
        EAL.save_loaded_asset(mi, False)
    if mi.get_editor_property("parent"): continue
    m = re.search(r"_M_DG_(bodyShape\w)_(Shirt|Short)", path)
    if not m: unreal.log_warning("FIXCLOTH can't tell what %s is" % path); continue
    parent = unreal.load_asset(PLUGIN % (m.group(1), m.group(1), m.group(2)))
    if not parent: unreal.log_warning("FIXCLOTH no plugin material for %s" % path); continue
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, parent)
    EAL.save_loaded_asset(mi, False)
    fixed += 1
unreal.log_warning("FIXCLOTH fixed %d clothing materials" % fixed)
