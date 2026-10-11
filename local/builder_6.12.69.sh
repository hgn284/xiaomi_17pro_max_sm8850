#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT"
python3 -m venv .venv
.venv/bin/python -m pip install 'pyelftools==0.32'
.venv/bin/python local/prepare.py
exec .venv/bin/python local/build.py --jobs "${JOBS:-2}"
