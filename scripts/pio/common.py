import sys
scripts_path = Dir("./..").abspath
if scripts_path not in sys.path: sys.path.append(scripts_path)

# Self injects into builtins
import lm.log
