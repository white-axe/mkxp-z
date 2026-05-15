import shlex
import sys

host_machine = sys.argv[1]
convert_arg0 = sys.argv[2].lower() != 'false'
argv = sys.argv[3:]

if convert_arg0 and host_machine in ('windows', 'cygwin') and len(argv) > 0:
    argv[0] = argv[0].replace('\\', '/')
    if ':' in argv[0]:
        argv[0] = '/' + argv[0].split(':', 1)[0].lower() + argv[0].split(':', 1)[1]

print(' '.join(map(shlex.quote, argv)))
