from __future__ import annotations
import shutil
import tarfile
import zipfile
from pathlib import Path
from urllib.request import urlretrieve

from artifact.util.log import log


def download(url: str, dest: Path) -> None:
    log(f"  Downloading {dest.name} …", _stackoffset=2)
    urlretrieve(url, dest)


def extract_tar(archive: Path, dest: Path) -> None:
    with tarfile.open(archive) as tf:
        tf.extractall(dest)


def extract_zip(archive: Path, dest: Path) -> None:
    with zipfile.ZipFile(archive) as zf:
        zf.extractall(dest)


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
        download(url, archive_path)

    if dest_dir.exists():
        shutil.rmtree(dest_dir)
    dest_dir.mkdir(parents=True)

    if archive_name.endswith(".zip"):
        extract_zip(archive_path, dest_dir)
    else:
        extract_tar(archive_path, dest_dir)

    if strip_components:
        items = list(dest_dir.iterdir())
        if len(items) == 1 and items[0].is_dir():
            inner = items[0]
            for child in inner.iterdir():
                shutil.move(str(child), str(dest_dir / child.name))
            inner.rmdir()
