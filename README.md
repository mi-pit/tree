# `tree`

## Requirements

* `gcc`/`clang`
* macOS (realistically, it works on any POSIX-compliant system,
         it's just that the colors and other stuff is 'calibrated' for macOS)

## Usage

```bash
tree --help
tree
tree --depth=1 /
tree . -asc
```

---

## Compilation

```bash
cc --std=c2x -D_POSIX_C_SOURCE=200809L -o tree src/*.c lib/CLibs/src/*.c lib/CLibs/src/Structs/*.c
```

---

## Example

```bash
git clone https://github.com/mi-pit/tree
cd tree
cc --std=c2x -D_POSIX_C_SOURCE=200809L -o tree src/*.c lib/CLibs/src/*.c lib/CLibs/src/Structs/*.c
./tree
```
