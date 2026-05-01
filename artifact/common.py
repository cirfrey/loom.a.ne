from pathlib import Path
from typing import Union
from string import Template
from urllib.request import urlretrieve
import importlib.util
import sys
import re
import inspect
import os
import subprocess
import tarfile
import zipfile

def log(*args, stackoffset=1, **kwargs):
    caller = inspect.stack()[stackoffset]
    abspath = caller.filename
    line = caller.lineno

    try:
        from SCons.Script import Dir
        root = Dir("#").abspath
    except (ImportError, NameError):
        root = mypath(abspath)

    try:
        rel_path = os.path.relpath(abspath, root)
    except ValueError:
        rel_path = abspath

    print(f"[{rel_path}:{line:<3}] ", end="")
    print(*args, **kwargs)


def source_pyfile(filepath: Union[str, Path]) -> None:
    """Execute a Python file in an isolated namespace, then discard it.

    Use this to 'source' an activation script without polluting the caller's globals.
    """
    filepath = Path(filepath)
    # Provide a minimal namespace; __file__ is given so the script can reference itself if needed
    namespace = {'__file__': str(filepath)}
    with open(filepath) as f:
        exec(compile(f.read(), str(filepath), 'exec'), namespace)
    # 'namespace' is thrown away, so any names defined by the script vanish


def import_module(name: str, path: str):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module

def render_template(template_path: Path, output_path: Path, vars: dict):
    """Render a template.

    Supports ``% if VAR:`` / ``% endif`` blocks. A block is kept if
    ``VAR`` exists in *vars* and (optionally) its value is non‑empty.
    """
    with open(template_path) as f:
        text = f.read()

    lines = text.splitlines()
    kept_lines = []
    stack = []          # stack of (is_kept: bool) for nested ifs
    current_keep = True

    for line in lines:
        stripped = line.strip()
        if stripped.startswith('% if '):
            # Extract variable name: "% if VAR:" => "VAR"
            var = stripped[4:].strip().rstrip(':')
            # Keep this block if the variable is defined and (non‑empty or just defined)
            condition = var in vars
            # Optionally require non‑empty:
            # condition = var in vars and bool(vars[var])
            stack.append(current_keep)
            current_keep = condition
            continue
        elif stripped == '% endif':
            if stack:
                current_keep = stack.pop()
            continue
        if current_keep:
            kept_lines.append(line)

    processed = Template('\n'.join(kept_lines))
    result = processed.safe_substitute(vars)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(result)
    output_path.chmod(0o755)

def mypath(caller_file: str) -> Path: return Path(caller_file).resolve().parent

def die(msg):
    log(f"ERROR: {msg}", file=sys.stderr, stackoffset=2)
    sys.exit(1)

def get_nproc():
    """Return number of parallel jobs (safe for all platforms)."""
    try:
        return os.cpu_count() or 1
    except:
        return 1

def which(cmd):
    """Return path of executable or None."""
    return shutil.which(cmd)

def extract_tar(archive, dest_dir):
    """Extract a .tar.xz or .tar.gz archive."""
    with tarfile.open(archive) as tf:
        tf.extractall(dest_dir)

def extract_zip(archive, dest_dir):
    """Extract a .zip archive."""
    with zipfile.ZipFile(archive) as zf:
        zf.extractall(dest_dir)

def download(url, dest):
    """Download a file with progress indication."""
    log(f"  Downloading {os.path.basename(dest)} …", stackoffset=2)
    urlretrieve(url, dest)

def download_and_extract(url, archive_name, dest_dir, dldir, strip_components=1):
    """Download and decompress an archive, then flatten one level if needed."""
    archive_path = dldir / archive_name
    archive_path.parent.mkdir(exist_ok=True)
    if not archive_path.exists():
        download(url, archive_path)

    # Remove previous extraction
    if dest_dir.exists():
        shutil.rmtree(dest_dir)
    dest_dir.mkdir(parents=True)

    if archive_name.endswith(".zip"):
        extract_zip(archive_path, dest_dir)
    else:
        extract_tar(archive_path, dest_dir)

    # If the archive contains a single top-level directory, strip it.
    items = list(dest_dir.iterdir())
    if strip_components and len(items) == 1 and items[0].is_dir():
        inner = items[0]
        for child in inner.iterdir():
            shutil.move(str(child), str(dest_dir / child.name))
        inner.rmdir()

def run(cmd, **kwargs):
    log('Running [' + ' '.join(cmd) + ']', stackoffset=2)
    return subprocess.run(cmd, **kwargs)

def bucket_args(args, buckets, default_bucket):
    primary_map = {}
    ordered_lists = []

    for item in buckets:
        # Convert single string to a list so the loop works
        aliases = [item] if isinstance(item, str) else item

        new_list = []
        ordered_lists.append(new_list)

        for alias in aliases:
            primary_map[alias] = new_list

    # Initialize with the list belonging to the default alias
    if default_bucket not in primary_map:
        raise ValueError(f"Default bucket '{default_bucket}' not found in defined aliases.")

    current_section_list = primary_map[default_bucket]

    for arg in args:
        if arg in primary_map:
            current_section_list = primary_map[arg]
        else:
            current_section_list.append(arg)

    return ordered_lists
