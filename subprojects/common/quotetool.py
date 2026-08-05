import shlex
import sys

build_machine_system = sys.argv[1]
convert_arg0 = sys.argv[2].lower() != 'false'
argv = sys.argv[3:]

if convert_arg0 and build_machine_system in ('windows', 'cygwin') and len(argv) > 0:
    argv[0] = argv[0].replace('\\', '/')

print(' '.join(map(shlex.quote, argv)))
