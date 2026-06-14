project('jack1', 'c', meson_version: '>=1.2.0')

jack1 = declare_dependency(
    include_directories: '.',
    link_with: library(
        'jack',
        c_args: [
            '-DDEFAULT_TMP_DIR="/tmp"',
            '-DDO_BUFFER_RESIZE',
            '-DHAVE_CLOCK_GETTIME',
            '-DHAVE_POSIX_MEMALIGN',
            '-DHAVE_STDINT_H',
            '-DJACK_DEFAULT_DRIVER="dummy"',
            '-DJACK_LOCATION="/usr/bin"',
            '-DJACK_SEMAPHORE_KEY=0x282929',
            '-DJACK_SHM_TYPE="POSIX"',
            '-DJACK_THREAD_STACK_TOUCH=' + (host_machine.system() in ['darwin', 'openbsd'] ? '10000' : '500000'),
            '-DOLD_TRANSPORT',
            '-DUSE_MLOCK',
            '-DUSE_POSIX_SHM',
            '-Djack_protocol_version=25',
        ],
        include_directories: [
            '.',
            'config',
            'include',
        ],
        sources: [
            'libjack/client.c',
            'libjack/driver.c',
            'libjack/intclient.c',
            'libjack/messagebuffer.c',
            'libjack/midiport.c',
            'libjack/pool.c',
            'libjack/port.c',
            'libjack/ringbuffer.c',
            'libjack/sanitycheck.c',
            'libjack/shm.c',
            'libjack/simd.c',
            'libjack/systemtest.c',
            'libjack/thread.c',
            'libjack/time.c',
            'libjack/transclient.c',
            'libjack/unlock.c',
            'libjack/uuid.c',
        ],
        pic: get_option('b_staticpic'),
        gnu_symbol_visibility: 'hidden',
    ),
)

meson.override_dependency('jack', jack1)
