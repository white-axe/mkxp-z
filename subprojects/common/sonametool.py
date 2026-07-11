import sys

host_machine_system = sys.argv[1]
library = sys.argv[2]

host_machine_is_bsd = host_machine_system in ('dragonfly', 'freebsd', 'netbsd', 'openbsd')
host_machine_is_darwin = host_machine_system == 'darwin'
host_machine_is_linux = host_machine_system == 'linux'
host_machine_is_nix = host_machine_is_bsd or host_machine_is_linux
host_machine_is_windows = host_machine_system in ('cygwin', 'windows')

def main() -> str:
    if library == 'decor-0':
        if host_machine_is_nix:
            return 'libdecor-0.so.0'
    if library == 'EGL':
        if host_machine_is_bsd:
            return 'libEGL.so'
        if host_machine_is_darwin:
            return 'libEGL.dylib'
        if host_machine_is_linux:
            return 'libEGL.so.1'
        if host_machine_is_windows:
            return 'libEGL.dll'
    if library == 'fluidsynth':
        if host_machine_is_darwin:
            return 'libfluidsynth.3.dylib'
        if host_machine_is_nix:
            return 'libfluidsynth.so.3'
        if host_machine_is_windows:
            return 'fluidsynth.dll'
    if library == 'udev':
        if host_machine_is_nix:
            return 'libudev.so.1'
    if library == 'wayland-client':
        if host_machine_is_nix:
            return 'libwayland-client.so.0'
    if library == 'wayland-cursor':
        if host_machine_is_nix:
            return 'libwayland-cursor.so.0'
    if library == 'wayland-egl':
        if host_machine_is_nix:
            return 'libwayland-egl.so.1'
    if library == 'X11':
        if host_machine_is_nix:
            return 'libX11.so.6'
    if library == 'xcb':
        if host_machine_is_nix:
            return 'libxcb.so.1'
    if library == 'Xcursor':
        if host_machine_is_nix:
            return 'libXcursor.so.1'
    if library == 'Xext':
        if host_machine_is_nix:
            return 'libXext.so.6'
    if library == 'Xfixes':
        if host_machine_is_nix:
            return 'libXfixes.so.3'
    if library == 'Xi':
        if host_machine_is_nix:
            return 'libXi.so.6'
    if library == 'xkbcommon':
        if host_machine_is_nix:
            return 'libxkbcommon.so.0'
    if library == 'Xrandr':
        if host_machine_is_nix:
            return 'libXrandr.so.2'
    raise RuntimeError('Could not determine the soname for the ' + library + ' library on ' + host_machine_system + '. Please add it in subprojects/common/sonametool.py.')

print(main())
