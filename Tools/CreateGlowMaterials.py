# Creates the two self-lit materials the code tints at run time (parameter "Color", linear, can go above 1):
#   /Game/Art/Materials/M_HU_Glow     - opaque, unlit: lit windows across the street, neon, a monitor screen, a bulb
#   /Game/Art/Materials/M_HU_GlowAdd  - additive, unlit: rain streaks and drops on the glass catching the city's light
# The engine's BasicShapeMaterial only takes a base colour, so nothing made of blocks could glow before.
# Run: UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script="Tools\CreateGlowMaterials.py"
import unreal

ASSETS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
FOLDER = "/Game/Art/Materials"


def make(name, blend):
    path = f"{FOLDER}/{name}"
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    m = ASSETS.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", blend)
    m.set_editor_property("two_sided", True)
    # the city's lit windows and the rain are instanced meshes: without this they draw with the default material
    m.set_editor_property("used_with_instanced_static_meshes", True)
    color = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -420, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    MEL.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    EAL.save_asset(path, only_if_is_dirty=False)
    unreal.log(f"HomicideUnit: made {path}")


make("M_HU_Glow", unreal.BlendMode.BLEND_OPAQUE)
make("M_HU_GlowAdd", unreal.BlendMode.BLEND_ADDITIVE)
