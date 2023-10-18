#!/bin/bash
output_dir="torrent_renamer_cpp_windows_x64"
build_dir="build"

# clean directory
rm -rf ${output_dir}
mkdir -p ${output_dir}

# copy binaries
cp ${build_dir}/main.exe ${output_dir}/torrent_renamer_cpp.exe

# copy resources
cp -rf res/ ${output_dir}/
# avoid copying our private credentials
rm ${output_dir}/res/credentials.json

# copy imgui workspace files
cp *.ini ${output_dir}/

# copy user docs
cp README.md ${output_dir}/