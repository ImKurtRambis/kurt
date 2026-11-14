"""Verify that the canonical saved station assets match their committed revision manifest."""
from pathlib import Path
import hashlib, json

root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'Content/Maps/StationMaps.json').read_text(encoding='utf-8'))
failed=[]
for row in manifest['maps']:
    path=root/'Content/Maps'/(row['map'].rsplit('/',1)[-1]+'.umap')
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=row['sha256']:
        failed.append(row['map'])
if failed:
    raise SystemExit('StationMaps: FAILED saved asset revision: '+', '.join(failed))
print('StationMaps: PASSED - '+manifest['revision']+'; '+str(len(manifest['maps']))+' permanent map assets match')
