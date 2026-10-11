#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Bind legacy split-BTF layout to THIS compilation's strict recursive graph.
No old lock/native graph, CRC, KCFI or executable code is reused or patched.
"""
from pathlib import Path
import sys,subprocess,hashlib,json,struct,importlib.util,collections,os
R=Path(sys.argv[1]).resolve(); target=Path(sys.argv[2]).resolve(); P=Path(os.environ['OS44_REPO_ROOT']);LIB=P/'lib/btf'
PYTHON=Path(sys.executable)
subprocess.run([str(PYTHON),'-B',str(LIB/'audit_changed_native_btf.py'),'--output',str(R),'--vmlinux',str(target)],check=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
sys.path.insert(0,str(LIB));from btf_reader import parse
b=R/'btf';a=json.loads((R/'native_btf_audit.json').read_text());native=(R/'native.btf').read_bytes()
assert native==(b/'native_build.btf').read_bytes()
assert a['strict_graph_unmatched']==2 and {x['stock_id'] for x in a['unmatched']}=={133230,159432}
stock=LIB/'stock_Image.btf';blob=stock.read_bytes();assert sha(stock)==a['stock_sha256']
st,_=parse(blob,0);nt,_=parse(native,0)
spec=importlib.util.spec_from_file_location('semantic',b/'reindex_deduplicated.py');s=importlib.util.module_from_spec(spec);spec.loader.exec_module(s)
meta=json.loads((LIB/'module_inventory.json').read_text());modules={};fixed=set();closure=set()
for n,m in meta['modules'].items():
 modules[n]={k:m[k] for k in ['module_file_sha256','module_btf_sha256']};fixed.update(m['module_base_string_offsets']);closure.update(m['base_closure_ids'])
pairs=a['mapping'];s.validate_pairs(st,nt,pairs)
protos=[{'stock_id':133230,'path':'net/netlink/af_netlink.c','declaration':'static void netlink_deliver_tap(struct net *net, struct sk_buff *skb)'},{'stock_id':159432,'path':'include/trace/events/sched.h','declaration':'TP_PROTO(int ret)'}]
assert closure=={x for x,y in pairs}|{p['stock_id'] for p in protos}
proofs=[]
for p in protos:
 src=R/'kernel'/p['path'];txt=src.read_text();assert p['declaration'] in txt
 proofs.append({**p,'source_sha256':sha(src),'line':txt[:txt.index(p['declaration'])].count('\n')+1,'stock_type':st[p['stock_id']-1]})
lock={'schema':1,'stock_sha256':sha(stock),'native_sha256':hashlib.sha256(native).hexdigest(),'output_types':len(st),'string_bytes':struct.unpack_from('<HBBIIIII',blob)[7],'strict_pairs':pairs,'fixed_string_offsets':sorted(fixed),'modules':modules,'source_prototypes':protos}
save(b/'candidate_lock.json',lock);save(b/'source_prototype_evidence.json',proofs)
nodes=[(['VOID'],[])]+[s.semantic_node(t) for t in nt];local=[json.dumps(x,sort_keys=True).encode() for x,e in nodes];h=[hashlib.sha256(x).digest() for x in local]
for _ in range(16):h=[hashlib.sha256(local[i]+b''.join(h[j] for j in e)).digest() for i,(x,e) in enumerate(nodes)]
groups=collections.defaultdict(list)
for t in nt:
 if t['kind'] not in ('FUNC','VAR','DATASEC','DECL_TAG'):groups[h[t['id']]].append(t['id'])
dup=[];cache=set()
def same(a,b):
 todo=[(a,b)];seen=set()
 while todo:
  x,y=todo.pop()
  if x==y or (x,y) in seen or (x,y) in cache:continue
  seen.add((x,y));ax,ex=nodes[x];ay,ey=nodes[y]
  if ax!=ay or len(ex)!=len(ey):return False
  todo.extend(zip(ex,ey))
 cache.update(seen);return True
for g in groups.values():
 reps=[]
 for i in g:
  rep=next((j for j in reps if same(i,j)),None)
  if rep is None:reps.append(i)
  else:dup.append([rep,i])
save(b/'dedup_audit.json',{'native_sha256':lock['native_sha256'],'native_types':len(nt),'exact_recursive_duplicate_pairs':dup,'potential_savings':len(dup),'metadata_modified':False,'fresh_computation':True})
save(b/'namespace_prepared.json',{'native_input':'current compilation .tmp_vmlinux1','old_lock_reused':False,'upstream_freeze':json.loads((P/'manifest/source.json').read_text())['bakasu_commit'],'pinned_types':len(closure),'fixed_string_offsets':len(fixed),'strict_graph_matches':len(pairs),'source_grounded_prototypes':len(protos),'dedup_pairs':len(dup),'source_grounded_not_original_private_type_recovery':True})
print('FRESH_BTF_LOCK_READY '+json.dumps({'pinned_types':len(closure),'native_types':len(nt),'dedup_pairs':len(dup)}),flush=True)
