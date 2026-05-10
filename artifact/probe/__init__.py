from pathlib import Path
import tempfile

from artifact.util.process import run, mypath

import os

def boost_links(cc: Path, cxx: Path, meson: str = "artifact-artifact-meson.cmd", env = os.environ) -> bool:
    testdir = mypath(__file__)/'boost_links'
    if env is None: env = {}

    with tempfile.TemporaryDirectory() as d:
        ret = run([meson, 'setup', d, str(testdir)], env_add={"CC": str(cc), "CXX": str(cxx)}, preview={'show_after': True}, _stackoffset=1)
        if ret.returncode == 0:
            return True
    return False
