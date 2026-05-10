from pathlib import Path

def main():
    import sys
    convert(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else 16)

def convert(infile, delim_size):
    with open(infile, 'r') as f: content = f.read()
    delim = rand_alpha(delim_size)
    print(f'// Generated from [{Path(infile).resolve()}].')
    print(f'static constexpr char {Path(infile).stem}[] = R"{delim}({content}){delim}";')

def rand_alpha(len):
    import random
    return ''.join(random.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ') for i in range(len))

if __name__ == '__main__': main()
