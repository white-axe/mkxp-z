import sys

host_machine_system = sys.argv[1]
library = sys.argv[2]

host_machine_is_bsd = host_machine_system in ('dragonfly', 'freebsd', 'netbsd', 'openbsd')
host_machine_is_darwin = host_machine_system == 'darwin'
host_machine_is_linux = host_machine_system == 'linux'
host_machine_is_nix = host_machine_is_bsd or host_machine_is_linux
host_machine_is_windows = host_machine_system in ('cygwin', 'windows')

def main() -> str:
    if False:
        pass
    elif library == 'asound' and host_machine_is_linux:
        return 'libasound.so.2'
    elif library == 'EGL' and host_machine_is_bsd:
        return "libEGL.so"
    elif library == 'EGL' and host_machine_is_darwin:
        return "libEGL.dylib"
    elif library == 'EGL' and host_machine_is_linux:
        return "libEGL.so.1"
    elif library == 'EGL' and host_machine_is_windows:
        return "libEGL.dll"
    elif library == 'fluidsynth' and host_machine_is_darwin:
        return "libfluidsynth.3.dylib"
    elif library == 'fluidsynth' and host_machine_is_nix:
        return "libfluidsynth.so.3"
    elif library == 'fluidsynth' and host_machine_is_windows:
        return "fluidsynth.dll"
    elif library == 'jack' and host_machine_is_nix:
        return 'libjack.so.0'
    elif library == 'pipewire-0.3' and host_machine_is_nix:
        return 'libpipewire-0.3.so.0'
    elif library == 'pulse-simple' and host_machine_is_nix:
        return 'libpulse-simple.so.0'
    elif library == 'usb-1.0' and host_machine_is_nix:
        return 'libusb-1.0.so.0'
    elif library == 'wayland-client' and host_machine_is_nix:
        return 'libwayland-client.so.0'
    elif library == 'wayland-cursor' and host_machine_is_nix:
        return 'libwayland-cursor.so.0'
    elif library == 'wayland-egl' and host_machine_is_nix:
        return 'libwayland-egl.so.1'
    elif library == 'decor-0' and host_machine_is_nix:
        return 'libdecor-0.so.0'
    elif library == 'X11' and host_machine_is_nix:
        return 'libX11.so.6'
    elif library == 'xcb' and host_machine_is_nix:
        return 'libxcb.so.1'
    elif library == 'Xcursor' and host_machine_is_nix:
        return 'libXcursor.so.1'
    elif library == 'Xext' and host_machine_is_nix:
        return 'libXext.so.6'
    elif library == 'Xfixes' and host_machine_is_nix:
        return 'libXfixes.so.3'
    elif library == 'Xi' and host_machine_is_nix:
        return 'libXi.so.6'
    elif library == 'xkbcommon' and host_machine_is_nix:
        return 'libxkbcommon.so.0'
    elif library == 'Xrandr' and host_machine_is_nix:
        return 'libXrandr.so.2'
    elif library == 'udev' and host_machine_is_nix:
        return 'libudev.so.1'
    else:
        raise RuntimeError('Could not determine the soname for the ' + library + ' library on ' + host_machine_system + '. Please add it in subprojects/common/sonametool.py.')

print(main())
