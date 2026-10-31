# This script is used to verify the integrity of a FreeBSD package using a FreeBSD package checksum.
# Licensed under CC0.

import base64
import hashlib
import sys
import textwrap

expected_checksum = sys.argv[1]
filename = sys.argv[2]

with open(filename, 'rb') as f:
    checksum = hashlib.file_digest(f, 'blake2b').digest()

# Reverse the bits in each byte of the checksum, then convert to a binary string
checksum = ''.join('{:08b}'.format(byte)[::-1] for byte in checksum)

# Split the binary string into groups of 5 digits, with an extra 3 zeros at the end for padding
checksum = textwrap.wrap(checksum, 5)
checksum[-1] += '000'

# Reverse the digits in each group of 5 digits
checksum = (bits[::-1] for bits in checksum)

# Join them back together
checksum = ''.join(checksum)

# Split the binary string into groups of 8 digits, with an extra 5 zeros at the end for padding
checksum = textwrap.wrap(checksum, 8)
checksum[-1] += '00000'

# Convert back into bytes
checksum = bytes(int(byte, 2) for byte in checksum)

# Convert to base32
checksum = base64.b32encode(checksum).decode('utf-8')

# Remove the last character, which is padding
assert checksum[-1] == 'A'
checksum = checksum[:-1]
assert '=' not in checksum

# Convert to z-base-32 alphabet
checksum = checksum.translate(str.maketrans('ABCDEFGHIJKLMNOPQRSTUVWXYZ234567', 'ybndrfg8ejkmcpqxot1uwisza345h769'))

# Add '2$' at the beginning
checksum = '2$' + checksum

if checksum == expected_checksum:
    print(filename + ': OK', file=sys.stderr)
else:
    print(filename + ': FAILED', file=sys.stderr)
    print('Expected: ' + expected_checksum, file=sys.stderr)
    print('     Got: ' + checksum, file=sys.stderr)
    exit(1)
