"""Import only the two briefing props and create the soft additive projection material.
UnrealEditor-Cmd HomicideUnit.uproject -run=pythonscript -script=Tools/ImportBriefingProps.py
"""
import os
import unreal as u
e=u.EditorAssetLibrary
dest='/Game/Art/Props'
for name in ('BriefingPodium','ProjectorFrustum','FleetCruiser'):
    target=dest+'/SM_'+name
    folder=dest+'/Imported/'+name
    # Only these generated assets are replaced; keep every other prop untouched.
    if e.does_directory_exist(folder): e.delete_directory(folder)
    if e.does_asset_exist(target): e.delete_asset(target)
    task=u.AssetImportTask()
    task.filename=os.path.join(u.Paths.project_content_dir(),'Art','Props','Source',name,name+'.gltf')
    task.destination_path=dest+'/Imported/'+name
    task.automated=True; task.replace_existing=True; task.save=True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    meshes=[p for p in task.imported_object_paths if isinstance(u.load_asset(p),u.StaticMesh)]
    if len(meshes)!=1: raise RuntimeError('Expected one mesh for '+name)
    if not e.does_asset_exist(target):
        if not e.rename_asset(meshes[0].split('.')[0],target): raise RuntimeError('Cannot move '+name)
    mesh=u.load_asset(target)
    editor=u.get_editor_subsystem(u.StaticMeshEditorSubsystem) or u.EditorStaticMeshLibrary
    editor.remove_collisions(mesh)
    if name!='ProjectorFrustum': editor.add_simple_collisions(mesh,u.ScriptCollisionShapeType.BOX)
    else:
        path='/Game/Art/Materials/M_HU_ProjectorBeam'
        mat=u.load_asset(path) if e.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset('M_HU_ProjectorBeam','/Game/Art/Materials',u.Material,u.MaterialFactoryNew())
        mat.set_editor_property('blend_mode',u.BlendMode.BLEND_ADDITIVE)
        mat.set_editor_property('two_sided',True)
        mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
        lib=u.MaterialEditingLibrary
        lib.delete_all_material_expressions(mat)
        color=lib.create_material_expression(mat,u.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant',u.LinearColor(.25,.36,.55,1))
        fade=lib.create_material_expression(mat,u.MaterialExpressionDepthFade)
        fade.set_editor_property('opacity_default',.004)
        fade.set_editor_property('fade_distance_default',45)
        lib.connect_material_property(color,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        lib.connect_material_property(fade,'',u.MaterialProperty.MP_OPACITY)
        lib.recompile_material(mat); e.save_asset(path)
        mesh.set_material(0,mat)
    e.save_asset(target)
    u.log('BriefingProps: saved '+target)
e.save_directory(dest+'/Imported/BriefingPodium')
e.save_directory(dest+'/Imported/ProjectorFrustum')




path='/Game/Art/Materials/M_HU_ProjectorDust'
mat=u.load_asset(path) if e.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset('M_HU_ProjectorDust','/Game/Art/Materials',u.Material,u.MaterialFactoryNew())
mat.set_editor_property('shading_model',u.MaterialShadingModel.MSM_UNLIT)
lib=u.MaterialEditingLibrary
lib.delete_all_material_expressions(mat)
c=lib.create_material_expression(mat,u.MaterialExpressionConstant3Vector)
c.set_editor_property('constant',u.LinearColor(.5,.65,.9,1))
lib.connect_material_property(c,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
e.save_asset(path)
