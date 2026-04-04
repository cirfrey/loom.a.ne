import datetime
import subprocess
import os

Import("env")

try:
    git_hash = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).decode('ascii').strip()
except Exception:
    git_hash = "unknown"
build_date = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%S")
major      = env.GetProjectOption("custom_loomane_version_major")
minor      = env.GetProjectOption("custom_loomane_version_minor")

template_file = os.path.join(env.subst("$PROJECT_DIR"), "include", "lm", "version", "defs.hpp.in")
output_dir    = os.path.join(env.subst("$BUILD_DIR"), "generated", "include", "lm", "version")
output_file   = os.path.join(output_dir, "defs.hpp")

if not os.path.exists(output_dir): os.makedirs(output_dir)

if os.path.exists(template_file):
    with open(template_file, "r") as f:
        content = f.read()

        content = content.replace("@PROJECT_VERSION_MAJOR@", major)
        content = content.replace("@PROJECT_VERSION_MINOR@", minor)
        content = content.replace("@GIT_HASH@", git_hash)
        content = content.replace("@BUILD_DATE@", build_date)

    with open(output_file, "w") as f:
        f.write(content)


env.Append(CPPPATH=[os.path.join(env.subst("$BUILD_DIR"), "generated", "include")])
print(f"--- Generated {output_file} (Hash: {git_hash}) ---")
