eBPF Setup:

sudo apt-get -f install

sudo apt-get install clang --fix-missing

sudo apt-get install bpfcc-tools linux-headers-$(uname -r) --fix-broken

might need to kill thje process holding the lock on dpkg (sudo kill -9 [6550])

sudo apt install pkg-config

sudo apt install m4

sudo apt install libelf-dev

sudo apt install libpcap-dev

sudo apt install gcc-multilib

sudo apt-get install libbpf-dev


sudo rm /usr/sbin/bpftool

sudo apt update

sudo apt-get install llvm-dev

cd /

sudo git clone --recurse-submodules https://github.com/libbpf/bpftool.git

cd bpftool/src

sudo ln -s /usr/local/sbin/bpftool /usr/sbin/bpftool

sudo make install
