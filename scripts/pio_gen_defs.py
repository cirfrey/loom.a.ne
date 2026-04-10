import datetime
import subprocess
import os
from pathlib import Path

Import("env")

builtin_print = print
def log(*args, **kwargs):
    import inspect
    cf = inspect.currentframe()
    builtin_print(f"[{inspect.stack()[1][1]}:{cf.f_back.f_lineno}] ", end="")
    builtin_print(*args, **kwargs)
print = log

try:
    git_hash = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode('ascii').strip()
except Exception:
    git_hash = "unknown"
build_date = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
major      = env.GetProjectOption("custom_loomane_version_major")
minor      = env.GetProjectOption("custom_loomane_version_minor")

board_type = "unknown_board"
mcu_type   = "unknown_mcu"
max_ram    = "0"
try:
    board_config = env.BoardConfig()
    board_type   = env.subst("$BOARD")
    mcu_type     = board_config.get("build.mcu", "unknown_mcu")
    max_ram      = board.get("upload.maximum_ram_size")
except Exception: pass

template_file = Path(env.subst("$PROJECT_DIR"))/"include"/"lm"/"build.hpp.in"
output_dir    = Path(env.subst("$BUILD_DIR"))/"generated"/"include"/"lm"
output_file   = output_dir/"build.hpp"

if not os.path.exists(output_dir): os.makedirs(output_dir)

if os.path.exists(template_file):
    with open(template_file, "r") as f:
        content = f.read()

        content = content.replace("@PROJECT_VERSION_MAJOR@", major)
        content = content.replace("@PROJECT_VERSION_MINOR@", minor)
        content = content.replace("@GIT_HASH@", git_hash)
        content = content.replace("@BUILD_DATE@", build_date)
        content = content.replace("@BOARD@", board_type)
        content = content.replace("@MCU@", mcu_type)

    with open(output_file, "w") as f:
        f.write(content)
env.Append(CPPPATH=[Path(env.subst("$BUILD_DIR"))/"generated"/"include"])
print(f"Generated {output_file}")


## Add the necessary tinyusb files for espressif backends (uses cmake instead of the build_src_filter flag in platformio.ini)
tusb_base = Path(env.subst("$PROJECT_DIR")) / "lib" / "tinyusb"
tusb_src  = tusb_base / "src"
tusb_files = []
if env.get("PIOPLATFORM") == "espressif32":
    unique_files = set()
    for pattern in ["tusb.c", "common/*.c", "host/*.c", "device/*.c"]:
        for f in tusb_src.glob(pattern):
            unique_files.add(f)
    for f in tusb_src.rglob("class/**/*.c"):
        unique_files.add(f)
    for f in tusb_src.rglob("class/**/*.c"):
        unique_files.add(f)
    #for f in tusb_src.rglob("portable/synopsys/dwc2/*.c"):
    #    unique_files.add(f)
    tusb_files = sorted([str(f).replace("\\", "/") for f in unique_files])

cmake_defs_file = Path(env.subst("$BUILD_DIR"))/"generated"/"loomane_defs.cmake"
with open(cmake_defs_file, "w") as f:
    f.write(f'set(LOOMANE_VERSION_MAJOR {major})\n')
    f.write(f'set(LOOMANE_VERSION_MINOR {minor})\n')
    f.write(f'set(LOOMANE_BUILD_DATE    "{build_date}")\n')
    f.write(f'set(LOOMANE_GIT_HASH      "{git_hash}")\n')
    f.write(f'set(LOOMANE_BOARD_TYPE    "{board_type}")\n')
    f.write(f'set(LOOMANE_MCU_TYPE      "{mcu_type}")\n')

    f.write('set(LOOMANE_TINYUSB_SOURCES\n')
    for source in tusb_files:
        f.write(f'    "{source}"\n')
    f.write(')\n')
print(f"Generated {cmake_defs_file}")
os.environ["LOOMANE_DEFS_PATH"] = str(cmake_defs_file)
print(f"Set env-var LOOMANE_DEFS_PATH={str(cmake_defs_file)}")

banner_defs_file = Path(env.subst("$BUILD_DIR"))/"generated"/"banner_defs.ini"
with open(banner_defs_file, "w") as f:
    f.write(f'version_major = {major}\n')
    f.write(f'version_minor = {minor}\n')
    f.write(f'build_date = "{build_date}"\n')
    f.write(f'git_hash = "{git_hash}"\n')
    f.write(f'board = "{board_type}"\n')
    f.write(f'arch = "{mcu_type}"\n')
    f.write(f'max_ram = {max_ram}')
print(f"Generated {banner_defs_file}")


# Generates the includable .hpp versions of the .txt files
# in lm/arch/*

def randname(len):
    import random
    return ''.join(random.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ') for i in range(len))

include_arch_folder = Path(env.subst("$PROJECT_DIR"))/"include"/"lm"/"arch"
out_arch_folder = Path(env.subst("$BUILD_DIR"))/"generated"/"include"/"lm"/"arch"
for txt in include_arch_folder.rglob("**/*.txt"):
    print(f'Generating {out_arch_folder}/{txt.parent.name}/{txt.stem}.hpp')

    out_folder = out_arch_folder / txt.parent.name
    if not os.path.exists(out_folder): os.makedirs(out_folder)
    out = (out_folder / txt.name).with_suffix('.hpp')
    with open(out, "w") as f:
        with open(txt, "r") as infile:
            delim = randname(16)
            f.write(f'static constexpr char {txt.stem}[] = R"{delim}({infile.read()}){delim}";')
