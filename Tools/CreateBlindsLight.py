# Makes /Game/Art/Materials/M_HU_BlindsLight: a light function that throws a window and its venetian blinds onto the
# room, as a streetlight far outside would (HULamps::Window). Each pixel's ray back to the streetlight is crossed with
# the window's plane: inside the frame, off the mullion and between two slats, it's lit. The window is given in world
# space through parameters, so one material serves every window (as a dynamic instance).
# Run: UnrealEditor-Cmd.exe HomicideUnit.uproject -run=pythonscript -script="<project>\Tools\CreateBlindsLight.py"
import unreal

EAL = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
PATH = "/Game/Art/Materials/M_HU_BlindsLight"

CODE = r"""
float3 wp = WP.xyz, lp = LP.xyz, o = O.xyz, n = N.xyz, r = R.xyz, u = U.xyz;
float3 d = wp - lp;
float dn = dot(d, n);
if (dn <= 0.001) return 0;
float t = dot(o - lp, n) / dn;
if (t <= 0.0 || t >= 1.0) return 0;          // the window has to be between the street and this point
float3 h = lp + d * t;
float x = dot(h - o, r), y = dot(h - o, u);
float frame = step(abs(x), Size.x) * step(abs(y), Size.y);
float mullion = step(Size.z, abs(x));
float f = frac((y + 1000.0) / Slats.x);       // where between two slats it crossed
float open = smoothstep(Slats.y - 0.06, Slats.y + 0.06, f);
return frame * mullion * open;
"""

m = EAL.load_asset(PATH)
if not m:
    m = TOOLS.create_asset("M_HU_BlindsLight", "/Game/Art/Materials", unreal.Material, unreal.MaterialFactoryNew())
LIB.delete_all_material_expressions(m)
m.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)

custom = LIB.create_material_expression(m, unreal.MaterialExpressionCustom, -300, 0)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT1)
custom.set_editor_property("code", CODE)
names = ["WP", "LP", "O", "N", "R", "U", "Size", "Slats"]
inputs = []
for name in names:
    ci = unreal.CustomInput()
    ci.set_editor_property("input_name", name)
    inputs.append(ci)
custom.set_editor_property("inputs", inputs)

wp = LIB.create_material_expression(m, unreal.MaterialExpressionWorldPosition, -800, -300)
LIB.connect_material_expressions(wp, "", custom, "WP")
defaults = {
    "LP": (-600, 0, 400), "O": (0, 0, 175), "N": (1, 0, 0), "R": (0, 1, 0), "U": (0, 0, 1),
    "Size": (60, 70, 1.5), "Slats": (4.6, 0.55, 0),
}
for i, name in enumerate(names[1:]):
    p = LIB.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -800, -150 + i * 130)
    p.set_editor_property("parameter_name", name)
    x, y, z = defaults[name]
    p.set_editor_property("default_value", unreal.LinearColor(x, y, z, 0))
    LIB.connect_material_expressions(p, "", custom, name)

LIB.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
errors = LIB.recompile_material(m)
if errors:
    raise RuntimeError("M_HU_BlindsLight: " + " | ".join(errors))
EAL.save_loaded_asset(m)
unreal.log("HU blinds light: saved " + PATH)
