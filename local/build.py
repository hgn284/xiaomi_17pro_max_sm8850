#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build the frozen OS44 source. No device access, tests or automatic flashing."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work"


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--jobs", type=int, default=2)
    options = parser.parse_args()
    if options.jobs < 1:
        parser.error("--jobs must be positive")
    if not (WORK / "kernel/Makefile").is_file():
        parser.error("Run local/prepare.py first")
    lock = (WORK / "build.lock").open("w")
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    src, out = WORK / "kernel", WORK / "out"
    out.mkdir(exist_ok=True)
    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    # An earlier Image must never be mistaken for a successful new build.
    for name in ("Image", "Module.symvers", "build.config", "SHA256SUMS"):
        (dist / name).unlink(missing_ok=True)
    template = (ROOT / "config/os44.config").read_text()
    (out / ".config").write_text(template.replace("@REPO_ROOT@", str(ROOT)))
    (WORK / "btf").mkdir(exist_ok=True)
    for name in ("reindex_deduplicated.py", "btf_reader.py"):
        shutil.copy2(ROOT / "lib/btf" / name, WORK / "btf" / name)

    tools = WORK / "tools"
    clang = tools / "clang/clang-r536225"
    rust = tools / "rust/linux-x86/1.82.0.p2"
    platform = tools / "platform/linux-x86"
    kt = tools / "kernel/linux-x86"
    ct = tools / "clang_tools/linux-x86"
    paths = [Path(sys.executable).parent, clang / "bin", rust / "bin",
             tools / "platform/path/linux-x86", platform / "bin", kt / "bin",
             Path("/usr/bin"), Path("/bin")]
    env = dict(os.environ)
    env.update({
        "PATH": ":".join(map(str, paths)),
        "LIBCLANG_PATH": str(clang / "lib"),
        "LD_LIBRARY_PATH": ":".join(map(str, [kt / "lib64", clang / "lib", ct / "lib64"])),
        "RUST_LIB_SRC": str(rust / "lib/rustlib/src/rust/library"),
        "RUSTC_FORCE_RUSTC_VERSION": "rustc-android",
        "KRUSTFLAGS": (f"--remap-path-prefix={src}=/proc/self/cwd "
                       f"--remap-path-prefix={out}=/proc/self/cwd/out "
                       f"--remap-path-prefix={rust}=/proc/self/cwd/prebuilts/rust/linux-x86/1.82.0.p2"),
        "SOURCE_DATE_EPOCH": "1791086464",
        "KBUILD_BUILD_TIMESTAMP": "@1791086464",
        "KBUILD_BUILD_USER": "os44-rootfree-source-lab",
        "KBUILD_BUILD_HOST": "reproducible",
        "OS44_REPO_ROOT": str(ROOT),
        "OS44_BUILD_ROOT": str(WORK),
        "OS44_REAL_PAHOLE": str(kt / "bin/pahole"),
        "LLVM_OBJCOPY": str(clang / "bin/llvm-objcopy"),
    })
    prefix = (f"-fdebug-prefix-map={src}=. -ffile-prefix-map={src}=. "
              f"-fdebug-prefix-map={out}=/proc/self/cwd/out "
              f"-ffile-prefix-map={out}=/proc/self/cwd/out")
    command = [
        "make", "-C", str(src), f"-j{options.jobs}", "O=" + str(out),
        "ARCH=arm64", "LLVM=1", "LLVM_IAS=1", "CC=clang", "LD=ld.lld",
        "AR=llvm-ar", "NM=llvm-nm", "STRIP=llvm-strip", "OBJCOPY=llvm-objcopy",
        "OBJDUMP=llvm-objdump", "READELF=llvm-readelf", "HOSTCC=clang",
        "HOSTCXX=clang++", "HOSTLD=ld.lld", "RUSTC=rustc", "RUSTDOC=rustdoc",
        "RUSTFMT=rustfmt", "BINDGEN=" + str(ct / "bin/bindgen"),
        "PAHOLE=" + str(ROOT / "lib/btf/pahole.py"), "DEPMOD=" + str(kt / "bin/depmod"),
        "CLANG_TRIPLE=aarch64-linux-gnu-", "CROSS_COMPILE=aarch64-linux-gnu-",
        f"HOSTCFLAGS=-I{kt}/include", f"HOSTLDFLAGS=-L{kt}/lib64 -Wl,-rpath,{kt}/lib64",
        "KCFLAGS=" + prefix + " -D__ANDROID_COMMON_KERNEL__", "KAFLAGS=" + prefix,
        "KCPPFLAGS_COMPAT=" + prefix, "KBUILD_GENDWARFKSYMS_STABLE=1",
        "LOCALVERSION=", "KBUILD_BUILD_VERSION=1",
    ]
    started = time.time()
    record = {"started": started, "status": "running", "steps": [],
              "source": json.loads((ROOT / "manifest/source.json").read_text()),
              "toolchains": json.loads((ROOT / "manifest/toolchains.json").read_text()),
              "hardware_tested": False, "flash_package_generated": False}
    receipt = dist / "build.json"

    def save():
        receipt.write_text(json.dumps(record, indent=2) + "\n")

    save()
    rc = 0
    with (dist / "build.log").open("w") as log:
        for phase, targets in [("config", ["olddefconfig"]), ("kernel", ["Image", "modules"])]:
            argv = command + targets
            print(f"Building {phase}; output: {dist / 'build.log'}", flush=True)
            log.write(f"\nPHASE {phase}\n")
            log.flush()
            rc = subprocess.run(argv, env=env, stdout=log, stderr=subprocess.STDOUT).returncode
            record["steps"].append({"phase": phase, "argv": argv, "returncode": rc})
            save()
            if rc:
                break
    if not rc:
        expected = {"Image": out / "arch/arm64/boot/Image",
                    "Module.symvers": out / "Module.symvers", "build.config": out / ".config"}
        missing = [name for name, path in expected.items() if not path.is_file()]
        if missing:
            rc = 1
            record["missing_outputs"] = missing
        else:
            for name, path in expected.items():
                shutil.copy2(path, dist / name)
            record["sha256"] = {name: digest(dist / name) for name in expected}
            (dist / "SHA256SUMS").write_text("".join(
                f"{value}  {name}\n" for name, value in record["sha256"].items()))
    record.update(returncode=rc, status="failed" if rc else "built", seconds=time.time() - started)
    save()
    if rc:
        print((dist / "build.log").read_text(errors="replace")[-12000:], file=sys.stderr)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
