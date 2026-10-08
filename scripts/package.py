import argparse
import hashlib
import json
import shutil
import subprocess
import zipfile
from pathlib import Path
from common import ROOT, tool_environment

parser = argparse.ArgumentParser()
parser.add_argument('--mesa', type=Path, default=ROOT / 'mesa')
args = parser.parse_args()
env = tool_environment()
dist = ROOT / 'dist'
dist.mkdir(exist_ok=True)
for package in ('turnip-win-arm64', 'turnip-win-arm64x'):
    directory = ROOT / 'out' / package
    driver = directory / 'vulkan_freedreno.dll'
    header = subprocess.check_output(['llvm-readobj', '--file-headers', '--coff-load-config', '--coff-imports', str(driver)], text=True, env=env)
    expected = 'IMAGE_FILE_MACHINE_ARM64X' if package.endswith('arm64x') else 'IMAGE_FILE_MACHINE_ARM64 ('
    if expected not in header:
        raise RuntimeError(f'{package} does not have the expected PE machine type')
    if package.endswith('arm64x') and not all(name in header for name in ('CHPEMetadata', ' ARM64EC', ' ARM64')):
        raise RuntimeError('ARM64X image is missing hybrid metadata/code ranges')
    (directory / 'pe-info.txt').write_text(header, encoding='utf-8')
    (directory / 'turnip_icd.json').write_text(json.dumps({'file_format_version': '1.0.0',
        'ICD': {'library_path': '.\\vulkan_freedreno.dll', 'api_version': '1.3.0'}}, indent=2) + '\n', encoding='utf-8')
    (directory / 'run-with-turnip.cmd').write_text('@echo off\nsetlocal\nif "%~1"=="" exit /b 2\n'
        'set "VK_DRIVER_FILES=%~dp0turnip_icd.json"\nset "VK_ICD_FILENAMES=%VK_DRIVER_FILES%"\n%*\nexit /b %errorlevel%\n', encoding='utf-8')
    for source in (ROOT / 'out/build-info.json', ROOT / 'out/verification.json', ROOT / 'README.md', ROOT / 'LICENSE'):
        if source.exists():
            shutil.copy2(source, directory / source.name)
    shutil.copy2(args.mesa / 'docs/license.rst', directory / 'MESA-LICENSE.rst')
    shutil.copytree(ROOT / 'docs', directory / 'docs', dirs_exist_ok=True)
    files = sorted(path for path in directory.rglob('*') if path.is_file() and path.name != 'SHA256SUMS.txt' and path.suffix != '.pdb')
    (directory / 'SHA256SUMS.txt').write_text(''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(directory).as_posix()}\n' for path in files), encoding='utf-8')
    for symbols in (False, True):
        filename = package + ('-symbols' if symbols else '') + '.zip'
        with zipfile.ZipFile(dist / filename, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            if symbols:
                pdb = directory / 'vulkan_freedreno.pdb'
                archive.writestr(package + '/SHA256SUMS.txt', f'{hashlib.sha256(pdb.read_bytes()).hexdigest()}  {pdb.name}\n')
            for path in directory.rglob('*'):
                if path.is_file() and ((path.suffix == '.pdb') == symbols):
                    archive.write(path, package + '/' + path.relative_to(directory).as_posix())
        print(dist / filename)
