from pathlib import Path
import sys

def main():
    infile = sys.argv[1]

    with open(infile, 'r') as f:
        content = f.read()
    delim = rand_alpha(16)
    print(f'// Generated from [{Path(infile).resolve()}].')
    print(f'static constexpr char {Path(infile).stem}[] = R"{delim}({content}){delim}";')

def rand_alpha(len):
    import random
    return ''.join(random.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ') for i in range(len))

if __name__ == '__main__': main()
