from __future__ import annotations
import importlib.util
import os
import shutil
import subprocess
import threading
import queue
import sys
from pathlib import Path
from typing import Union

from artifact.util.log import log

import time
import subprocess
import threading
import queue
import shlex
from contextlib import nullcontext

def run(command, preview=True, _stackoffset=0, **kwargs):
    if kwargs.get("shell") and isinstance(command, list):
        command = " ".join(shlex.quote(arg) for arg in command)

    log(f"Running [{str(command)}] with {dict(kwargs.items())}", _stackoffset=2+_stackoffset)

    # 1. Resolve Configuration with FPS
    cfg = {
        'enabled': True,
        'show_after': lambda ret: ret.returncode != 0,
        'max_lines': 10,
        'fps': 60,
        'border_style': 'cyan',
        'border_style_success': 'green',
        'border_style_error': 'red',
    }

    if preview in (False, None):
        cfg['enabled'], cfg['show_after'] = False, False
    elif isinstance(preview, dict):
        cfg.update(preview)

    # Calculate sleep interval: $1 / fps$
    # We cap FPS at 60 to prevent terminal flickering/insane CPU usage
    refresh_interval = 1.0 / min(cfg['fps'], 60)

    use_rich = cfg['enabled'] or cfg['show_after']
    if use_rich:
        try:
            from rich.live import Live
            from rich.panel import Panel
            from rich.console import Console
            console = Console()
        except ImportError:
            cfg['enabled'] = False
            cfg['show_after'] = False

    # 2. Process Setup
    kwargs.update({
        'stdout': subprocess.PIPE,
        'stderr': subprocess.PIPE,
        'text': True,
        'bufsize': 1,
        'universal_newlines': True
    })

    # Use env_add= instead of env= so you dont flood the terminal with stuff from yous os.environ.
    if 'env_add' in kwargs:
        kwargs['env'] = os.environ | kwargs['env_add']
        del kwargs['env_add']

    process = subprocess.Popen(command, **kwargs)
    out_chunks, err_chunks, ui_buffer, ui_buffer_spill = [], [], [""], []
    line_queue = queue.Queue()

    def reader(stream, stream_name):
        while True:
            char = stream.read(1)
            if not char: break
            line_queue.put((stream_name, char))
        stream.close()

    threading.Thread(target=reader, args=(process.stdout, 'stdout'), daemon=True).start()
    threading.Thread(target=reader, args=(process.stderr, 'stderr'), daemon=True).start()

    # 3. The Responsive UI Loop
    live_ctx = Live(auto_refresh=False, transient=True, console=console) if cfg['enabled'] else nullcontext()

    with live_ctx as live:
        while process.poll() is None or not line_queue.empty():
            start_time = time.time()
            data_received = False

            # Drain the queue as fast as possible
            try:
                while True:
                    source, char = line_queue.get_nowait()
                    data_received = True

                    if source == 'stdout': out_chunks.append(char)
                    else: err_chunks.append(char)

                    if char == '\n':
                        ui_buffer.append("")
                    else:
                        prefix = "[red][ERR][/red] " if source == 'stderr' and not ui_buffer[-1] else ""
                        ui_buffer[-1] += prefix + char

                    if len(ui_buffer) > cfg['max_lines']:
                        ui_buffer_spill.append(ui_buffer.pop(0))
            except queue.Empty:
                pass

            # Update the display only if we have new data and live is enabled
            if data_received and cfg['enabled']:
                live.update(Panel("\n".join(ui_buffer).strip(), title=str(command), border_style=cfg['border_style']), refresh=True)

            # 4. Precision Timing
            # Calculate how long to sleep to maintain the desired FPS
            elapsed = time.time() - start_time
            sleep_time = max(0, refresh_interval - elapsed)
            time.sleep(sleep_time)

    # 5. Finalize
    result = subprocess.CompletedProcess(
        args=command,
        returncode=process.wait(),
        stdout="".join(out_chunks),
        stderr="".join(err_chunks)
    )

    if cfg['show_after'] and ui_buffer:
        show_val = cfg['show_after']
        if (callable(show_val) and show_val(result)) or (not callable(show_val) and show_val):
            console.print( Panel(
                "\n".join(ui_buffer_spill + ui_buffer),
                title=str(command),
                border_style=cfg['border_style_success'] if result.returncode == 0 else cfg['border_style_error']
            ))

    return result

def which(cmd: str) -> str | None:
    return shutil.which(cmd)

def get_nproc() -> int:
    try:
        return os.cpu_count() or 1
    except Exception:
        return 1

SOURCE_PYFILE_START='''echo \\" <<'RUN_AS_PYTHON' >/dev/null # " | Out-Null\n'''
SOURCE_PYFILE_END='''<#\nRUN_AS_PYTHON'''
def source_pyfile(
    filepath: Union[str, Path],
    start=SOURCE_PYFILE_START,
    end=SOURCE_PYFILE_END
) -> dict:
    """Execute a Python file in an isolated namespace (like shell source)."""
    filepath = Path(filepath)
    ns = {"__file__": str(filepath)}
    with open(filepath) as f:
        content = f.read()
        code = content[
            ((content.index(start) + len(start)) if start in content else 0):
            content.index(end) if end in content else len(content)
        ]
        exec(compile(code, str(filepath), "exec"), ns)
    return ns

from contextlib import contextmanager

@contextmanager
def newenv(name="unnamed", env_vars=None, prepend_path=None):
    """Temporarily modify os.environ and sys.path."""
    # Backup: original environment changes are tracked per key
    original_env = {}
    if env_vars:
        for key, value in env_vars.items():
            original_env[key] = os.environ.get(key)
            os.environ[key] = value

    # Backup and replace sys.path
    original_syspath = sys.path[:]
    if prepend_path:
        sys.path = prepend_path + sys.path

    try:
        log(f'Created new temporary env [{name}]', _stackoffset=3)
        yield
    finally:
        # Restore os.environ
        for key, old_value in original_env.items():
            if old_value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = old_value
        # Restore sys.path
        sys.path = original_syspath
        log(f'Dropped env [{name}]', _stackoffset=3)

def import_module(name: str, path: str):
    spec   = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module

def mypath(caller_file: str) -> Path:
    return Path(caller_file).resolve().parent
