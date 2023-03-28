# Recommended environment to build
* Ubuntu 20.04(WSL)

# Prepare dependencies

```sh
sudo apt install qemu-system-x86
sudo apt install build-essential clang nasm lld uuid-dev python
git clone https://github.com/tianocore/edk2.git --recursive -b edk2-stable202011
make -C edk2/BaseTools
```
This repository and EDK II shoud be cloned into the same directory hierarchy.

# How to build and run
```sh
./create_empty_img.sh
./build_loader.sh
sudo ./write_img.sh
./run.sh
```
