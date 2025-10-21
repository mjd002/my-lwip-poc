import os
import subprocess
import sys


HERE = os.path.dirname(__file__)
DLL_PATH = os.path.join(HERE, '..', 'cwrap', 'lwip_wrapper.dll')
DLL_PATH = os.path.abspath(DLL_PATH)
RUNNER = os.path.join(os.path.dirname(__file__), '_isolated_dll_runner.py')


def test_lwip_get_version_and_add():
    assert os.path.exists(DLL_PATH), f"DLL not found at {DLL_PATH}. Build it first."
    # run the isolated runner which loads the DLL and exercises simple wrappers
    subprocess.check_call([sys.executable, RUNNER, '--dll', DLL_PATH, '--mode', 'basic'])
