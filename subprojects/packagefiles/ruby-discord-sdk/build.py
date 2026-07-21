import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile

host_system = sys.argv[1]
srcdir = sys.argv[2]
builddir = sys.argv[3]
miniruby = os.path.join(os.getcwd(), sys.argv[4])
make = sys.argv[5:-2]
lipo = sys.argv[-2]
output_path = os.path.join(os.getcwd(), sys.argv[-1])

os.chdir(builddir)
os.makedirs('build', exist_ok=True)

opener = urllib.request.build_opener()
opener.addheaders = [('User-Agent', '')] # urllib's user agent is blacklisted so we need to change it
urllib.request.install_opener(opener)
urllib.request.urlretrieve('https://dl-game-sdk.discordapp.net/3.2.1/discord_game_sdk.zip', 'discord_game_sdk.zip')

with zipfile.ZipFile('discord_game_sdk.zip') as f:
    if host_system in ('windows', 'cygwin'):
        shutil.move(f.extract('lib/x86_64/discord_game_sdk.dll'), 'discord_game_sdk.dll')
    elif host_system == 'darwin':
        shutil.move(f.extract('lib/aarch64/discord_game_sdk.dylib'), 'discord_game_sdk.aarch64.dylib')
        shutil.move(f.extract('lib/x86_64/discord_game_sdk.dylib'), 'discord_game_sdk.x86_64.dylib')
        subprocess.run([lipo, 'discord_game_sdk.aarch64.dylib', 'discord_game_sdk.x86_64.dylib', '-output', 'libdiscord_game_sdk.dylib', '-create'])
    elif host_system == 'linux':
        shutil.move(f.extract('lib/x86_64/discord_game_sdk.so'), 'discord_game_sdk.so')
    else:
        raise RuntimeError('unknown system ' + host_system)

subprocess.run([miniruby, os.path.join(srcdir, 'extconf.rb')])
subprocess.run(make + ['DESTDIR=build'])
target_filename = 'discord.bundle' if host_system == 'darwin' else 'discord.so'
shutil.move(next(os.path.join(dirname, filename) for dirname, _, filenames in os.walk('build') for filename in filenames if filename == target_filename), target_filename)

with open(output_path, 'w'):
    pass
