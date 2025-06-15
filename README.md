# tree

## Requirements

* gcc/clang
* macOS

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
cc --std=c23 -o tree src/*.c lib/CLibs/src/*.c lib/CLibs/src/Structs/*.c
```

---

## Example

```bash
git clone https://github.com/mi-pit/tree
cd tree
cc --std=c23 -o tree src/*.c lib/CLibs/src/*.c lib/CLibs/src/Structs/*.c
./tree
```
