#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fresh recursive graph audit of native BTF vs frozen retained-module closure.

Reads actual new vmlinux. No previous candidate lock, ID rewrite or mutation.
Hash screening is followed by complete recursive type-graph comparison.
"""
import argparse, collections, hashlib, importlib.util, json, sys, os
from pathlib import Path
from elftools.elf.elffile import ELFFile
BASE=Path(__file__).resolve().parent;ROOT=Path(os.environ['OS44_REPO_ROOT'])
sys.path.insert(0,str(BASE))
from btf_reader import parse
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,default=BASE/'run_009');ap.add_argument('--vmlinux',type=Path);a=ap.parse_args();run=a.output.resolve();vmlinux=a.vmlinux.resolve() if a.vmlinux else run/'out/vmlinux'
    module_audit=BASE/'module_inventory.json'
    meta=json.loads(module_audit.read_text())
    target=set().union(*(set(m['base_closure_ids']) for m in meta['modules'].values()))
    stock=BASE/'stock_Image.btf'
    with vmlinux.open('rb') as f:native=ELFFile(f).get_section_by_name('.BTF').data()
    (run/'native.btf').write_bytes(native)
    st,_=parse(stock.read_bytes(),0);nt,_=parse(native,0)
    p=BASE/'reindex_deduplicated.py'
    spec=importlib.util.spec_from_file_location('semantics',p);sem=importlib.util.module_from_spec(spec);spec.loader.exec_module(sem)
    def node(t):
        attrs,edges=sem.semantic_node(t);return json.dumps(attrs,separators=(',',':')).encode(),edges
    A=[(b'VOID',[])]+[node(t) for t in st];B=[(b'VOID',[])]+[node(t) for t in nt]
    def fingerprints(nodes):
        local=[hashlib.sha256(k).digest() for k,e in nodes];h=local
        for _ in range(12):h=[hashlib.sha256(local[i]+b''.join(h[j] for j in edges)).digest() for i,(k,edges) in enumerate(nodes)]
        return h
    ha=fingerprints(A);hb=fingerprints(B);idx=collections.defaultdict(list)
    for i,h in enumerate(hb):idx[h].append(i)
    cache=set()
    def equivalent(a,b):
        todo=[(a,b)];seen=set()
        while todo:
            x,y=todo.pop()
            if (x,y) in seen or (x,y) in cache:continue
            seen.add((x,y));ka,ea=A[x];kb,eb=B[y]
            if ka!=kb or len(ea)!=len(eb):return False
            todo.extend(zip(ea,eb))
        cache.update(seen);return True
    matches=[];unmatched=[];assigned=set();same=collections.defaultdict(list)
    for t in nt:
        if t['name']:same[(t['kind'],t['name'])].append(t['id'])
    for i in sorted(target):
        choices=sorted(idx.get(ha[i],[]),key=lambda j:j in assigned);j=next((j for j in choices if equivalent(i,j)),None)
        if j is not None:matches.append([i,j]);assigned.add(j)
        else:
            t=st[i-1];unmatched.append({'stock_id':i,'kind':t['kind'],'name':t['name'],'same_named_native_ids':same.get((t['kind'],t['name']),[])})
    report={'status':'READ_ONLY_SEMANTIC_AUDIT_NOT_LEGACY_ID_ACCEPTANCE','native_sha256':hashlib.sha256(native).hexdigest(),'stock_sha256':sha(stock),'vmlinux_sha256':sha(vmlinux),'native_types':len(nt),'stock_types':len(st),'module_inventory_sha256':sha(module_audit),'frozen_module_metadata_records':len(meta['modules']),'base_closure_types':len(target),'strict_graph_matches':len(matches),'strict_graph_unmatched':len(unmatched),'mapping':matches,'unmatched':unmatched,'no_mutation':True,'old_lock_reused':False,'limits':['Frozen module metadata reused; executable modules not shipped or rehashed; not a new firmware audit','Source-built zram/node_cache replace retained allocator consumers, private stock types not certified','Equivalent graph does not preserve original type IDs or split-BTF string offsets','No legacy namespace build, lock or phone acceptance claimed']}
    (run/'native_btf_audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ['mapping']},indent=2))
if __name__=='__main__':main()
