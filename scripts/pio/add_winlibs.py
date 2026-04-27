import sys
Import("env")

# Add ws2_32 for true Windows builds (MSVC or MinGW).
# Cygwin uses its own POSIX socket layer and must NOT link ws2_32.
# We detect Cygwin by checking whether the compiler path contains "cygwin".
def is_cygwin_build(env):
    cc = env.get("CC", "")
    cxx = env.get("CXX", "")
    return "cygwin" in cc.lower() or "cygwin" in cxx.lower()

if sys.platform.startswith("win") and not is_cygwin_build(env):
    env.Append(LINKFLAGS=["-lws2_32"])
    lm.log("Linker: Added -lws2_32 for Windows native build (non-Cygwin)")
elif is_cygwin_build(env):
    lm.log("Linker: Cygwin build detected — skipping ws2_32 (uses POSIX sockets)")
