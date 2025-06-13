# tree

Requires [CLibs](https://github.com/mi-pit/CLibs)

## Usage

```{bash}
$ tree --help
Displays a directory and its sub-directories as a tree, kinda like 'tree' on windows
Options:
	-a	 Include directory entries whose names begin with a dot.
	-s	 Display the size of each file.
	-c	 Only use ASCII characters.
	-e	 Print an error message to stderr when failing to open/stat/... a file.
	-l	 Acts on the target of a symlink instead of the symlink itself.
	--depth=%i
		 where %i is a non-negative integer; Only goes %i levels deep (the starting directory is level 0).
	--exclude=%s[,%s]*
		 Don't dive into these directories. Strings (names) separated by `,', doesn't yet support globbing
	--help
		 Display this message.
```

---

## Example

```{bash}
/path/to/CLibs/>$ tree # or `tree .` or `tree /path/to/CLibs`
.
├── CMakeLists.txt
├── LICENSE
├── README.md
├── Tests
│   ├── test_dynstr.h
│   ├── test_foreach.h
│   ├── test_hash_func.c
│   ├── test_leet.c
│   ├── test_list.h
│   ├── test_misc_c.h
│   ├── test_sets.c
│   ├── test_string_utils.h
│   ├── test_swex.h
│   ├── test_unit_tests.c
│   └── tests.c
└── src
    ├── Dev
    │   ├── assert_that.h
    │   ├── attributes.h
    │   ├── errors.h
    │   ├── filenames.h
    │   ├── pointer_utils.h
    │   ├── terminal_colors.h
    │   └── unit_tests.h
    ├── Structs
    │   ├── dictionary.c
    │   ├── dictionary.h
    │   ├── dynarr.c
    │   ├── dynarr.h
    │   ├── dynstring.c
    │   ├── dynstring.h
    │   ├── sets.c
    │   └── sets.h
    ├── array_printf.h
    ├── extra_types.h
    ├── foreach.h
    ├── item_print_functions.c
    ├── item_print_functions.h
    ├── misc.c
    ├── misc.h
    ├── string_utils.c
    ├── string_utils.h
    ├── swexpr.c
    └── swexpr.h
```
