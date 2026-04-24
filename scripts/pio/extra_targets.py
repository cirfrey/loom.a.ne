# TODO: refactor and reorganize this whole file.
# TODO: Label the partitions according to the name of the folder.

Import("env")
import os
import csv
import shutil
from pathlib import Path

builtin_print = print
def log(*args, **kwargs):
    import inspect
    cf = inspect.currentframe()
    builtin_print(f"[{inspect.stack()[1][1]}:{cf.f_back.f_lineno}] ", end="")
    builtin_print(*args, **kwargs)
print = log

# 1. Define paths
project_dir = env.subst("$PROJECT_DIR")
assets_dir = os.path.join(project_dir, "assets")
partitions_csv = env.GetProjectOption("board_build.partitions", default=None)
build_dir = env.subst("$BUILD_DIR")

def generate_banner_action(*args, **kwargs):
    print("--- Executing generate_banner Target ---")
    generated_dir = Path(build_dir)/"generated"

    banner_out_txt = generated_dir / "banner.txt"
    banner_defs_ini = generated_dir / "banner_defs.ini"

    # TODO: get uuid from esptool if environtment is esp32.

    arch = "unknown_mcu"
    try: arch = env.BoardConfig().get("build.mcu", "unknown_mcu")
    except: pass
    archdir = Path(project_dir)/"include"/"lm"/"arch"/arch
    name_txt = archdir/"name.txt"
    banner_txt = archdir/"banner.txt"

    env.Execute(
        'pio run -e banner -t exec ' +
        f'--program-arg "{banner_out_txt}" ' +
        f'--program-arg "{banner_defs_ini}" ' +
        f'--program-arg "{name_txt}" ' +
        f'--program-arg "{banner_txt}" '
    )
generate_banner_target = env.AddCustomTarget(
    name="generate_banner",
    dependencies=None,
    actions=[generate_banner_action],
    title="Generate build Info banner",
    description="Compiles the native generator and runs it to create generated/banner.txt"
)


# 2. Grab tools from the PlatformIO environment
mkfatfs_tool = env.subst("$PROJECT_PACKAGES_DIR/tool-mkfatfs/mkfatfs")
if env.subst("$PLATFORM") == "win32" or os.name == "nt":
    mkfatfs_tool += ".exe"

esptool_py = env.subst("$PROJECT_PACKAGES_DIR/tool-esptoolpy/esptool.py")
python_exe = env.subst("$PYTHONEXE")

def parse_size(size_str):
    """Converts ESP-IDF size formats (1M, 4K, 0x1000) to raw bytes."""
    s = size_str.lower().strip()
    if s.endswith('m'): return int(s[:-1]) * 1024 * 1024
    if s.endswith('k'): return int(s[:-1]) * 1024
    if s.startswith('0x'): return int(s, 16)
    return int(s)

def get_partition_data(target_label):
    print(f'Reading {target_label} from {partitions_csv}')
    if not os.path.exists(partitions_csv):
        print(f"Error: Partition CSV not found at {partitions_csv}")
        return None, None

    print(f'Reading {partitions_csv}')

    with open(partitions_csv, 'r') as f:
        # Use a more flexible way to read the file
        lines = f.readlines()
        for line in lines:
            line = line.strip()
            # Skip comments or empty lines
            if not line or line.startswith('#'):
                continue

            # Split by comma and strip each part
            parts = [p.strip() for p in line.split(',')]

            # Check if this row has enough columns
            if len(parts) < 5:
                continue

            label = parts[0]

            if label == target_label:
                offset = parts[3]
                size_str = parts[4]
                return offset, parse_size(size_str)

    return None, None


# def build_and_upload_assets(*args, **kwargs):
#     if not os.path.exists(assets_dir):
#         print(f"No assets directory found at {assets_dir}. Create it and add your folders!")
#         return

#     # Grab the upload port from platformio.ini if defined, otherwise let esptool auto-detect
#     upload_port = env.get("UPLOAD_PORT", "")
#     port_arg = f"--port {upload_port}" if upload_port else ""

#     # Iterate over every folder inside assets/
#     for folder_name in os.listdir(assets_dir):
#         folder_path = os.path.join(assets_dir, folder_name)

#         # We only care about directories
#         if not os.path.isdir(folder_path):
#             continue

#         print(f"\n--- Processing '{folder_name}' ---")

#         offset, size_bytes = get_partition_data(folder_name)
#         # Only skip if size_bytes is None. If offset is empty, we'll warn later.
#         if size_bytes is None:
#             print(f"!! Result: '{folder_name}' has no size defined. Skipping.")
#             continue

#         # If offset is empty in CSV, we can't flash!
#         if not offset:
#             print(f"!! Error: Partition '{folder_name}' has an empty OFFSET in CSV.")
#             print(f"   Please put the explicit hex address (e.g. 0x310000) in the CSV.")
#             continue

#         bin_file = os.path.join(build_dir, f"{folder_name}_image.bin")

#         # Step 1: Generate FAT image
#         build_cmd = f'"{mkfatfs_tool}" -c "{folder_path}" -s {size_bytes} "{bin_file}"'
#         print(f"Building FAT image...")
#         if env.Execute(build_cmd) != 0:
#             print(f"Failed to build image for {folder_name}!")
#             continue

#         # Step 2: Upload via esptool
#         chip_type = env.get("BOARD_MCU", "esp32s2")
#         upload_cmd = f'"{python_exe}" "{esptool_py}" --chip {chip_type} {port_arg} write_flash {offset} "{bin_file}"'
#         print(f"Uploading to offset {offset}...")
#         if env.Execute(upload_cmd) != 0:
#             print(f"Failed to upload {folder_name}!")
#             continue

#         print(f"Success! Flashed '{folder_name}' to {offset}.")

def build_and_upload_assets(*args, **kwargs):
    if not os.path.exists(assets_dir):
        print(f"No assets directory found at {assets_dir}!")
        return

    upload_port = env.get("UPLOAD_PORT", "")
    port_arg = f"--port {upload_port}" if upload_port else ""

    # Iterate over every folder inside assets/ (e.g., 'static')
    for folder_name in os.listdir(assets_dir):
        static_src_path = os.path.join(assets_dir, folder_name)

        if not os.path.isdir(static_src_path):
            continue

        # Define a staging directory inside the build folder to avoid tree contamination
        staging_path = os.path.join(build_dir, f"staging_{folder_name}")

        # Clean and recreate staging area
        if os.path.exists(staging_path):
            shutil.rmtree(staging_path)
        os.makedirs(staging_path)

        print(f"\n--- Staging & Merging '{folder_name}' ---")

        # Step A: Copy static assets to staging
        for item in os.listdir(static_src_path):
            s = os.path.join(static_src_path, item)
            d = os.path.join(staging_path, item)
            if os.path.isdir(s):
                shutil.copytree(s, d)
            else:
                shutil.copy2(s, d)

        # Step B: If this is the 'static' partition, merge the generated info.txt
        if folder_name == "static":
            generated_info = os.path.join(project_dir, ".pio", "build", "native", "info.txt")
            if os.path.exists(generated_info):
                print(f"Merging generated artifact: info.txt")
                shutil.copy2(generated_info, os.path.join(staging_path, "info.txt"))
            else:
                print("!! Warning: Native info.txt not found. Skipping merge.")

        # --- Standard Partition/Flashing Logic ---
        offset, size_bytes = get_partition_data(folder_name)
        if size_bytes is None:
            print(f"!! Result: '{folder_name}' has no size. Skipping.")
            continue

        if not offset:
            print(f"!! Error: Partition '{folder_name}' has an empty OFFSET.")
            continue

        bin_file = os.path.join(build_dir, f"{folder_name}_image.bin")

        # Build FAT image from STAGING_PATH (not folder_path)
        build_cmd = f'"{mkfatfs_tool}" -c "{staging_path}" -s {size_bytes} "{bin_file}"'
        print(f"Building FAT image from merged staging...")
        if env.Execute(build_cmd) != 0:
            continue

        # Upload via esptool
        chip_type = env.get("BOARD_MCU", "esp32s2")
        upload_cmd = f'"{python_exe}" "{esptool_py}" --chip {chip_type} {port_arg} write_flash {offset} "{bin_file}"'
        print(f"Uploading to offset {offset}...")
        if env.Execute(upload_cmd) != 0:
            continue

        print(f"Success! Flashed merged '{folder_name}' to {offset}.")

# Register the custom target in PlatformIO
env.AddCustomTarget(
    name="upload_assets",
    dependencies=[generate_banner_target],
    actions=[build_and_upload_assets],
    title="Upload Asset Folders",
    description="Builds and flashes FAT images from assets/ to their matching partitions"
)
