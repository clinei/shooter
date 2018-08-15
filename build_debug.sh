#!/bin/bash
emcc shooter.c -o shooter.js -O0 -g4 -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']" -s "RESERVED_FUNCTION_POINTERS=1" --source-map-base http://localhost:6931/