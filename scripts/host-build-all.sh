# fail on any errors
set -e

# run autogen.sh
./autogen.sh

# build all images for all targets.
pwd=`pwd`

# Build library for host platform architecture
./configure --with-arch=host --enable-library=yes --host=x86_64-unknown-linux-gnu --prefix=$pwd/../host-root/
make
make install
