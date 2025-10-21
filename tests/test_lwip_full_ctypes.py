import os
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
CWRAP = os.path.join(ROOT, 'cwrap')
# The build produces 'lwip_extended.dll'
DLL = os.path.join(CWRAP, 'lwip_extended.dll')


def build_lwip():
    script = os.path.join(CWRAP, 'build_lwip.ps1')
    # Run PowerShell script to build
    subprocess.check_call(['powershell', '-ExecutionPolicy', 'Bypass', '-File', script], cwd=CWRAP)


def test_build_and_load_lwip():
    build_lwip()
    assert os.path.exists(DLL), f"Built DLL not found at {DLL}"
    runner = os.path.join(os.path.dirname(__file__), '_isolated_dll_runner.py')
    subprocess.check_call([sys.executable, runner, '--dll', DLL, '--mode', 'full'])
