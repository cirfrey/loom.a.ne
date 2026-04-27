import sys
Import("env")

# Add ws2_32 for true Windows builds (MSVC or MinGW).
# Cygwin uses its own POSIX socket layer and must NOT link ws2_32.
def is_cygwin_build(env):
    if env.get("PLATFORM") == "cygwin": return True

    cc = env.subst("$CC")
    if cc == "cc": cc = "gcc" # Assume gcc if it's the generic 'cc'

    import subprocess
    try:
        # Check the help or version output for the word 'cygwin'
        output = subprocess.check_output([cc, "-v"], stderr=subprocess.STDOUT).decode().lower()
        return "cygwin" in output
    except:
        return False

is_cygwin = is_cygwin_build(env)
is_windows = sys.platform.startswith("win") or sys.platform == "msys"

if is_windows and not is_cygwin:
    env.Append(LINKFLAGS=["-lws2_32"])
    lm.log("Added -lws2_32 for Windows native build")
elif is_cygwin:
    lm.log("Cygwin build detected — skipping ws2_32 (uses POSIX sockets)")
