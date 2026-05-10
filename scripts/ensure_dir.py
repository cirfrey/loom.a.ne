from pathlib import Path

def main():
    import sys
    enrure_dir(sys.argv[1])

def enrure_dir(d):
    d = Path(d)
    d.mkdir(parents=True, exist_ok=True)
    print(d.resolve())

if __name__ == '__main__': main()
