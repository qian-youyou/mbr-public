import sys

def SDBMHash(key):
    hash = 0
    for i in range(len(key)):
        hash = ord(key[i]) + (hash << 6) + (hash << 16) - hash;
    return (hash & 0x7FFFFFFF)

print SDBMHash(sys.argv[1])
