# tree

## Requirements

* gcc/clang
* [CLibs](https://github.com/mi-pit/CLibs) (must be located below the current directory)

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
cc --std=c23 -o tree main.c ../CLibs/src/*.c ../CLibs/src/Structs/*.c
```

---

## Example

```bash
git clone https://github.com/mi-pit/tree
cd tree
cc --std=c23 -o tree src/main.c lib/CLibs/src/*.c lib/CLibs/src/Structs/*.c
./tree
```
