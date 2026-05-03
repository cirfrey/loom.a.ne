from __future__ import annotations
from pathlib import Path
from string import Template


def render_template(template_path: Path, output_path: Path, vars: dict) -> None:
    """
    Render a template file supporting  % if VAR: / % endif  blocks.
    A block is kept if VAR is present in vars (regardless of value).
    Substitution uses Python's string.Template ($VAR / ${VAR}).
    """
    with open(template_path) as f:
        lines = f.read().splitlines()

    kept:    list[str] = []
    stack:   list[bool] = []
    keeping: bool = True

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("% if "):
            var = stripped[5:].rstrip(":")
            stack.append(keeping)
            keeping = var in vars
        elif stripped == "% endif":
            keeping = stack.pop() if stack else True
        elif keeping:
            kept.append(line)

    result = Template("\n".join(kept)).safe_substitute(vars)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(result)
    output_path.chmod(0o755)
