@echo off
emcc -O3 md5.c main.c -o upc.js -s AGGRESSIVE_VARIABLE_ELIMINATION=1 -s EXPORTED_FUNCTIONS="['_upc_keys']"