#!/bin/bash
emcc shooter.c -o shooter.js -O3 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']" -s "RESERVED_FUNCTION_POINTERS=1"