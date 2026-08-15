import shlex
import sys

convert_arg0 = sys.argv[1].lower() != 'false'
argv = sys.argv[2:]

if convert_arg0:
    argv[0] = argv[0].replace('\\', '/')

print(' '.join(map(shlex.quote, argv)))
