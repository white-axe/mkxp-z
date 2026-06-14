project('tools', 'c', meson_version: '>=1.2.0')

python = import('python').find_installation(required: true)

# embedtool SOURCE_FILE DESTINATION_FILE NAME
#   Embeds the contents of a file into a C array.
#   SOURCE_FILE: Path to the file to embed.
#   DESTINATION_FILE: Path where the C source file containing the array with the contents of SOURCE_FILE should be written to.
#   NAME: The output C source file will contain an array of uint8_t named NAME containing the contents of SOURCE_FILE and a size_t named NAME_len containing the length of NAME in bytes.
embedtool = [python, files('embedtool.py')[0]]

# quotetool_args [ARGS...]
#   Outputs a shell-escaped string containing the given arguments, so that shell splitting the string output by this command yields the original arguments. For example, if the arguments are `hello` and `foo bar` and `foo 'bar'`, the output will be `hello 'foo bar' 'foo '"'"'bar'"'"''`.
quotetool_args = [python, files('quotetool.py')[0], build_machine.system(), 'false']

# quotetool_command [ARGS...]
#   This is the same thing as quotetool_args, except if the build machine is Windows, all backslashes in the first argument are replaced with forward slashes to avoid problems with build tools such as CMake that cannot handle certain path formats correctly. If the build machine is not Windows, this is exactly equivalent to quotetool_args.
quotetool_command = [python, files('quotetool.py')[0], build_machine.system(), 'true']

host_triplet_cpu = host_machine.cpu()
host_triplet_vendor = 'unknown'
if host_machine.system() in ['windows', 'cygwin']
    host_triplet_os = 'mingw64'
else
    host_triplet_os = host_machine.system()
endif
host_triplet = host_triplet_cpu + '-' + host_triplet_vendor + '-' + host_triplet_os

base_cflags = []
base_ldflags = []

if meson.get_compiler('c').get_linker_id() in ['ld64', 'ld64.lld']
    base_ldflags += '-Wl,-w'
endif

if get_option('optimization') == '0'
    base_cflags += '-O0'
elif get_option('optimization') == 'g'
    base_cflags += '-Og'
elif get_option('optimization') == '1'
    base_cflags += '-O1'
elif get_option('optimization') == '2'
    base_cflags += '-O2'
elif get_option('optimization') == '3'
    base_cflags += '-O3'
elif get_option('optimization') == 's'
    base_cflags += '-Os'
endif

if get_option('debug')
    base_cflags += '-g'
endif

if get_option('b_staticpic')
    base_cflags += '-fPIC'
endif

if get_option('b_lto')
    if get_option('b_lto_mode') == 'thin' and meson.get_compiler('c').has_argument('-flto=thin')
        base_cflags += '-flto=thin'
        base_ldflags += '-flto=thin'
    elif meson.get_compiler('c').has_argument('-flto=auto')
        base_cflags += '-flto=auto'
        base_ldflags += '-flto=auto'
    else
        base_cflags += '-flto'
        base_ldflags += '-flto'
    endif
endif

if get_option('b_sanitize') != 'none'
    base_cflags += '-fsanitize=' + get_option('b_sanitize')
    base_ldflags += '-fsanitize=' + get_option('b_sanitize')
    if 'address' in get_option('b_sanitize').split(',')
        base_cflags += '-fno-omit-frame-pointer'
    endif
endif

if get_option('b_ndebug') == 'true' or (get_option('b_ndebug') == 'if-release' and get_option('buildtype') in ['plain', 'release'])
    base_cflags += '-DNDEBUG'
endif
