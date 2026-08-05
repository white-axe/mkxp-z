import shlex
import sys

print(' '.join(map(shlex.quote, sys.argv[1:])))
