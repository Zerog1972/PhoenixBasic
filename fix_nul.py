import os

p = 'src/parser/parser.c'
with open(p, 'rb') as f:
    d = f.read()

# Replace the literal byte sequence: ' \x00 ' (single quote, NUL byte, single quote)
# with the two-character sequence: ' \ 0 ' (escape sequence)
n = d.count(b"'\x00'")
d = d.replace(b"'\x00'", b"'\\0'")

with open(p, 'wb') as f:
    f.write(d)

print('Replaced NUL literals:', n)