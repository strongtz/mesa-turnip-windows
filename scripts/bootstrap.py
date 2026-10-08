import argparse
import hashlib
import json
import os
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path
from common import ROOT, run, tool_environment

parser = argparse.ArgumentParser()
parser.add_argument('--proxy', help='Optional download/Git proxy; never stored in the repository')
parser.add_argument('--winflexbison', type=Path, help='Reuse a directory containing win_flex.exe and win_bison.exe')
parser.add_argument('--glslang', type=Path, help='Reuse a directory containing glslangValidator.exe')
args = parser.parse_args()
if args.proxy:
    os.environ['HTTP_PROXY'] = os.environ['HTTPS_PROXY'] = args.proxy
python = ROOT / '.venv/Scripts/python.exe'
if not python.exists():
    run([sys.executable, '-m', 'venv', ROOT / '.venv'])
run([python, '-m', 'pip', 'install', '-r', ROOT / 'requirements.txt'])
deps = ROOT / 'deps'
deps.mkdir(exist_ok=True)
flex = args.winflexbison.resolve() if args.winflexbison else deps / 'winflexbison'
if not (flex / 'win_flex.exe').exists():
    url = 'https://github.com/lexxmark/winflexbison/releases/download/v2.5.25/win_flex_bison-2.5.25.zip'
    archive = deps / 'win_flex_bison-2.5.25.zip'
    print('Downloading ' + url, flush=True)
    urllib.request.urlretrieve(url, archive)
    expected = '8d324b62be33604b2c45ad1dd34ab93d722534448f55a16ca7292de32b6ac135'
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise RuntimeError('winflexbison checksum mismatch')
    with zipfile.ZipFile(archive) as package:
        package.extractall(flex)
glslang = args.glslang.resolve() if args.glslang else deps / 'glslang-build/StandAlone'
if not (glslang / 'glslangValidator.exe').exists():
    source = deps / 'glslang'
    revision = '23baa3f7b58f0dfdccec6755c53e85c31e52efba'
    if not (source / '.git').exists():
        run(['git', 'init', source])
        run(['git', '-C', source, 'remote', 'add', 'origin', 'https://github.com/KhronosGroup/glslang.git'])
    run(['git', '-C', source, 'fetch', '--depth=1', 'origin', revision])
    run(['git', '-C', source, 'checkout', '--detach', revision])
    env = tool_environment()
    build = deps / 'glslang-build'
    run(['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
         '-DCMAKE_C_COMPILER=cl', '-DCMAKE_CXX_COMPILER=cl', '-DENABLE_OPT=OFF',
         '-DBUILD_TESTING=OFF', '-DENABLE_GLSLANG_BINARIES=ON'], env=env)
    run(['cmake', '--build', build, '--parallel', '4'], env=env)
for path in (flex / 'win_flex.exe', flex / 'win_bison.exe', glslang / 'glslangValidator.exe'):
    if not path.exists():
        raise FileNotFoundError(path)
(ROOT / 'toolchain.json').write_text(json.dumps({'winflexbison': str(flex), 'glslang': str(glslang)}, indent=2), encoding='utf-8')
print('Bootstrap complete. Run .venv/Scripts/python.exe scripts/build.py')
