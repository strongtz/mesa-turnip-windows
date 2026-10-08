import ctypes
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET = 'src/freedreno/vulkan/vulkan_freedreno.dll'
_BASE_ENV = None

def run(args, **kwargs):
    print('+ ' + subprocess.list2cmdline([str(a) for a in args]), flush=True)
    return subprocess.run([str(a) for a in args], check=True, **kwargs)

def require_arm64_host():
    if os.name != 'nt':
        raise RuntimeError('Use Windows ARM64 to build this driver.')
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.IsWow64Process2.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ushort), ctypes.POINTER(ctypes.c_ushort)]
    kernel.IsWow64Process2.restype = ctypes.c_int
    process_machine, native_machine = ctypes.c_ushort(), ctypes.c_ushort()
    if not kernel.IsWow64Process2(kernel.GetCurrentProcess(), ctypes.byref(process_machine), ctypes.byref(native_machine)):
        raise ctypes.WinError(ctypes.get_last_error())
    if native_machine.value != 0xAA64:
        raise RuntimeError('Use Windows ARM64; x64-host cross compilation is not supported by these scripts.')

def tool_environment(target='arm64'):
    global _BASE_ENV
    require_arm64_host()
    if _BASE_ENV is None:
        _BASE_ENV = {key.upper(): value for key, value in os.environ.items()}
    env = _BASE_ENV.copy()
    vswhere = Path(env.get('PROGRAMFILES(X86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    installation = subprocess.check_output([str(vswhere), '-latest', '-products', '*',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.ARM64', '-property', 'installationPath'], text=True).strip()
    if not installation:
        raise RuntimeError('Install Visual Studio with the ARM64 C++ toolchain.')
    vs = Path(installation)
    batch = vs / 'VC/Auxiliary/Build' / ('vcvarsarm64_amd64.bat' if target == 'x64' else 'vcvarsarm64.bat')
    with tempfile.TemporaryDirectory(prefix='turnip-env-') as temporary:
        script = Path(temporary) / 'environment.cmd'
        script.write_text(f'@echo off\ncall "{batch}" >nul\nif errorlevel 1 exit /b 1\nset\n', encoding='utf-8')
        output = subprocess.check_output(['cmd.exe', '/d', '/c', str(script)], env=env, text=True)
    for line in output.splitlines():
        key, separator, value = line.partition('=')
        if separator and key:
            env[key.upper()] = value
    llvm_candidates = [vs / 'VC/Tools/Llvm/ARM64/bin', Path(env.get('PROGRAMFILES', 'C:/Program Files')) / 'LLVM/bin']
    llvm = next((path for path in llvm_candidates if (path / 'clang-cl.exe').exists()), None)
    if llvm is None:
        raise RuntimeError('Install clang-cl/lld 22 or newer, including LLVM utilities.')
    extra = [ROOT / '.venv/Scripts', llvm,
        vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja',
        vs / 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin']
    state_path = ROOT / 'toolchain.json'
    if state_path.exists():
        state = json.loads(state_path.read_text(encoding='utf-8'))
        extra.extend(Path(state[key]) for key in ('winflexbison', 'glslang'))
    env['PATH'] = os.pathsep.join(map(str, extra)) + os.pathsep + env['PATH']
    env['PYTHONUTF8'] = '1'
    for name in ('CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS'):
        env.pop(name, None)
    for name in ('clang-cl', 'lld-link', 'llvm-lib', 'llvm-objcopy', 'llvm-readobj', 'ninja', 'cmake'):
        if not shutil.which(name, path=env['PATH']):
            raise RuntimeError(f'Missing build tool: {name}')
    os.environ.update(env)
    return env

def split_command(command):
    count = ctypes.c_int()
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
    pointer = parse(command, ctypes.byref(count))
    if not pointer:
        raise ctypes.WinError()
    try:
        return [pointer[i] for i in range(count.value)]
    finally:
        free = ctypes.windll.kernel32.LocalFree
        free.argtypes = [ctypes.c_void_p]
        free.restype = ctypes.c_void_p
        free(pointer)

def compdb(directory, *rules, env):
    return json.loads(subprocess.check_output(['ninja', '-C', str(directory), '-t', 'compdb', '-x', *rules], env=env, text=True))

def response_run(args, response, *, cwd, env):
    response.parent.mkdir(parents=True, exist_ok=True)
    response.write_text(subprocess.list2cmdline([str(a) for a in args[1:]]), encoding='utf-8')
    run([args[0], '@' + str(response)], cwd=cwd, env=env)
