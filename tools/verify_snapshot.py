#!/usr/bin/env python3
"""Verify archived source hashes; this does not validate a device or a build."""
import argparse, hashlib, json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=Path);a=p.parse_args()
m=json.loads((a.snapshot/'snapshot.json').read_text())
for item in m['files']:
    f=a.snapshot/item['path'];b=f.read_bytes()
    if len(b)!=item['size'] or hashlib.sha256(b).hexdigest()!=item['sha256']:raise SystemExit('Mismatch: '+str(f))
print(m['snapshot_id']+': source hashes PASS; device/build status is separate')
