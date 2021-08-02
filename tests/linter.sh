#! /bin/sh

set -eu

find packages -name '*.hgn' | sort | while read package ; do
	assumeUsed="$(awk "/$(basename ${package})/{print \$2}" ./tests/data/assume-used.lst)"
	( set -x && ./build/release/huginn/1exec --module-path packages --lint --assume-used "${assumeUsed}" "${package}" )
done

