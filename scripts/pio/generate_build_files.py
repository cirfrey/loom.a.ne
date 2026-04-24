import datetime
import subprocess
import os
import re
from pathlib import Path
from typing import Dict

Import("env")

def rand_alpha(len):
    import random
    return ''.join(random.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ') for i in range(len))

def get_git_hash():
    try: return subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode('ascii').strip()
    except Exception: return "unknown"

def get_board_type():
    try: return env.subst("$BOARD")
    except Exception: return "unknown_board"

def get_mcu_type():
    try: return env.BoardConfig().get("build.mcu", "unknown_mcu")
    except Exception: return "unknown_mcu"

def get_max_ram():
    try: return str(env.BoardConfig().get("upload.maximum_ram_size"))
    except Exception: return "0"

def get_build_src_filter():
    # PlatformIO uses PROJECT_SRC_DIR (usually 'src') as the root for filters
    src_dir = Path(env.subst("$PROJECT_SRC_DIR")).resolve()

    raw_opt    = env.GetProjectOption("build_src_filter", "")
    raw_filter = ' '.join(raw_opt) if isinstance(raw_opt, list) else raw_opt

    rules = re.findall(r'([+-])<([^>]+)>', raw_filter)
    valid_exts = {'.c', '.cpp', '.cc', '.cxx', '.S', '.s'}
    included_files = set()

    for action, pattern in rules:
        pattern = pattern.strip().replace('\\', '/')

        # Strip leading slashes to prevent pathlib from treating it as an absolute path
        target = src_dir / pattern.lstrip('/')
        if target.is_dir(): matches = target.rglob('*')
        else:               matches = src_dir.glob(pattern)

        matched_files = {
            p.resolve() for p in matches
            if p.is_file() and p.suffix in valid_exts
        }

        if action == '+':   included_files.update(matched_files)
        elif action == '-': included_files.difference_update(matched_files)

    return sorted([f.as_posix() for f in included_files])


def generate(output_file: str, content: str, message = ""):
    output_dir = Path(output_file).parent
    if not os.path.exists(output_dir): os.makedirs(output_dir)
    with open(output_file, "w") as f: f.write(content)

    if message == "": message = f"Generated [{output_file}]"
    if message is not None: lm.log(message)

def copy(input_file: str, output_file: str):
    with open(input_file, "r") as f: content = f.read()
    generate(output_file, content, f"Copied [{input_file}] to [{output_file}]")

def generate_from_template(input_file: str, output_file: str, replacements: Dict):
    with open(input_file, "r") as f: content = f.read()
    for k, v in replacements.items(): content = content.replace(k, v)
    generate(output_file, content, f"Generated [{output_file}] from template [{input_file}]")

def generate_wrap(input_file: str, output_file: str, wrapper):
    with open(input_file, "r") as f: content = f.read()
    generate(output_file, wrapper(content), f"Generated [{output_file}] by wrapping [{input_file}]")

def make_path_cmake_friendly(p):
    rel_p = Path(p).resolve().relative_to(project_dir).as_posix()
    return f'"${{CMAKE_CURRENT_SOURCE_DIR}}/{rel_p}"'

project_dir   = Path(env.subst("$PROJECT_DIR"))
generated_dir = Path(env.subst("$BUILD_DIR"))/"generated"

replacements = {
    "@PROJECT_VERSION_MAJOR@": env.GetProjectConfig().get("common", "version_major"),
    "@PROJECT_VERSION_MINOR@": env.GetProjectConfig().get("common", "version_minor"),
    "@GIT_HASH@": get_git_hash(),
    "@BUILD_DATE@": datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S"),
    "@BOARD@": get_board_type(),
    "@MCU@": get_mcu_type(),
    "@MAX_RAM@": get_max_ram(),
    "@BUILD_SRC_FILTER_CMAKE@": '\n\t' + '\n\t'.join(map(make_path_cmake_friendly, get_build_src_filter())) + '\n',
}


copy(project_dir/"include/lm/board"/get_board_type()/"board.hpp", generated_dir/"include/lm/board.hpp")
copy(project_dir/"include/lm/usbd/tusb_config.h",                 generated_dir/"include/tusb_config.h")

# --- assets/templates ---
template_dir = project_dir/"assets/templates"
templates = [t for t in template_dir.rglob("**/*") if t.is_file()]
for template in templates:
    generate_from_template(
        template,
        generated_dir/(template.relative_to(template_dir)),
        replacements
    )

# --- sdkconfig.h (native) ---
if env.get("PIOPLATFORM") == "native":
    generate(generated_dir/"include/sdkconfig.h",
        "#pragma once\n"+
        "#define CFG_TUSB_MCU OPT_MCU_NONE\n" +
        "#define CFG_TUSB_OS OPT_OS_NONE\n" +
        "#define TUP_DCD_ENDPOINT_MAX 10\n"
    )

os.environ["LOOMANE_DEFS_PATH"] = str(generated_dir/"loomane_defs.cmake")
lm.log(f"Set env-var LOOMANE_DEFS_PATH={os.environ['LOOMANE_DEFS_PATH']}")

# --- lm/arch/**/*.txt -> .hpp ---
for txt in (project_dir/"include/lm/arch").rglob("**/*.txt"):
    out = (generated_dir/"include/lm/arch"/txt.parent.name/txt.name).with_suffix('.hpp')
    delim = rand_alpha(16)
    generate_wrap(txt, out, lambda content: f'static constexpr char {txt.stem}[] = R"{delim}({content}){delim}";')
