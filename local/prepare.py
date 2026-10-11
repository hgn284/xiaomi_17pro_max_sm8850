#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fetch the frozen source and pinned toolchains; never flash a device."""
from pathlib import Path
import hashlib
import json
import subprocess
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work"
WORK.mkdir(exist_ok=True)
lock = json.loads((ROOT / "manifest/source.json").read_text())

def run(argv):
    subprocess.run(list(map(str, argv)), check=True)

def sha(path):
    with path.open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()

archive = WORK / lock["archive"]
if not archive.exists():
    partial = archive.with_suffix(archive.suffix + ".part")
    urllib.request.urlretrieve(lock["url"], partial)
    if sha(partial) != lock["sha256"]:
        raise SystemExit("Downloaded source differs from the frozen manifest")
    partial.rename(archive)
if sha(archive) != lock["sha256"]:
    raise SystemExit("Source archive differs from manifest")
if not (WORK / "kernel/Makefile").exists():
    with tarfile.open(archive, "r:gz") as tf:
        tf.extractall(WORK, filter="data")

# BakaSU derives its version from real history; do not invent a version code.
root = WORK / "root_git"
if not (root / ".git").exists():
    run(["git", "-C", root, "init"])
    run(["git", "-C", root, "remote", "add", "origin", lock["bakasu_url"]])
current = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                         capture_output=True, text=True)
if current.returncode != 0 or current.stdout.strip() != lock["bakasu_commit"]:
    run(["git", "-C", root, "fetch", "--filter=blob:none", "origin", lock["bakasu_commit"]])
    run(["git", "-C", root, "reset", "--mixed", lock["bakasu_commit"]])

tools = WORK / "tools"
tools.mkdir(exist_ok=True)
for item in json.loads((ROOT / "manifest/toolchains.json").read_text()):
    target = tools / item["name"]
    if not (target / ".git").exists():
        target.mkdir(exist_ok=True)
        run(["git", "-C", target, "init"])
        run(["git", "-C", target, "remote", "add", "origin", item["url"]])
    current = subprocess.run(["git", "-C", str(target), "rev-parse", "HEAD"],
                             capture_output=True, text=True)
    if current.returncode == 0 and current.stdout.strip() == item["commit"]:
        continue
    run(["git", "-C", target, "sparse-checkout", "init", "--cone"])
    run(["git", "-C", target, "sparse-checkout", "set", *item["paths"]])
    run(["git", "-C", target, "fetch", "--depth=1", "--filter=blob:none", "origin", item["commit"]])
    run(["git", "-C", target, "checkout", "--detach", "FETCH_HEAD"])
print("Frozen source and toolchains prepared; no kernel build or hardware test performed here.")
