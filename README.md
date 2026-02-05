# Collection
Collection is a library that implements various data structures and algorithms.

## Installation
You can just clone the repository and compile the source files as a shared object in `./so/libcollection.so` with
```sh
git clone https://github.com/derialdavi/collection
cd collection
make
```
and then linking the library to other projects with the compiler linker flags, for example
```sh
gcc mysrc.c -lcollection -L<path-to-collection>/so -Wl,-rpath,<path-to-collection>/so
```
OR, you can install the library in the default `gcc` directories with
```sh
sudo make install
```
this will find the shared object library and copy the library there, then copy the headers in `/usr/include`. If you changed your default settings, please make sure to install it in the right place.

## Tests
This project uses [ceedling](https://github.com/ThrowTheSwitch/Ceedling) to automate tests. You can run all tests with
```sh
ceedling test:all
```
or run test for a specific module by specifying the module name after the colons.