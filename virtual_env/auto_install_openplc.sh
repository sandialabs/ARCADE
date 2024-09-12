##OPEN PLC##
#Stop and delete old container
sudo docker container stop openplc_dev

#kill openplc if installed on host
sudo systemctl stop openplc.service

git clone https://github.com/thiagoralves/OpenPLC_v3.git

#Comment out entrypoint command in docker image build file
#cp openplc/Dockerfile OpenPLC_v3
sed -e '/ENTRYPOINT/ s/^#*/#/' -i ./OpenPLC_v3/Dockerfile

#correct local ip address for openplc
sed -i 's/localhost/0.0.0.0/g' ./OpenPLC_v3/webserver/openplc.py

cd OpenPLC_v3
sudo docker build -t openplc_dev:v3 .
cd /opt/minimega
sudo docker run -it -d --name openplc_dev --rm --privileged -p 8080:8080 -v `pwd`:/os:rw --cap-add SYS_ADMIN --device /dev/loop0  openplc_dev:v3

#add /etc/fstab file
sudo docker exec -it openplc_dev sh -c "cat > /etc/fstab <<EOF
/dev/sda2 / ext4 defaults 0 0
EOF"

#change password of the root user for container
sudo docker exec -it openplc_dev sh -c "echo 'root:root' | chpasswd"

#Apt update
sudo docker exec -it openplc_dev sh -c "apt update -y"

#Copy the correct rc.local into container
sudo docker exec -it openplc_dev sh -c "cp /os/rc.local/rc.local.openplc /etc/rc.local"

#Copy all the potential st files to container
sudo docker exec -it openplc_dev sh -c "cp -r /os/DB/PLCS /workdir/webserver/custom_st_files"

sudo docker exec -it openplc_dev sh -c "cp /os/openplc_vm_files/custom_plc_setup.sh /workdir"

#Install ifconfig to container
sudo docker exec -it openplc_dev sh -c "apt install -y net-tools"

#install lightweight TE
sudo docker exec -it openplc_dev sh -c "apt install -y nano"

#Install ip tool to container
sudo docker exec -it openplc_dev sh -c "apt install -y iproute2"

#Install init
sudo docker exec -it openplc_dev sh -c "apt install -y init"

#Install fdisk
sudo docker exec -it openplc_dev sh -c "apt install fdisk -y"

#Copy miniccc to container
sudo docker exec -it openplc_dev sh -c "cp /os/bin/miniccc /bin/miniccc"

#Copy miniccc service to container
sudo docker exec -it openplc_dev sh -c "cp /os/services/miniccc.service /etc/systemd/system/miniccc.service"

#Create symbolic link to file in multi-user.target
sudo docker exec -it openplc_dev sh -c "ln -s /etc/systemd/system/miniccc.service /etc/systemd/system/multi-user.target.wants/miniccc.service"

#Install linux-image-amd64 boot files
sudo docker exec -it openplc_dev sh -c "apt install -y linux-image-amd64"

#Commit docker container to image
sudo docker commit openplc_dev openplc_dev:v3

#Determine image size
#IMG_SIZE=1000000000 #2.6 GB size of the image, shell for container contents
IMG_SIZE=2150000000 #2.6 GB size of the image, shell for container contents

#Create Image file
sudo docker exec -it openplc_dev sh -c "dd if=/dev/zero of=/os/openplc.img bs=$IMG_SIZE count=1"


#Then need to image the VM
sudo docker exec -it openplc_dev sh -c "sfdisk /os/openplc.img <<EOF
label: dos
label-id: 0x5d8b75fc
device: new.img
unit: sectors
openplc.img1 : start=2048, size=4192248, type=83, bootable
EOF"

#scada.img1 : start=4096, size=4190208, type=83, bootable
# size 4190208 is 2G need to leave enough space for container contents and generated content
#needs to be smaller than image size

#Get Container ID
CID=$(sudo docker run -d openplc_dev:v3 /bin/true)

#Create tar file of container contents
sudo docker export -o ./tars/openplc.tar ${CID}

# size 419028 is 2G need to leave enough space for container contents and generated content
# Create OFFSET
OFFSET=$(expr 512 \* 2048)

# Connect scada to loop device
# Need to check what loop devices are free
LOOP=$(losetup -f)
echo $LOOP

sudo losetup -o $OFFSET $LOOP ./openplc.img #need to make sure the img path is right

#Format the loop device
sudo mkfs.ext3 $LOOP

#Mount the loop device to mnt dir
sudo mount -t auto $LOOP /mnt

#Empty container contents into mounted image
sudo tar -xvf ./tars/openplc.tar -C /mnt

#Install ext linux
sudo extlinux --install /mnt/boot/

#Add syslinux config file
cat > syslinux.txt <<EOF
DEFAULT linux
  SAY Now booting the kernel from SYSLINUX...
 LABEL linux
  KERNEL /vmlinuz
  APPEND ro root=/dev/sda1 initrd=/initrd.img
EOF
sudo cp syslinux.txt /mnt/boot/syslinux.cfg

#Needed for VM to boot adds syslinux bytes to file
sudo dd if=/usr/lib/syslinux/mbr/mbr.bin of=./openplc.img bs=440 count=1 conv=notrunc

sudo umount $LOOP
sudo losetup -d $LOOP

sudo docker container stop openplc_dev
sudo rm -r OpenPLC_v3