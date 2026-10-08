import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path
from common import ROOT

parser = argparse.ArgumentParser()
parser.add_argument('--gpu', action='store_true', help='Run hardware checks on a supported Snapdragon host')
parser.add_argument('--wsi', action='store_true', help='Also check displayed pixels; requires an unlocked desktop')
args = parser.parse_args()
if args.wsi and not args.gpu:
    parser.error('--wsi requires --gpu')
logs = ROOT / 'logs'
logs.mkdir(exist_ok=True)
results = []
for package in ('turnip-win-arm64', 'turnip-win-arm64x'):
    directory = ROOT / 'out' / package
    driver = directory / 'vulkan_freedreno.dll'
    env = os.environ.copy()
    env['VK_DRIVER_FILES'] = str(directory / 'turnip_icd.json')
    env['VK_ICD_FILENAMES'] = env['VK_DRIVER_FILES']
    env.pop('VK_LOADER_DEBUG', None)
    arches = ['arm64'] + (['x64'] if package.endswith('arm64x') else [])
    for arch in arches:
        cases = [('load_probe', [str(driver)])]
        if args.gpu:
            cases += [('icd_smoke', ['vulkan-1.dll']), ('sync_smoke', []),
                      ('bda_smoke', ['vulkan-1.dll']), ('copy_smoke', ['vulkan-1.dll']),
                      ('copy_state', ['vulkan-1.dll']), ('typed_smoke', ['vulkan-1.dll'])]
        if args.wsi:
            cases.append(('wsi_smoke', []))
        for name, options in cases:
            log = logs / f'{package}-{arch}-{name}.txt'
            with log.open('w', encoding='utf-8') as stream:
                result = subprocess.run([str(directory / 'tests' / arch / f'{name}.exe'), *options],
                    env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=120)
            passed = result.returncode == 0 and 'PASS' in log.read_text(encoding='utf-8')
            results.append({'package': package, 'architecture': arch, 'test': name,
                'passed': passed, 'exit_code': result.returncode, 'driver_sha256': hashlib.sha256(driver.read_bytes()).hexdigest(),
                'log': log.name})
            print(f'{package} {arch} {name}: {"PASS" if passed else "FAIL"}', flush=True)
(ROOT / 'out/verification.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
raise SystemExit(0 if all(result['passed'] for result in results) else 1)
