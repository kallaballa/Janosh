
echo "Building luajit and luarocks..."
git clone https://github.com/kallaballa/luajit-rocks.git
cd luajit-rocks
mkdir build
cd build
make clean
cmake ..
make install

