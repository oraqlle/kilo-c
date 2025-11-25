# Kilo Editor

A simple nano inspired editor written in C in under 1000 lines of code[^1].

## Usage

```sh
kilo <filename>
```

* `<C-q>` - Quit
* `<C-s>` - Save
* `<C-f>` - String search

## Build

```sh
make kilo

# ... or for debug builds

make dkilo
```

## Notes

Built following [tutorial from snaptoken](https://viewsourcecode.org/snaptoken/kilo/index.html)
and based on Salvatore Sanfilippo aka antirez original implementation.

[^1]: Made possible by horrible formatting

