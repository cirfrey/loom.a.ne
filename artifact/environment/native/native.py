from pathlib import Path
from artifact.common import *
import platform
import shutil
from packaging.version import parse

system = platform.system().lower()  # "linux", "darwin", "windows"
machine = platform.machine().lower()  # "x86_64" or "aarch64" etc.

def ensure_toolchain(prefix: Path, dldir: Path, environment: str, args):
    # TODO: implement correctly (check compiler version, build cmake and clang if necessary).
    return

    toolchain_dir = prefix/'toolchain'/environment
    if toolchain_dir.exists():
        source_pyfile(toolchain_dir/'activate.py')
        return

    try:
        toolchain_dir.mkdir(parents=True)
        ensure_cmake(toolchain_dir, dldir)
        ensure_llvm(toolchain_dir, dldir)
    except Exception as e:
        shutil.rmtree(toolchain_dir)
        log(f"Failed setting up toolchain [{toolchain_dir}]: {e}")
        raise e
    source_pyfile(toolchain_dir/'activate.py')


LLVM_VERSION  = "18.1.8"
CMAKE_VERSION = "3.30.2"

# ----------------------------------------------------------------------
# Optional: prebuilt Clang/LLVM
# ----------------------------------------------------------------------
def ensure_llvm(prefix: Path, dldir: Path):
    """Install prebuilt Clang/LLVM from GitHub releases."""

    log("📦 Installing LLVM/Clang …")

    base_url = f"https://github.com/llvm/llvm-project/releases/download/llvmorg-{LLVM_VERSION}"
    archive_name = f"clang+llvm-{LLVM_VERSION}-{suffix}"
    url = f"{base_url}/{archive_name}"

    dest_dir = Path("build") / "llvm"
    download_and_extract(url, archive_name, dest_dir, dldir, strip_components=1)

    # Copy the whole directory tree into the prefix
    for item in dest_dir.iterdir():
        dst = prefix / item.name
        if dst.exists():
            if dst.is_dir():
                shutil.rmtree(dst)
            else:
                dst.unlink()
        shutil.move(str(item), str(dst))
    log(f"✓ LLVM installed at {prefix}")

# ----------------------------------------------------------------------
# Optional: prebuilt CMake
# ----------------------------------------------------------------------
def ensure_cmake(prefix: Path, dldir: Path):
    """Install prebuilt CMake."""

    ret = run(['cmake', '--version'], capture_output=True, text=True)
    if ret.returncode == 0:
        cmake_version = ret.stdout.split('\n')[0].split(' ')[-1].split('-')[0]
        if parse(cmake_version) > parse(CMAKE_VERSION): return

    # Unimplemented!
    raise Exception

    # if ret.returncode != 0 or ret.stdout().strip
    cmake_exe = prefix / "bin" / ("cmake" + (".exe" if system == "windows" else ""))
    if not force and cmake_exe.exists():
        log(f"✓ CMake found at {cmake_exe}")
        return

    log("📦 Installing CMake …")
    if system == "linux":
        if machine == "x86_64":
            archive_name = "cmake-{}-linux-x86_64.tar.gz".format(CMAKE_VERSION)
        elif machine == "aarch64":
            archive_name = "cmake-{}-linux-aarch64.tar.gz".format(CMAKE_VERSION)
        else:
            die(f"Unsupported Linux arch for CMake: {machine}")
    elif system == "darwin":
        # Universal binary for macOS
        archive_name = "cmake-{}-macos-universal.tar.gz".format(CMAKE_VERSION)
    elif system == "windows":
        if machine == "x86_64":
            archive_name = "cmake-{}-windows-x86_64.zip".format(CMAKE_VERSION)
        else:
            die(f"Unsupported Windows arch for CMake: {machine}")
    else:
        die(f"Unsupported OS for CMake: {system}")

    base_url = f"https://github.com/Kitware/CMake/releases/download/v{CMAKE_VERSION}"
    url = f"{base_url}/{archive_name}"

    dest_dir = Path("build") / "cmake"
    download_and_extract(url, archive_name, dest_dir, dldir, strip_components=1)

    # Move everything into prefix, merging bin/ share/ etc.
    for item in dest_dir.iterdir():
        dst = prefix / item.name
        if dst.exists():
            if dst.is_dir():
                # merge directories: copy contents
                for sub in item.iterdir():
                    shutil.move(str(sub), str(dst / sub.name))
                item.rmdir()
            else:
                dst.unlink()
                shutil.move(str(item), str(dst))
        else:
            shutil.move(str(item), str(dst))
    log(f"✓ CMake installed at {prefix}")
