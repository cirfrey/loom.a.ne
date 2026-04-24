import sys
Import("env")

builtin_print = print
def log(*args, **kwargs):
    import inspect
    cf = inspect.currentframe()
    builtin_print(f"[{inspect.stack()[1][1]}:{cf.f_back.f_lineno}] ", end="")
    builtin_print(*args, **kwargs)
print = log

# Add ws2_32 for true Windows builds (MSVC or MinGW).
# Cygwin uses its own POSIX socket layer and must NOT link ws2_32.
# We detect Cygwin by checking whether the compiler path contains "cygwin".
def is_cygwin_build(env):
    cc = env.get("CC", "")
    cxx = env.get("CXX", "")
    return "cygwin" in cc.lower() or "cygwin" in cxx.lower()

if sys.platform.startswith("win") and not is_cygwin_build(env):
    env.Append(LINKFLAGS=["-lws2_32"])
    print("Linker: Added -lws2_32 for Windows native build (non-Cygwin)")
elif is_cygwin_build(env):
    print("Linker: Cygwin build detected — skipping ws2_32 (uses POSIX sockets)")
