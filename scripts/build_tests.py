import argparse
import shutil
from pathlib import Path
from common import ROOT, run, tool_environment

parser = argparse.ArgumentParser()
parser.add_argument('--mesa', type=Path, default=ROOT / 'mesa')
args = parser.parse_args()
build = ROOT / 'build'
build.mkdir(exist_ok=True)
env = tool_environment()
for name, version in [('compute', 'vulkan1.0'), ('typed', 'vulkan1.0'), ('copy_state', 'vulkan1.0'), ('bda_state', 'vulkan1.1')]:
    run(['glslangValidator', '-V', '--target-env', version, ROOT / 'tests' / f'{name}.comp',
         '--vn', f'{name}_spv', '-o', build / f'{name}_spv.h'], env=env)
names = ['load_probe', 'icd_smoke', 'sync_smoke', 'bda_smoke', 'wsi_smoke',
         'typed_smoke', 'copy_smoke', 'copy_state', 'memory_smoke']
for arch in ('arm64', 'x64'):
    env = tool_environment(arch)
    directory = build / 'tests' / arch
    directory.mkdir(parents=True, exist_ok=True)
    for name in names:
        run(['cl', '/nologo', '/MD', '/EHsc', '/O2', '/I' + str(args.mesa.resolve() / 'include'),
             ROOT / 'tests' / f'{name}.cpp', '/Fe:' + str(directory / f'{name}.exe'),
             '/Fo:' + str(directory / f'{name}.obj')], env=env)
    packages = ['turnip-win-arm64x'] + (['turnip-win-arm64'] if arch == 'arm64' else [])
    for package in packages:
        output = ROOT / 'out' / package / 'tests' / arch
        output.mkdir(parents=True, exist_ok=True)
        for name in names:
            shutil.copy2(directory / f'{name}.exe', output / f'{name}.exe')
