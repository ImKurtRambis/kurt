"""Import only the two ambient-life sounds. /Game/Art is already always cooked."""
import os
import unreal as u

source=os.path.join(os.path.dirname(os.path.abspath(__file__)),'StaffAudio')
dest='/Game/Art/Audio/PrecinctLife'
for name in ('SFX_OfficePhone','AMB_OfficeFan'):
    task=u.AssetImportTask()
    task.filename=os.path.join(source,name+'.wav')
    task.destination_path=dest
    task.automated=True; task.replace_existing=True; task.save=True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound=u.load_asset(dest+'/'+name)
    if not sound: raise RuntimeError('Missing '+name)
    sound.set_editor_property('looping',name.startswith('AMB_'))
    u.EditorAssetLibrary.save_loaded_asset(sound,False)
    u.log('PrecinctLifeAudio: saved '+name)
