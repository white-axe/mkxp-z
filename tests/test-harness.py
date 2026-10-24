import os
import shlex
import shutil
import subprocess
import sys
import tempfile

exe_path = os.path.realpath(sys.argv[1])
test_name = sys.argv[2]
templates = [template for template in (sys.argv[3] if len(sys.argv) > 3 else '').split(';') if template != '']

src_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
build_dir = os.path.dirname(exe_path)
exe_name = os.path.basename(exe_path)
script_dir = os.path.join(src_dir, 'scripts')
test_dir = os.path.join(src_dir, 'tests', test_name.replace('/', os.sep))

with tempfile.TemporaryDirectory(dir=build_dir) as test_harness_dir:
    test_harness_subdir = os.path.join(test_harness_dir, 'test')
    shutil.copytree(test_dir, test_harness_subdir)
    shutil.copytree(script_dir, os.path.join(test_harness_subdir, 'scripts'))
    for entry in os.scandir(build_dir):
        if entry.is_file() and any(entry.name.endswith(suffix) for suffix in ('.bundle', '.dll', '.dylib', '.local', '.so')):
            shutil.copy2(entry.path, os.path.join(test_harness_subdir, entry.name), follow_symlinks=False) # `follow_symlinks=False` makes it so that if the source file is a symbolic link, the created destination file is also a symbolic link instead of a copy of the file that the source file refers to; this is important for dynamic libraries to work correctly
    shutil.copy2(exe_path, os.path.join(test_harness_subdir, exe_name))
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
