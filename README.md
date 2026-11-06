regiongc
============================================================

regiongc is a small C11 library for region-based memory management with optional tracing garbage collection.

Objects are accessed through stable references and organized into hierarchical lifetime scopes. References between
objects are tracked explicitly.

Building
------------------------------------------------------------

Requires CMake and a C11 compiler.

```sh
mkdir ./build
cd ./build
cmake ..
make
```

Status
------------------------------------------------------------

Experimental. API and implementation are subject to change.

License
------------------------------------------------------------

Beautiful zlib-license.
