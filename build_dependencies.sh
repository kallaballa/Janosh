
echo "Building luajit and luarocks..."
git clone https://github.com/torch/luajit-rocks.git
cd luajit-rocks/lua-5.1
mkdir build
cd build
make clean
cmake ..
make install

