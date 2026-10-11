#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
from pathlib import Path
import os,subprocess,sys,tempfile
R=Path(os.environ['OS44_BUILD_ROOT']);P=Path(os.environ['OS44_REPO_ROOT'])
TOOL=Path(os.environ['OS44_REAL_PAHOLE'])
subprocess.run([str(TOOL),*sys.argv[1:]],check=True)
if '-J' not in sys.argv[1:]:sys.exit(0)
target=Path(sys.argv[-1]).resolve()
if target.suffix=='.ko' and (target.is_relative_to(R/'out') or target.is_relative_to(R/'qpace_extension') or target.is_relative_to(R/'combined_fixture')):
 assert '--btf_base' in sys.argv and Path(sys.argv[sys.argv.index('--btf_base')+1]).resolve()==R/'out/vmlinux'
 sys.exit(0)
assert target.parent==R/'out' and target.name=='.tmp_vmlinux1'
objcopy=os.environ['LLVM_OBJCOPY']
with tempfile.TemporaryDirectory(prefix='btf-namespace-',dir=target.parent) as work:
 d=Path(work);native=d/'native.btf';output=d/'reindexed.btf'
 subprocess.run([objcopy,'--dump-section','.BTF='+str(native),str(target)],check=True)
 (R/'btf/native_build.btf').write_bytes(native.read_bytes())
 subprocess.run([sys.executable,str(P/'lib/btf/prepare_namespace.py'),str(R),str(target)],check=True)
 subprocess.run([sys.executable,str(R/'btf/reindex_deduplicated.py'),'--native',str(native),'--stock',str(P/'lib/btf/stock_Image.btf'),'--lock',str(R/'btf/candidate_lock.json'),'--source',str(R/'kernel'),'--output',str(output),'--report',str(R/'btf/build_reindex_report.json')],check=True)
 subprocess.run([objcopy,'--update-section','.BTF='+str(output),str(target)],check=True)
