#!/usr/bin/env bash
# Create a set of SquashFS test images (and deltas) for two given
# directories (old and new).

dir1=${1}
dir2=${2}
out=${3}

if ! [[ ${dir1} && ${dir2} && ${out} ]]; then
	echo "Usage: ${0} <dir1> <dir2> <output-file-prefix>" >&2
	exit 1
fi

tests=()
extra_dist=()
cleanfiles=()

create_files() {
	local prefix=${1}
	local opts=( "${@:2}" )

	mksquashfs "${dir1}" "${prefix}.in" -all-root "${opts[@]}" >&2
	mksquashfs "${dir2}" "${prefix}.out" -all-root "${opts[@]}" >&2
	squashdelta "${prefix}.in" "${prefix}.out" "${prefix}.sqdelta"

	tests+=( "${prefix##*/}.sqdelta" )
	extra_dist+=( "${prefix##*/}.in" "${prefix##*/}.out" "${prefix##*/}.sqdelta" )
	cleanfiles+=( "${prefix##*/}.testout" )
}

set -e -x

create_files "${out}-lzo1" -comp lzo -Xcompression-level 1
create_files "${out}-lzo9" -comp lzo -Xcompression-level 9
create_files "${out}-lz4" -comp lz4
create_files "${out}-lz4hc" -comp lz4 -Xhc

set +e +x

echo "TESTS += \\"
for x in "${tests[@]}"; do
	echo "	${x} \\"
done
echo
echo "EXTRA_DIST += \\"
for x in "${extra_dist[@]}"; do
	echo "	${x} \\"
done
echo
echo "CLEANFILES += \\"
for x in "${cleanfiles[@]}"; do
	echo "	${x} \\"
done
