import zlib

def main(filename):
    with open(filename, 'r') as f:
        data = f.read()
        