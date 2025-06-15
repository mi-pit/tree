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
cc --std=c11 -D_POSIX_C_SOURCE=200809L -o tree src/*.c lib/CLibs/src/*.c lib/CLibs/src/Structs/*.c
```

(`std` may also be `c17`/`c23`/`c2x`)

### Example

```bash
git clone --recursive https://github.com/mi-pit/tree
cd tree
cc --std=c11 -D_POSIX_C_SOURCE=200809L -o tree src/*.c lib/CLibs/src/*.c lib/CLibs/src/Structs/*.c
./tree
```

## Other info

Written in pure C, compatible with C11 and newer.

Tested on

* macOS Sequoia
* macOS Monterrey
* Red Hat Enterprise Linux

Doesn't have recursive link detection (does have a maximum depth set to SIZE_MAX by default)
