# Errands

> [!WARNING]
> Development branch. Work in progress.

### Building

Errands use my own build system called **[pug.h](https://github.com/mrvladus/pug.h)**. It needs no external build tools. Only C compiler.

Bootstrap build system:

```sh
gcc -o pug pug.c
```

Build:

```sh
./pug
```

Run:

```sh
./pug run
```

Clean:

```sh
./pug clean
```
