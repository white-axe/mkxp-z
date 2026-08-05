import sys

print(';'.join(arg.replace('\\', '/') for arg in sys.argv[1:]))
