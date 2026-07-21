import os
import shutil
import sys

builddir = sys.argv[1]
host_system = sys.argv[2]
bundle_name = sys.argv[3]

destdir = os.getenv('MESON_INSTALL_DESTDIR_PREFIX')
gemdir = os.path.join(destdir, 'gems')
os.makedirs(gemdir, exist_ok=True)
libdir = os.path.join(destdir, bundle_name, 'Contents', 'Frameworks') if bundle_name != '' else destdir
os.makedirs(libdir, exist_ok=True)

if host_system in ('windows', 'cygwin'):
    shutil.copy2(os.path.join(builddir, 'discord.so'), os.path.join(gemdir, 'discord.so'))
    shutil.copy2(os.path.join(builddir, 'discord_game_sdk.dll'), os.path.join(libdir, 'discord_game_sdk.dll'))
elif host_system == 'darwin':
    shutil.copy2(os.path.join(builddir, 'discord.bundle'), os.path.join(gemdir, 'discord.bundle'))
    shutil.copy2(os.path.join(builddir, 'libdiscord_game_sdk.dylib'), os.path.join(libdir, 'libdiscord_game_sdk.dylib'))
elif host_system == 'linux':
    shutil.copy2(os.path.join(builddir, 'discord.so'), os.path.join(gemdir, 'discord.so'))
    shutil.copy2(os.path.join(builddir, 'discord_game_sdk.so'), os.path.join(libdir, 'discord_game_sdk.so'))
else:
    raise RuntimeError('unknown system ' + host_system)
