from __future__ import annotations
import shutil
import tarfile
import zipfile
from pathlib import Path
from urllib.request import urlretrieve
from contextlib import nullcontext
from artifact.util.log import log

def _get_progress_ctx():
    """Helper to return a transient Rich Progress context if available."""
    try:
        from rich.progress import (
            Progress,
            DownloadColumn,
            TextColumn,
            BarColumn,
            TransferSpeedColumn,
            TimeRemainingColumn,
        )
        return Progress(
            TextColumn("[bold blue]{task.description}"),
            BarColumn(),
            DownloadColumn(),
            TransferSpeedColumn(),
            TimeRemainingColumn(),
            transient=True  # Automatically clears the output when finished
        )
    except ImportError:
        return nullcontext()

# TODO: cleanup on exception
def download(url: str, dest: Path, _stackoffset=2) -> None:
    """Downloads a file. Automatically uses a transient Rich progress bar if available."""
    ctx = _get_progress_ctx()

    with ctx as progress:
        if progress:
            task_id = progress.add_task(f"[cyan]Downloading {dest.name}", total=None)

            def reporthook(block_num: int, block_size: int, total_size: int):
                if total_size > 0:
                    progress.update(task_id, total=total_size, completed=block_num * block_size)

            urlretrieve(url, dest, reporthook=reporthook)
        else:
            log(f"Downloading {dest.name}", _stackoffset=_stackoffset)
            urlretrieve(url, dest)

    log(f'Downloaded {dest.name} to {dest}', _stackoffset=_stackoffset)

def extract_zip(archive: Path, dest: Path, _stackoffset=2) -> None:
    """Extracts a zip file. Automatically uses a transient Rich progress bar if available."""
    ctx = _get_progress_ctx()

    with ctx as progress:
        with zipfile.ZipFile(archive) as zf:
            members = zf.infolist()
            total_size = sum(m.file_size for m in members)

            if progress:
                task_id = progress.add_task(f"[green]Extracting {archive.name}", total=total_size)
                for member in members:
                    zf.extract(member, dest)
                    progress.update(task_id, advance=member.file_size)
            else:
                log(f'Extracting {archive.name}', _stackoffset=_stackoffset)
                zf.extractall(dest)

    log(f'Extracted {archive.name} to {dest}', _stackoffset=_stackoffset)

def extract_tar(archive: Path, dest: Path, _stackoffset=2) -> None:
    """Extracts a tar file. Automatically uses a transient Rich progress bar if available."""
    ctx = _get_progress_ctx()

    with ctx as progress:
        with tarfile.open(archive) as tf:
            members = tf.getmembers()

            if progress:
                task_id = progress.add_task(f"[green]Extracting {archive.name}", total=len(members))
                for member in members:
                    tf.extract(member, dest)
                    progress.update(task_id, advance=1)
            else:
                log(f'Extracting {archive.name}', _stackoffset=_stackoffset)
                tf.extractall(dest)

    log(f'Extracted {archive.name} to {dest}', _stackoffset=_stackoffset)


def download_and_extract(
    url:              str,
    archive_name:     str,
    dest_dir:         Path,
    dldir:            Path,
    strip_components: int = 1,
) -> None:
    archive_path = dldir / archive_name
    archive_path.parent.mkdir(parents=True, exist_ok=True)

    if not archive_path.exists():
        download(url, archive_path, _stackoffset=3)

    if dest_dir.exists():
        shutil.rmtree(dest_dir)
    dest_dir.mkdir(parents=True)

    if archive_name.endswith(".zip"):
        extract_zip(archive_path, dest_dir, _stackoffset=3)
    else:
        extract_tar(archive_path, dest_dir, _stackoffset=3)

    if strip_components:
        items = list(dest_dir.iterdir())
        if len(items) == 1 and items[0].is_dir():
            inner = items[0]
            for child in inner.iterdir():
                shutil.move(str(child), str(dest_dir / child.name))
            inner.rmdir()
