cd ../edk2
ln -s ../lemonade-os/loader loader
cp loader/target_temp.txt Conf/target.txt
source edksetup.sh
build

