
sudo id
echo "Building luajit and luarocks..."
rm -fr luajit-rocks/
git clone https://github.com/torch/luajit-rocks.git
cd luajit-rocks/luajit-2.0
mkdir build
cd build
make clean
cmake ..
sudo make install

