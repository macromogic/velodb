#!/bin/sh

# psw
cd ~/linux-sgx;
make psw;
make psw_install_pkg;
# check the detailed command suggested on the screen.
sudo ./linux/installer/bin/sgx_linux_x64_psw_2.20.100.4.bin;
# replace 'focal' with 'bionic' for Ubuntu 18 or 'jammy' for Ubuntu 22
echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list;
# Download the 'intel-sgx-deb.key' from https://download.01.org/intel-sgx/sgx_repo/ubuntu/
sudo apt-key add intel-sgx-deb.key;
# There should be 'OK' on the screen
sudo apt-get update;
sudo apt-get install libsgx-launch libsgx-urts -y;
sudo apt-get install libsgx-epid libsgx-urts -y;
sudo apt-get install libsgx-quote-ex libsgx-urts -y;
sudo apt-get install libsgx-dcap-ql -y;
source /opt/intel/sgxsdk/environment;
sudo service aesmd start;

# openssl
cd ~/;
git clone https://github.com/intel/intel-sgx-ssl;
cd intel-sgx-ssl;
cd openssl_source;
# download the exact OpenSSL version required by the version of intel-sgx-ssl you downloaded (double check by its github webpage)
wget https://www.openssl.org/source/openssl-3.0.11.tar.gz;
cd ../Linux;
make clean;
make;
sudo make install;