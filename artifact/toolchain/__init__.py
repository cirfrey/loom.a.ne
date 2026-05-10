import importlib

from artifact import settings

def fetch(s: settings, name: str, toolchain_args):
    tc = importlib.import_module(f"artifact.toolchain.toolchains.{name}")
    tc.fetch(s, toolchain_args)
