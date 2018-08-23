CLANG_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++
GCC_FLAGS := -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_CXX_COMPILER=/usr/bin/g++

all: clang
clang: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(CLANG_FLAGS) && make -j4
gcc: build
	cd build && cmake ../ -DCMAKE_BUILD_TYPE=RelWithDebInfo $(GCC_FLAGS) && make -j4
build:
	mkdir build


configure-clang-travis: configure-clang6-1404
configure-gcc-travis: configure-gcc-7


configure-gcc-7:
	sudo apt -y --force-yes install g++-7
	sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 100 --slave /usr/bin/g++ g++ /usr/bin/g++-7
	sudo update-alternatives --set gcc /usr/bin/gcc-7


download-clang6-1404:
	sudo wget -O /tmp/clang++6.zip http://ateh10.net/dev/clang++6-1404.zip
	sudo unzip -quo /tmp/clang++6.zip -d /usr/local
	sudo rm -f /tmp/clang++6.zip
download-clang6-1604:
	sudo wget -O /tmp/clang++6.zip http://ateh10.net/dev/clang++6-1604.zip
	sudo unzip -quo /tmp/clang++6.zip -d /usr/local
	sudo rm -f /tmp/clang++6.zip
	sudo ls -l /usr/local/clang++6
install-clang6:
	sudo ln -sf /usr/local/clang++6/lib/libc++.so.1    /usr/lib
	sudo ln -sf /usr/local/clang++6/lib/libc++abi.so.1 /usr/lib
	sudo ln -sf /usr/local/clang++6/bin/clang      /usr/bin/clang-6.0
	sudo ln -sf /usr/local/clang++6/bin/clang++    /usr/bin/clang++-6.0
	sudo ln -sf /usr/local/clang++6/include/c++/v1 /usr/include/c++/v1
	sudo update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-6.0   100
	sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-6.0 100
	sudo update-alternatives --set clang   /usr/bin/clang-6.0
	sudo update-alternatives --set clang++ /usr/bin/clang++-6.0
configure-clang6-1604: download-clang6-1604 install-clang6
configure-clang6-1404: download-clang6-1404 install-clang6


/opt/pip: configure-pip
configure-pip:
	sudo mkdir -p /opt/pip
	sudo wget https://bootstrap.pypa.io/get-pip.py -P /opt/pip
	sudo python3.6 /opt/pip/get-pip.py

/opt/cmake: configure-cmake
configure-cmake:
	sudo mkdir -p /opt/cmake
	sudo wget https://cmake.org/files/v3.12/cmake-3.12.1-Linux-x86_64.sh -P /opt/cmake
	sudo sh /opt/cmake/cmake-3.12.1-Linux-x86_64.sh --prefix=/opt/cmake --skip-license
	sudo ln -sf /opt/cmake/bin/cmake /usr/bin/cmake
