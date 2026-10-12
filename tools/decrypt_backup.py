#!/usr/bin/env python3
"""Decrypt the exact original backup locally. Never upload the private key."""
import argparse, base64, hashlib, json
from pathlib import Path
from cryptography.hazmat.primitives import serialization, hashes
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive',type=Path);p.add_argument('private_key',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args()
    if a.output.exists():p.error('output already exists; choose a new file')
    b=a.archive.read_bytes()
    metadata=a.archive.parent/'archive.json'
    if metadata.exists():
        m=json.loads(metadata.read_text())
        if hashlib.sha256(b).hexdigest()!=m['sha256']:p.error('encrypted archive SHA-256 mismatch')
    header,cipher=b.split(b'\n',1);h=json.loads(header)
    if h['format']!='BOILER-HYBRID-v1':p.error('unsupported archive format')
    private=serialization.load_pem_private_key(a.private_key.read_bytes(),password=None)
    key=private.decrypt(base64.b64decode(h['wrapped_key']),padding.OAEP(mgf=padding.MGF1(hashes.SHA256()),algorithm=hashes.SHA256(),label=None))
    plain=AESGCM(key).decrypt(base64.b64decode(h['nonce']),cipher,header)
    with a.output.open('xb') as f:f.write(plain)
    a.output.chmod(0o600)
    print('Authenticated archive decrypted; contains private network credentials.')

if __name__=='__main__':main()
