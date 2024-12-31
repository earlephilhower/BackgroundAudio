#!/bin/bash

echo "::group::Compiling $(basename $1) for $2 into $3"
./arduino-cli compile -b "$2":opt=Optimize2 -v --warnings all --build-path "$3" "$1" 
echo "::endgroup::"
size "$3/$(basename $1).elf"
