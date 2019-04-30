sudo apt-get install automake autoconf
cd liquid-dsp
./bootstrap.sh
./configure
make
sudo make install
sudo ldconfig
cd ..