import os
import shlex
import shutil
import subprocess
import sys
import uuid

exe_path = os.path.realpath(sys.argv[1])
src_dir = os.path.realpath(sys.argv[2])
build_dir = os.path.realpath(sys.argv[3])
test_name = sys.argv[4]
templates = [template for template in sys.argv[5].split(';') if template != '']

exe_name = os.path.basename(exe_path)
script_dir = os.path.join(src_dir, 'scripts')
test_dir = os.path.join(src_dir, 'tests', test_name.replace('/', os.sep))
test_harness_dir = os.path.join(build_dir, exe_name + '.test-harness.p')
test_harness_subdir = os.path.join(test_harness_dir, str(uuid.uuid4()))

try:
    os.mkdir(test_harness_dir)
except FileExistsError:
    pass
shutil.copytree(test_dir, test_harness_subdir)
try:
    shutil.copytree(script_dir, os.path.join(test_harness_subdir, 'scripts'))
    for entry in os.scandir(build_dir):
        if entry.is_file():
            shutil.copy2(entry.path, os.path.join(test_harness_subdir, entry.name))
    if len(templates) > 0:
        with open(os.path.join(test_harness_subdir, 'mkxp.json'), 'r') as config_file:
            config = config_file.read()
        for template in templates:
            key, value = template.split('=', 1)
            config = config.replace('@' + key + '@', value)
        with open(os.path.join(test_harness_subdir, 'mkxp.json'), 'w') as config_file:
            config_file.write(config)
    result = subprocess.run(
        (shlex.split(os.environ['MESON_EXE_WRAPPER']) if 'MESON_EXE_WRAPPER' in os.environ else ()) + (
            os.path.join(test_harness_subdir, exe_name),
        ),
        cwd=test_harness_subdir,
    )
    exit(result.returncode)
finally:
    shutil.rmtree(test_harness_subdir)
