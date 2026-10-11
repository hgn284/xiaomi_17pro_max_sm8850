#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Strict BTF v1 parser for immutable arm64 Image inputs; no decompiler claims."""
from pathlib import Path
import struct,json,hashlib,re,collections
P=Path(__file__).resolve().parents[1]
SIZES={0:0,1:4,2:0,3:12,4:12,5:12,6:8,7:0,8:0,9:0,10:0,11:0,12:0,13:8,14:4,15:12,16:0,17:4,18:0,19:12}
KINDS=['UNKN','INT','PTR','ARRAY','STRUCT','UNION','ENUM','FWD','TYPEDEF','VOLATILE','CONST','RESTRICT','FUNC','FUNC_PROTO','VAR','DATASEC','FLOAT','DECL_TAG','TYPE_TAG','ENUM64']
def parse(b,at):
 magic,ver,flags,hl,to,tl,so,sl=struct.unpack_from('<HBBIIIII',b,at);assert (magic,ver,flags,hl)==(0xeb9f,1,0,24)
 strings=b[at+hl+so:at+hl+so+sl];assert strings[0]==0 and strings[-1]==0
 def name(offset):
  assert 0<=offset<len(strings)
  end=strings.find(b'\0',offset);assert end>=offset
  return strings[offset:end].decode('utf8')
 out=[];pos=at+hl+to;end=pos+tl
 while pos<end:
  no,info,st=struct.unpack_from('<III',b,pos);pos+=12
  vlen=info&0xffff;kind=(info>>24)&0x1f;kflag=info>>31;assert kind in SIZES
  count=vlen if kind in (4,5,6,13,15,19) else 1
  n=SIZES[kind]*count;assert pos+n<=end
  payload=list(struct.unpack_from('<'+'I'*(n//4),b,pos)) if n else [];pos+=n
  r={'id':len(out)+1,'kind':KINDS[kind],'name':name(no),'vlen':vlen,'kind_flag':kflag,'size_or_type':st,'payload':payload}
  if kind in (4,5):
   r['members']=[{'name':name(payload[i]),'type_id':payload[i+1],'bit_offset':payload[i+2]&0xffffff if kflag else payload[i+2],'bit_size':payload[i+2]>>24 if kflag else 0} for i in range(0,n//4,3)]
  elif kind==13:r['params']=[{'name':name(payload[i]),'type_id':payload[i+1]} for i in range(0,n//4,2)]
  elif kind in (6,19):r['enum_names']=[name(payload[i]) for i in range(0,n//4,2 if kind==6 else 3)]
  out.append(r)
 assert pos==end
 for r in out:
  if r['kind'] in ('PTR','TYPEDEF','VOLATILE','CONST','RESTRICT','FUNC','FUNC_PROTO','VAR','DECL_TAG','TYPE_TAG'):assert r['size_or_type']<=len(out)
  for m in r.get('members',[])+r.get('params',[]):assert m['type_id']<=len(out)
 return out,b[at:at+hl+max(to+tl,so+sl)]
def type_name(t,idx,seen=None):
 if not idx:return 'void'
 seen=set() if seen is None else seen
 if idx in seen:return '<cycle>'
 seen=seen|{idx};r=t[idx-1];k=r['kind'];n=r['name']
 if n:return (k.lower()+' ' if k in ('STRUCT','UNION','ENUM') else '')+n
 if k=='PTR':return type_name(t,r['size_or_type'],seen)+' *'
 if k in ('CONST','VOLATILE','RESTRICT','TYPE_TAG'):return k.lower()+' '+type_name(t,r['size_or_type'],seen)
 if k=='ARRAY':return type_name(t,r['payload'][0],seen)+f"[{r['payload'][2]}]"
 if k=='FUNC_PROTO':return type_name(t,r['size_or_type'],seen)+' ('+', '.join(type_name(t,a['type_id'],seen) for a in r['params'])+')'
 return k.lower()
