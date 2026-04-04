import datetime
import subprocess
import os
from pathlib import Path

Import("env")

this_script = "scripts/pio_gen_prebuild.py"

try:
    git_hash = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode('ascii').strip()
except Exception:
    git_hash = "unknown"
build_date = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
major      = env.GetProjectOption("custom_loomane_version_major")
minor      = env.GetProjectOption("custom_loomane_version_minor")

if env.subst("$PIOPLATFORM") == "native":
    # Fallbacks for Desktop/Native builds
    # TODO: maybe get more info about the pc?
    board_type = env.GetProjectOption("custom_board", "native")
    mcu_type   = env.GetProjectOption("custom_mcu", "native")
else:
    try:
        board_config = env.BoardConfig()
        board_type   = env.subst("$BOARD")
        mcu_type     = board_config.get("build.mcu", "unknown_mcu")
    except Exception:
        # Final safety fallback if BoardConfig fails for any other reason
        board_type = "unknown_board"
        mcu_type   = "unknown_mcu"

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
print(f"[{this_script}] --- Generated {output_file} ---")


cmake_defs_file = Path(env.subst("$BUILD_DIR"))/"generated"/"loomane_defs.cmake"
with open(cmake_defs_file, "w") as f:
    f.write(f'set(LOOMANE_VERSION_MAJOR {major})\n')
    f.write(f'set(LOOMANE_VERSION_MINOR {minor})\n')
    f.write(f'set(LOOMANE_BUILD_DATE    "{build_date}")\n')
    f.write(f'set(LOOMANE_GIT_HASH      "{git_hash}")\n')
    f.write(f'set(LOOMANE_BOARD_TYPE    "{board_type}")\n')
    f.write(f'set(LOOMANE_MCU_TYPE      "{mcu_type}")\n')
print(f"[{this_script}] --- Generated {cmake_defs_file} ---")
os.environ["LOOMANE_DEFS_PATH"] = str(cmake_defs_file)
print(f"[{this_script}] --- Set env-var LOOMANE_DEFS_PATH={str(cmake_defs_file)} ---")
