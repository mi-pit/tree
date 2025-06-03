# tree

Requires [CLibs](https://github.com/mi-pit/CLibs)

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
