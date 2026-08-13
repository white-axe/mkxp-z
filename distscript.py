import configparser
import os
import shlex
import shutil
import subprocess
import sys

meson = shlex.split(os.environ['MESONREWRITE'])[:-1]

# Write the current Git hash into the release if it isn't already in git-hash
if len(sys.argv) > 1:
    shutil.copy2(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'git-hash'), os.environ['MESON_DIST_ROOT'])
    with open(os.path.join(os.environ['MESON_DIST_ROOT'], 'git-hash'), 'a') as f:
        f.write(subprocess.run(sys.argv[1:], cwd=os.environ['MESON_SOURCE_ROOT'], check=True, stdout=subprocess.PIPE).stdout.decode('UTF-8'))

# Make sure all subprojects are downloaded, even ones that are not used in the current build configuration
subprocess.run(meson + ['subprojects', 'download'], cwd=os.environ['MESON_SOURCE_ROOT'], check=True, env={**os.environ, 'MESON_PACKAGE_CACHE_DIR': ''})

# Copy each wrap-file tarball and wrap-git source repository into the release artifact
os.makedirs(os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', 'packagecache'), exist_ok=True)
for filename in os.listdir(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects')):
    if not filename.endswith('.wrap'):
        continue
    config = configparser.ConfigParser()
    config.read(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', filename))
    if 'wrap-file' in config:
        shutil.copy2(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', 'packagecache', config['wrap-file']['source_filename']), os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', 'packagecache'))
        if 'patch_filename' in config['wrap-file']:
            shutil.copy2(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', 'packagecache', config['wrap-file']['patch_filename']), os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', 'packagecache'))
    elif 'wrap-redirect' not in config:
        shutil.copytree(os.path.join(os.environ['MESON_SOURCE_ROOT'], 'subprojects', filename[:-5]), os.path.join(os.environ['MESON_DIST_ROOT'], 'subprojects', filename[:-5]))
