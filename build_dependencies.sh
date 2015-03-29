
echo "Building luajit and luarocks..."
git clone https://github.com/kallaballa/luajit-rocks.git
cd luajit-rocks
mkdir build
cd build
cmake ..
make install

echo "Building zeromq lua bindings..."
LIBDIR=/usr/lib
test -d /usr/lib64 && LIBDIR=/usr/lib64
test -d /usr/lib/arm-linux-gnueabihf && LIBDIR=/usr/lib/arm-linux-gnueabihf
test -d /usr/lib/x86_64-linux-gnu/ && LIBDIR=/usr/lib/x86_64-linux-gnu/

export PATH="$PATH:/usr/local/bin/"
luarocks install https://raw.github.com/Neopallium/lua-zmq/master/rockspecs/lua-zmq-scm-1.rockspec ZEROMQ_DIR=/usr/ ZEROMQ_LIBDIR="$LIBDIR"

luarocks install luaposix
