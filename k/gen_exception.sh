#!/bin/sh

# Generates an exception number given the name as its only argument.

echo -n "$1" | md5sum | sed 's/^\([a-z0-9]\{16\}\).*$/\1/g'
