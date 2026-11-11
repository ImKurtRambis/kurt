import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
TEXTURE_DIR = "/Game/Art/Materials/Textures"
MATERIAL_DIR = "/Game/Art/Materials/Precinct"
unreal.EditorAssetLibrary.make_directory(MATERIAL_DIR)

def asset(name):
    return unreal.EditorAssetLibrary.load_asset(TEXTURE_DIR + "/" + name)

def expression(material, cls, x, y):
    node = LIB.create_material_expression(material, cls, x, y)
    if not node:
        raise RuntimeError("Could not create expression " + str(cls))
    return node

def make_parent():
    path = MATERIAL_DIR + "/M_HU_WorldAlignedSurface"
    material = unreal.EditorAssetLibrary.load_asset(path)
    if not material:
        factory = unreal.MaterialFactoryNew()
        material = TOOLS.create_asset("M_HU_WorldAlignedSurface", MATERIAL_DIR, unreal.Material, factory)

    LIB.delete_all_material_expressions(material)
    world_aligned = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedTexture.WorldAlignedTexture")
    world_normal = unreal.load_asset("/Engine/Functions/Engine_MaterialFunctions01/Texturing/WorldAlignedNormal.WorldAlignedNormal")
    if not world_aligned or not world_normal:
        raise RuntimeError("Could not load Unreal world-aligned material functions")

    def texture_object(parameter, x, y):
        node = expression(material, unreal.MaterialExpressionTextureObjectParameter, x, y)
        node.set_editor_property("parameter_name", parameter)
        return node

    size = expression(material, unreal.MaterialExpressionScalarParameter, -900, 800)
    size.set_editor_property("parameter_name", "TileSizeCm")
    size.set_editor_property("default_value", 200.0)
    size_vector = expression(material, unreal.MaterialExpressionConstant3Vector, -900, 920)
    size_vector.set_editor_property("constant", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    size_multiply = expression(material, unreal.MaterialExpressionMultiply, -650, 800)
    LIB.connect_material_expressions(size, "", size_multiply, "A")
    LIB.connect_material_expressions(size_vector, "", size_multiply, "B")
    base_obj = texture_object("BaseColor", -900, -240)
    base_call = expression(material, unreal.MaterialExpressionMaterialFunctionCall, -350, -240)
    base_call.set_editor_property("material_function", world_aligned)
    LIB.connect_material_expressions(base_obj, "", base_call, "TextureObject")
    LIB.connect_material_expressions(size_multiply, "", base_call, "TextureSize")
    LIB.connect_material_property(base_call, "XYZ Texture", unreal.MaterialProperty.MP_BASE_COLOR)

    rough_obj = texture_object("RoughnessMap", -900, 120)
    rough_call = expression(material, unreal.MaterialExpressionMaterialFunctionCall, -350, 120)
    rough_call.set_editor_property("material_function", world_aligned)
    LIB.connect_material_expressions(rough_obj, "", rough_call, "TextureObject")
    LIB.connect_material_expressions(size_multiply, "", rough_call, "TextureSize")
    LIB.connect_material_property(rough_call, "XYZ Texture", unreal.MaterialProperty.MP_ROUGHNESS)

    normal_obj = texture_object("NormalMap", -900, 480)
    normal_call = expression(material, unreal.MaterialExpressionMaterialFunctionCall, -350, 480)
    normal_call.set_editor_property("material_function", world_normal)
    LIB.connect_material_expressions(normal_obj, "", normal_call, "TextureObject")
    LIB.connect_material_expressions(size_multiply, "", normal_call, "TextureSize")
    LIB.connect_material_property(normal_call, "XYZNormal", unreal.MaterialProperty.MP_NORMAL)

    errors = LIB.recompile_material(material)
    if errors:
        raise RuntimeError("Material compile errors: " + " | ".join(errors))
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material

def make_instance(material, name, base, normal, roughness, tile_size):
    path = MATERIAL_DIR + "/" + name
    instance = unreal.EditorAssetLibrary.load_asset(path)
    if not instance:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        instance = TOOLS.create_asset(name, MATERIAL_DIR, unreal.MaterialInstanceConstant, factory)
    instance.set_editor_property("parent", material)
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance, "BaseColor", asset(base))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance, "NormalMap", asset(normal))
    unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(instance, "RoughnessMap", asset(roughness))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance, "TileSizeCm", tile_size)
    unreal.EditorAssetLibrary.save_loaded_asset(instance)

parent = make_parent()
make_instance(parent, "MI_HU_Plaster", "M_Plaster_BaseColor", "M_Plaster_Normal", "M_Plaster_Roughness", 200.0)
make_instance(parent, "MI_HU_Linoleum", "M_Linoleum_BaseColor", "M_Linoleum_Normal", "M_Linoleum_Roughness", 100.0)
make_instance(parent, "MI_HU_Walnut", "M_Walnut_BaseColor", "M_Walnut_Normal", "M_Walnut_Roughness", 200.0)
unreal.log("Created precinct material parent and three instances.")
