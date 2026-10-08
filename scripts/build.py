import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from common import ROOT, TARGET, compdb, response_run, run, split_command, tool_environment
from arm64ec import prepare_entrypoints, prepare_meson

OPTIONS = ['--buildtype=debugoptimized', '-Dplatforms=windows', '-Dgallium-drivers=',
    '-Dvulkan-drivers=freedreno', '-Dfreedreno-kmds=wddm', '-Dglx=disabled', '-Degl=disabled',
    '-Dgbm=disabled', '-Dgles1=disabled', '-Dgles2=disabled', '-Dopengl=false', '-Dllvm=disabled',
    '-Dshared-llvm=disabled', '-Dvideo-codecs=', '-Dbuild-tests=false', '-Dperfetto=false',
    '-Dprecomp-compiler=auto', '-Dzlib:default_library=static']

def configure_build(mesa, build, arch, env, jobs):
    native = build.parent / f'{arch}.ini'
    target = 'arm64ec' if arch == 'arm64ec' else 'aarch64'
    native.write_text(f"[binaries]\nc = ['clang-cl', '/clang:--target={target}-pc-windows-msvc']\n"
        f"cpp = ['clang-cl', '/clang:--target={target}-pc-windows-msvc']\n"
        "ar = 'llvm-lib'\nc_ld = 'lld-link'\ncpp_ld = 'lld-link'\n", encoding='utf-8')
    configure = ['meson', 'setup', build, mesa, '--native-file', native, *OPTIONS]
    if (build / 'meson-private/coredata.dat').exists():
        configure.append('--reconfigure')
    run(configure, env=env)
    run(['ninja', '-C', build, '-j', str(jobs), TARGET], env=env)
    command = next(x for x in compdb(build, 'cpp_LINKER_RSP', env=env) if x['output'] == TARGET)
    args = split_command(command['command'])
    if arch == 'arm64ec':
        replacements = prepare_entrypoints(build, build.parent / 'arm64ec-entrypoints', env)
        for i, arg in enumerate(args[1:], 1):
            prefix, path = '', arg
            if arg.upper().startswith('/WHOLEARCHIVE:'):
                prefix, path = arg.split(':', 1)
                prefix += ':'
            elif arg.startswith('/'):
                continue
            replacement = replacements.get(str((build / path).resolve()).lower())
            if replacement:
                args[i] = prefix + str(replacement)
    args = [a for a in args if a != '-Wl,-Bsymbolic']
    args.append(f'/linkreprofullpathrsp:{build.parent / (arch + "-full.rsp")}')
    response_run(args, build.parent / f'{arch}-link.rsp', cwd=build, env=env)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--mesa', type=Path, default=ROOT / 'mesa')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    mesa = args.mesa.resolve()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    revision = json.loads((ROOT / 'mesa-revision.json').read_text())
    if not (mesa / '.git').exists():
        run(['git', 'init', mesa])
        run(['git', '-C', mesa, 'remote', 'add', 'origin', revision['repository']])
        run(['git', '-C', mesa, 'fetch', '--depth=1', 'origin', revision['revision']])
        run(['git', '-C', mesa, 'checkout', '--detach', revision['revision']])
    actual = subprocess.check_output(['git', '-C', str(mesa), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != revision['revision']:
        raise RuntimeError(f'Mesa checkout is {actual}; expected {revision["revision"]}. Update the lock explicitly.')
    if subprocess.check_output(['git', '-C', str(mesa), 'status', '--porcelain']).strip():
        raise RuntimeError('Mesa checkout must be clean to produce a release package.')
    env = tool_environment()
    clang = subprocess.check_output(['clang-cl', '--version'], env=env, text=True)
    if int(re.search(r'clang version (\d+)', clang)[1]) < 22:
        raise RuntimeError('clang-cl/lld 22 or newer is required for this ARM64X build.')
    prepare_meson()
    build = ROOT / 'build'
    build.mkdir(exist_ok=True)
    for arch in ('arm64', 'arm64ec'):
        configure_build(mesa, build / arch, arch, env, args.jobs)
    out = ROOT / 'out'
    arm64 = out / 'turnip-win-arm64'
    arm64x = out / 'turnip-win-arm64x'
    arm64.mkdir(parents=True, exist_ok=True)
    arm64x.mkdir(parents=True, exist_ok=True)
    for suffix in ('.dll', '.pdb'):
        shutil.copy2((build / 'arm64' / TARGET).with_suffix(suffix), arm64 / ('vulkan_freedreno' + suffix))
    link = ['lld-link', '/nologo', '/machine:arm64x', '/dll', '/debug', '/incremental:no',
        '/dynamicbase', '/nxcompat', '/subsystem:console', f'/out:{arm64x / "vulkan_freedreno.dll"}',
        f'/pdb:{arm64x / "vulkan_freedreno.pdb"}', f'/implib:{build / "vulkan_freedreno.lib"}',
        f'/def:{build / "arm64ec/src/vulkan/vulkan_api.def"}',
        f'/defArm64Native:{build / "arm64/src/vulkan/vulkan_api.def"}',
        f'@{build / "arm64ec-full.rsp"}', f'@{build / "arm64-full.rsp"}']
    response_run(link, build / 'arm64x-link.rsp', cwd=ROOT, env=env)
    metadata = {'mesa_repository': revision['repository'], 'mesa_revision': actual,
        'upstream_base': revision['upstream_base'], 'clang': clang, 'build_options': OPTIONS}
    (out / 'build-info.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    run([sys.executable, ROOT / 'scripts/package.py', '--mesa', mesa], env=env)

if __name__ == '__main__':
    main()
