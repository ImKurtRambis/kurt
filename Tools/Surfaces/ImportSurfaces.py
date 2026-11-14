# The game's surface library, in Unreal (run with the editor closed or from Tools > Execute Python Script):
#   UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script="<project>\Tools\Surfaces\ImportSurfaces.py"
# 1. imports Content/Art/Textures/Source/T_<Name>_{BaseColor,Normal,Roughness}.png (made by make_surfaces.py) into
#    /Game/Art/Materials/Textures, with the right settings for each kind of map
# 2. makes two parents in /Game/Art/Materials/Surfaces: M_HU_Surface (opaque) and M_HU_SurfaceGlass (see-through),
#    both world-aligned (no stretching on blocks of any size) with a Tint the game sets per block, and TileSizeCm
# 3. makes MI_Surface_<Name> for every surface - the 11 new ones and ChatGPT's plaster, linoleum and walnut maps -
#    which HUSurfaces.h loads by name.
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
SOURCE = unreal.Paths.project_content_dir() + "Art/Textures/Source/"
TEX = "/Game/Art/Materials/Textures"
MAT = "/Game/Art/Materials/Surfaces"

# name -> (texture prefix, tile size in cm, metallic). Tile size: how many cm of the world one repeat covers.
SURFACES = {
    "Brick":        ("T_Brick", 110.0, 0.0),
    "Concrete":     ("T_Concrete", 300.0, 0.0),
    "Asphalt":      ("T_Asphalt", 300.0, 0.0),
    "Tile":         ("T_Tile", 120.0, 0.0),
    "Carpet":       ("T_Carpet", 150.0, 0.0),
    "WoodFloor":    ("T_WoodFloor", 100.0, 0.0),
    "CeilingTile":  ("T_CeilingTile", 120.0, 0.0),
    "PaintedMetal": ("T_PaintedMetal", 200.0, 0.0),
    "Glass":        ("T_Glass", 150.0, 0.0),
    "Fabric":       ("T_Fabric", 60.0, 0.0),
    "Leather":      ("T_Leather", 80.0, 0.0),
    "Plaster":      ("M_Plaster", 200.0, 0.0),   # ChatGPT's maps (named M_ when they were imported)
    "Linoleum":     ("M_Linoleum", 100.0, 0.0),
    "Walnut":       ("M_Walnut", 200.0, 0.0),
}

# ---------- 1. textures ----------
tasks = []
for name, (prefix, _, _) in SURFACES.items():
    if not prefix.startswith("T_"): continue
    for kind in ("BaseColor", "Normal", "Roughness"):
        asset = f"{prefix}_{kind}"
        t = unreal.AssetImportTask()
        t.filename = SOURCE + asset + ".png"
        t.destination_path = TEX
        t.destination_name = asset
        t.automated = True
        t.save = True
        t.replace_existing = True   # re-running picks up regenerated maps
        tasks.append(t)
TOOLS.import_asset_tasks(tasks)
for name, (prefix, _, _) in SURFACES.items():
    if not prefix.startswith("T_"): continue
    for kind in ("BaseColor", "Normal", "Roughness"):
        tex = EAL.load_asset(f"{TEX}/{prefix}_{kind}")
        if not tex: raise RuntimeError("Missing texture after import: " + prefix + "_" + kind)
        tex.set_editor_property("srgb", kind == "BaseColor")
        if kind == "Normal":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("flip_green_channel", False)   # already DirectX
        elif kind == "Roughness":
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        EAL.save_loaded_asset(tex)

# ---------- 2. the parents ----------
EAL.make_directory(MAT)
ALIGNED = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture.WorldAlignedTexture")
ALIGNED_N = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal.WorldAlignedNormal")

def node(m, cls, x, y):
    n = LIB.create_material_expression(m, cls, x, y)
    if not n: raise RuntimeError("Could not create " + str(cls))
    return n

def parent(name, glass):
    path = f"{MAT}/{name}"
    m = EAL.load_asset(path) or TOOLS.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(m)
    if glass:
        m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
        m.set_editor_property("two_sided", True)
    size = node(m, unreal.MaterialExpressionScalarParameter, -1000, 800); size.set_editor_property("parameter_name", "TileSizeCm"); size.set_editor_property("default_value", 200.0)
    one = node(m, unreal.MaterialExpressionConstant3Vector, -1000, 920); one.set_editor_property("constant", unreal.LinearColor(1, 1, 1, 1))
    sz = node(m, unreal.MaterialExpressionMultiply, -760, 800)
    LIB.connect_material_expressions(size, "", sz, "A"); LIB.connect_material_expressions(one, "", sz, "B")

    def aligned(param, fn, out, y):
        obj = node(m, unreal.MaterialExpressionTextureObjectParameter, -1000, y); obj.set_editor_property("parameter_name", param)
        call = node(m, unreal.MaterialExpressionMaterialFunctionCall, -500, y); call.set_editor_property("material_function", fn)
        LIB.connect_material_expressions(obj, "", call, "TextureObject"); LIB.connect_material_expressions(sz, "", call, "TextureSize")
        return call, out

    base, bout = aligned("BaseColor", ALIGNED, "XYZ Texture", -300)
    tint = node(m, unreal.MaterialExpressionVectorParameter, -500, -460); tint.set_editor_property("parameter_name", "Tint"); tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    tinted = node(m, unreal.MaterialExpressionMultiply, -200, -300)
    LIB.connect_material_expressions(base, bout, tinted, "A"); LIB.connect_material_expressions(tint, "", tinted, "B")
    LIB.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough, rout = aligned("RoughnessMap", ALIGNED, "XYZ Texture", 100)
    LIB.connect_material_property(rough, rout, unreal.MaterialProperty.MP_ROUGHNESS)
    nrm, nout = aligned("NormalMap", ALIGNED_N, "XYZNormal", 450)
    LIB.connect_material_property(nrm, nout, unreal.MaterialProperty.MP_NORMAL)
    metal = node(m, unreal.MaterialExpressionScalarParameter, -500, -600); metal.set_editor_property("parameter_name", "Metallic"); metal.set_editor_property("default_value", 0.0)
    LIB.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    if glass:
        op = node(m, unreal.MaterialExpressionScalarParameter, -500, 650); op.set_editor_property("parameter_name", "Opacity"); op.set_editor_property("default_value", 0.25)
        LIB.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
        spec = node(m, unreal.MaterialExpressionConstant, -500, 750); spec.set_editor_property("r", 0.9)
        LIB.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    errs = LIB.recompile_material(m)
    if errs: raise RuntimeError(name + " compile errors: " + " | ".join(errs))
    EAL.save_loaded_asset(m)
    return m

opaque = parent("M_HU_Surface", False)
glass = parent("M_HU_SurfaceGlass", True)

# ---------- 3. one instance per surface ----------
for name, (prefix, tile, metallic) in SURFACES.items():
    iname = "MI_Surface_" + name
    mi = EAL.load_asset(f"{MAT}/{iname}") or TOOLS.create_asset(iname, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", glass if name == "Glass" else opaque)
    for param, kind in (("BaseColor", "BaseColor"), ("NormalMap", "Normal"), ("RoughnessMap", "Roughness")):
        tex = EAL.load_asset(f"{TEX}/{prefix}_{kind}")
        if not tex: raise RuntimeError("Missing texture " + prefix + "_" + kind)
        LIB.set_material_instance_texture_parameter_value(mi, param, tex)
    LIB.set_material_instance_scalar_parameter_value(mi, "TileSizeCm", tile)
    LIB.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    EAL.save_loaded_asset(mi)
    unreal.log(f"HU surfaces: {iname} ({prefix}, {tile:.0f} cm)")
unreal.log("HU surfaces: done - %d surfaces" % len(SURFACES))
