import os
import shutil
import subprocess
import sys

input_path = sys.argv[1]
output_path = os.path.join(os.getenv('MESON_INSTALL_DESTDIR_PREFIX'), sys.argv[2])
strip = sys.argv[3:]

os.makedirs(os.path.dirname(output_path), exist_ok=True)
shutil.copy2(input_path, output_path)
if len(strip) > 0:
    subprocess.run(strip + [output_path])
