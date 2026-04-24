import inspect
import os
import builtins
from types import SimpleNamespace

def log(*args, **kwargs):
    caller = inspect.stack()[1]
    abspath = caller.filename
    line = caller.lineno

    try:
        from SCons.Script import Dir
        root = Dir("#").abspath
    except (ImportError, NameError):
        root = os.getcwd()

    try:
        rel_path = os.path.relpath(abspath, root)
    except ValueError:
        rel_path = abspath

    print(f"[{rel_path}:{line:<3}] ", end="")
    print(*args, **kwargs)

# Logic to auto-inject if not already there
if not hasattr(builtins, "lm"):
    lm = SimpleNamespace()
    builtins.lm = lm
lm.log = log
