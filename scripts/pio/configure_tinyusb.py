# - Makes tinyusb buildable without having to manually add its includes
# in its library.json (needed since they use relative includes and
# platformio doesn't like that for stuff under /lib).
# - Also passes our "tusb_config.h" to tinyusb.
Import("env")
from pathlib import Path


PROCESSED_NODES = set()
TINYUSBSRCDIR = 'thirdparty/tinyusb/src'
def replace_flags_for_tinyusb_files(node):
    node_path = node.get_abspath() if hasattr(node, "get_abspath") else str(node)
    if node_path in PROCESSED_NODES:
        return node

    base_src = Path(TINYUSBSRCDIR).resolve()
    include_dirs = [
        Path(env.subst("$PROJECT_DIR"))/"include",
        Path(env.subst("$BUILD_DIR"))/"generated/include",
        base_src,
        base_src/"common",
        base_src/"device",
        base_src/"class/audio",
        base_src/"class/cdc",
        base_src/"class/hid",
        base_src/"class/midi",
        base_src/"class/msc",
        base_src/"class/net",
    ]

    new_env = env.Clone()
    # Don't leak any of *our* flags into tinyusb, for correctness-sake.
    new_env.Replace(CPPDEFINES=[])
    new_env.Replace(CPPPATH=[])
    flags_to_strip = {"-Wall", "-Werror", "-Wextra", "-Wpedantic", "-Weverything", f"-isystem{TINYUSBSRCDIR}"}
    for flag_var in ["CCFLAGS", "CFLAGS", "CXXFLAGS"]:
        if flag_var in new_env:
            cleaned_flags = [
                f for f in env.Flatten(new_env[flag_var])
                if str(f) not in flags_to_strip and not str(f).startswith("-Werror=")
            ]
            new_env.Replace(**{flag_var: cleaned_flags})
    # Convert Path objects to strings for SCons
    new_env.PrependUnique(CPPPATH=[str(p) for p in include_dirs])
    new_env.PrependUnique(CFLAGS=['-w' if 'cl' not in env['CXX'] else '/W0'])
    new_node = new_env.Object(node)

    # Track the resulting nodes so we don't process them again
    # new_env.Object() usually returns a list
    for n in new_node: PROCESSED_NODES.add(n.get_abspath())

    return new_node

env.AddBuildMiddleware(replace_flags_for_tinyusb_files, f"**/{TINYUSBSRCDIR}/**")
