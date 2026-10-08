import struct
import subprocess
import shutil
import json
from pathlib import Path

from common import compdb, response_run, split_command

def weak_names(path):
    data = path.read_bytes()
    machine, = struct.unpack_from('<H', data)
    if machine == 0:
        machine, = struct.unpack_from('<H', data, 6)
        offset, count = struct.unpack_from('<II', data, 48)
        stride, storage_offset = 20, 18
    else:
        offset, count = struct.unpack_from('<II', data, 8)
        stride, storage_offset = 18, 16
    if machine != 0xA641:
        return []
    strings = offset + count * stride
    names = []
    i = 0
    while i < count:
        pos = offset + i * stride
        storage, aux = struct.unpack_from('BB', data, pos + storage_offset)
        if storage == 105 and aux:
            search, = struct.unpack_from('<I', data, pos + stride + 4)
            if search == 3:
                zero, index = struct.unpack_from('<II', data, pos)
                if zero == 0:
                    end = data.index(0, strings + index)
                    name = data[strings + index:end].decode('utf-8')
                else:
                    name = data[pos:pos + 8].split(b'\0')[0].decode('utf-8')
                if not name.startswith('#') and '$$h' not in name:
                    if name.startswith('?'):
                        if '@@Y' not in name:
                            raise ValueError(f'Unsupported function name: {name}')
                        replacement = name.replace('@@Y', '@@$$hY', 1)
                    else:
                        replacement = '#' + name
                    names.append((name, replacement))
        i += 1 + aux
    return names

def fix(path, env):
    names = weak_names(path)
    if not names:
        return False
    mapping = path.with_suffix('.ec-redefine.txt')
    cpp_names = [(a, b) for a, b in names if not b.startswith('#')]
    c_names = [(a, b) for a, b in names if b.startswith('#')]
    if cpp_names:
        mapping.write_text(''.join(f'{name} {replacement}\n' for name, replacement in cpp_names), encoding='utf-8')
        subprocess.run(['llvm-objcopy', f'--redefine-syms={mapping}', str(path)], check=True, env=env)
    for start in range(0, len(c_names), 40):
        args = [f'--redefine-sym={a}={b}' for a, b in c_names[start:start + 40]]
        subprocess.run(['llvm-objcopy', *args, str(path)], check=True, env=env)
    print(f'{path.name}: {len(names)} weak references')
    return True

def prepare_entrypoints(build, destination, env):
    replacements = {}
    for source in build.rglob('*entrypoints*.obj'):
        if not weak_names(source):
            continue
        target = destination / source.relative_to(build)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        fix(target, env)
        replacements[str(source.resolve()).lower()] = target
    data = compdb(build, 'STATIC_LINKER', 'STATIC_LINKER_RSP', env=env)
    for entry in data:
        args = split_command(entry['command'])
        changed = False
        for i, arg in enumerate(args[1:], 1):
            if arg.startswith('/'):
                continue
            target = replacements.get(str((build / arg).resolve()).lower())
            if target:
                args[i] = str(target)
                changed = True
        if not changed:
            continue
        source = build / entry['output']
        target = destination / entry['output']
        target.parent.mkdir(parents=True, exist_ok=True)
        args = [f'/OUT:{target}' if a.upper().startswith('/OUT:') else a for a in args]
        response = target.with_suffix('.link.rsp')
        response.write_text(subprocess.list2cmdline(args[1:]), encoding='utf-8')
        subprocess.run([args[0], '@' + str(response)], cwd=build, check=True, env=env)
        replacements[str(source.resolve()).lower()] = target
    return replacements

def prepare_meson():
    from pathlib import Path
    import mesonbuild
    
    path = Path(mesonbuild.__file__).parent / 'compilers/mixins/visualstudio.py'
    text = path.read_text(encoding='utf-8')
    old = "        elif 'aarch64' in target:\n"
    new = "        elif 'arm64ec' in target:\n            self.machine = 'arm64ec'\n" + old
    if "elif 'arm64ec' in target:" not in text:
        if text.count(old) != 1:
            raise SystemExit('Unrecognized Meson compiler target detection')
        path.write_text(text.replace(old, new), encoding='utf-8')
    print('Meson ARM64EC target detection ready')
