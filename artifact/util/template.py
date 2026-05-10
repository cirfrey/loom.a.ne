from __future__ import annotations
import re
import sys
from pathlib import Path
from io import StringIO


def render_template(template_path: Path, output_path: Path, subst: dict) -> None:
    """
    Render a template file supporting  % if VAR: / % endif  blocks.
    A block is kept if VAR is present in vars (regardless of value).
    Substitution uses Python's string.Template ($VAR / ${VAR}).
    """
    with open(template_path) as f: content = f.read()
    result = render_template_str(content, subst)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(result)
    output_path.chmod(0o755)

def render_template_str(template: str, subst: dict) -> str:
    # 1. Handle % if / % endif (Line-based logic)
    def handle_if(match):
        expr, content = match.group(1).strip().rstrip(":"), match.group(2)
        try:
            # Use subst as globals, but disable __builtins__ for a tiny bit of safety
            if eval(expr, {"__builtins__": __builtins__}, subst):
                return content.strip("\n")
            return ""
        except Exception as e:
            return f"[[ If Error: {e} ]]"

    template = re.sub(r'^\s*% if (.*?):?\n(.*?)\s*% endif' , handle_if,
                      template, flags=re.DOTALL | re.MULTILINE)

    # 2. Handle {{% code %}} (Execution blocks - captures stdout)
    def handle_block(match):
        code = match.group(1).strip()
        output = StringIO()
        old_stdout = sys.stdout
        sys.stdout = output
        try:
            exec(code, {"__builtins__": __builtins__}, subst)
            return output.getvalue()
        except Exception as e:
            return f"[[ Exec Error: {e} ]]"
        finally:
            sys.stdout = old_stdout

    template = re.sub(r'\{\{\%(.*?)\%\}\}', handle_block, template, flags=re.DOTALL)

    # 3. Handle {{ expression }} (Evaluation blocks - like print)
    def handle_expr(match):
        expr = match.group(1).strip()
        try:
            return str(eval(expr, {"__builtins__": __builtins__}, subst))
        except Exception as e:
            return f"[[ Eval Error: {e} ]]"

    result = re.sub(r'\{\{(.*?)\}\}', handle_expr, template)

    return result
