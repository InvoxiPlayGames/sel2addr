# sel2addr

A simple C program that takes a Wii .SEL file and a .DOL (or .ELF, or RAM dump) from a Wii game, and outputs function addresses from the .SEL symbol table.

Licensed under the MIT License.

## Building

(Only tested on macOS clang-1316.0.21.2.5. *Should* work on GCC on Linux.)

```
cc sel2addr.c -o sel2addr
```