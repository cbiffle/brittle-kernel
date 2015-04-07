#!/bin/sh

echo -n "$1" | md5sum | sed 's/^\([a-z0-9]\{16\}\).*$/\1/g'
