"""Refresh and save the canonical station maps; never create a competing version.

Build first, then run in a full editor (a separate hidden instance is fine):
  UnrealEditor.exe HomicideUnit.uproject -ExecutePythonScript=Tools/RefreshStationMaps.py -unattended
Copies the previous .umaps to LocalAppData/HomicideUnit/MapBackups before saving.
Keeps all existing actors, player starts, names and gameplay settings.
"""
from pathlib import Path
import datetime, hashlib, json, os, shutil
import unreal as u

project=Path(__file__).resolve().parents[1]
revision='station-2026-11-14-staffed'
maps=(('L_Precinct','HUPrecinct'),('L_Briefing','HUBriefingRoom'),('L_Lobby','HUSquadRoom'))
backup=Path(os.environ['LOCALAPPDATA'])/'HomicideUnit'/'MapBackups'/datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True,exist_ok=True)
for name,_ in maps:
    source=project/'Content'/'Maps'/(name+'.umap')
    if not source.is_file(): raise RuntimeError('Expected existing canonical map: '+str(source))
    shutil.copy2(source,backup/source.name)

levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
if not levels or not actors: raise RuntimeError('Use -ExecutePythonScript in a full editor, not -run=pythonscript')
def mark_saved(actor):
    tags=list(actor.get_editor_property('tags'))
    if u.Name('HU_SavedStationLayout') not in tags: tags.append(u.Name('HU_SavedStationLayout'))
    actor.set_editor_property('tags',tags)
    for child in actor.get_components_by_class(u.ChildActorComponent):
        if child.get_editor_property('child_actor'): mark_saved(child.get_editor_property('child_actor'))

records=[]
for name,kind in maps:
    path='/Game/Maps/'+name
    if not levels.load_level(path): raise RuntimeError('Could not open '+path)
    builders=[a for a in actors.get_all_level_actors() if a.get_class().get_path_name()=='/Script/HomicideUnit.'+kind]
    if len(builders)!=1: raise RuntimeError(path+' must have exactly one '+kind)
    builder=builders[0]
    u.HUStationMapLibrary.refresh_builder(builder)
    mark_saved(builder)
    builder.set_actor_label(name+' / canonical station layout')
    if name=='L_Precinct':
        existing=[a for a in actors.get_all_level_actors() if isinstance(a,u.HUPrecinctLifeLayout)]
        if len(existing)>1: raise RuntimeError('Duplicate staff layouts in '+path)
        if not existing:
            layout=actors.spawn_actor_from_class(u.HUPrecinctLifeLayout,builder.get_actor_location(),builder.get_actor_rotation())
            if not layout: raise RuntimeError('Could not create permanent staff layout')
            layout.set_actor_label('Precinct staff / saved positions, phones and fan')
        else: layout=existing[0]
        if len(layout.get_editor_property('staff'))!=6: raise RuntimeError('Expected six saved background staff slots')
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    u.EditorAssetLibrary.set_metadata_tag(world,'HU_MapRevision',revision)
    if not levels.save_current_level(): raise RuntimeError('Save failed: '+path)
    file=project/'Content'/'Maps'/(name+'.umap')
    records.append(dict(map=path,builder=kind,sha256=hashlib.sha256(file.read_bytes()).hexdigest(),bytes=file.stat().st_size))
    u.log('StationMaps: SAVED '+path+' ('+revision+')')

# Reopen from disk: this catches a transient edit that was never actually saved.
for name,kind in maps:
    path='/Game/Maps/'+name
    if not levels.load_level(path): raise RuntimeError('Reopen failed: '+path)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if u.EditorAssetLibrary.get_metadata_tag(world,'HU_MapRevision')!=revision: raise RuntimeError('Revision was not saved: '+path)
    if name=='L_Precinct' and len([a for a in actors.get_all_level_actors() if isinstance(a,u.HUPrecinctLifeLayout)])!=1:
        raise RuntimeError('Staff layout did not survive reopening')
    u.log('StationMaps: REOPENED '+path+' - saved revision verified')

(project/'Content'/'Maps'/'StationMaps.json').write_text(json.dumps(dict(revision=revision,maps=records),indent=2)+'\n',encoding='utf-8')
levels.load_level('/Game/Maps/L_Precinct')
u.log('StationMaps: PASSED - three canonical map assets saved and reopened; backups in '+str(backup))
u.SystemLibrary.quit_editor()
