import importlib.util
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent


def load(relative: str):
    """A tool script under the repo root, loaded as a module."""
    path = ROOT / relative
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
