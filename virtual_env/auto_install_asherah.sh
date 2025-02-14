#First need to download all the images
#First do for a single image
#Run in /opt/minimega dir

### SCADA ###
#Pull the image down from dockerhub
sudo docker pull ubuntu:20.04
#Then need to add the desired packages and files

#Stop and delete old container
sudo docker container stop asherah_dev
sudo docker container rm asherah_dev

#Start Docker Image as Container in background
sudo docker run -it -d --name asherah_dev -v `pwd`:/os:rw --cap-add SYS_ADMIN --device /dev/loop0 ubuntu:20.04

#add /etc/fstab file
sudo docker exec -it asherah_dev sh -c "cat > /etc/fstab <<EOF
/dev/sda2 / ext4 defaults 0 0
EOF"

#change password of the root user for container
sudo docker exec -it asherah_dev sh -c "echo 'root:root' | chpasswd"

#Apt update
sudo docker exec -it asherah_dev sh -c "apt update -y && DEBIAN_FRONTEND=noninteractive"

#Apt update
sudo docker exec -it asherah_dev sh -c "apt-get update -y"

#Install python3 and python zmq
sudo docker exec -it asherah_dev sh -c "apt install -y python3 python3-pip python3-zmq"

#install libzmq
sudo docker exec -it asherah_dev sh -c "apt-get install -y libzmq3-dev"

#Install pymodbus and opcua if installing on 20.04
sudo docker exec -it asherah_dev sh -c "pip3 install pymodbus opcua"

#Install pymodbus and opcua if installing on 22.04+ 
sudo docker exec -it asherah_dev sh -c "pip3 install pymodbus opcua --break-system-packages"

#Install ifconfig to container
sudo docker exec -it asherah_dev sh -c "apt install -y net-tools"

#Copy the correct rc.local into container
sudo docker exec -it asherah_dev sh -c "cp /os/rc.local/rc.local.asherah /etc/rc.local"

#install lightweight TE
sudo docker exec -it asherah_dev sh -c "apt install -y nano vim"

#Install ip tool to container
sudo docker exec -it asherah_dev sh -c "apt install -y iproute2"

#Install init
sudo docker exec -it asherah_dev sh -c "apt install -y init"

#Install fdisk
sudo docker exec -it asherah_dev sh -c "apt install -y fdisk"

#Copy miniccc to container
sudo docker exec -it asherah_dev sh -c "cp -r /os/DB /opt/DB"

#Copy miniccc to container
sudo docker exec -it asherah_dev sh -c "cp /os/bin/miniccc /bin/miniccc"

#Copy miniccc to container
sudo docker exec -it asherah_dev sh -c "cp /os/services/miniccc.service /etc/systemd/system/miniccc.service"

#Create symbolic link to file in multi-user.target
sudo docker exec -it asherah_dev sh -c "ln -s /etc/systemd/system/miniccc.service /etc/systemd/system/multi-user.target.wants/miniccc.service"

#Copy files from local DB folder to image
sudo docker exec -it asherah_dev sh -c "cp /os/asherah_sim_vm_files/start_sim.sh /opt/DB/"

#Fix permissions on files
sudo docker exec -it asherah_dev sh -c "chmod +x /opt/DB/DB"

sudo docker exec -it asherah_dev sh -c "chmod +x /opt/DB/ans_simdiag_15jul21"

#Copy sim script to container
sudo docker exec -it asherah_dev sh -c "cp /os/services/sim.service /etc/systemd/system/sim.service"

#Create symbolic link to file in multi-user.target
sudo docker exec -it asherah_dev sh -c "ln -s /etc/systemd/system/sim.service /etc/systemd/system/multi-user.target.wants/sim.service"

#Install linux-image-amd64 boot files
sudo docker exec -it asherah_dev sh -c "apt install -y linux-image-generic"
sudo docker exec -it asherah_dev sh -c "apt-get -y install --no-install-recommends systemd-sysv"

#Commit docker container to image
sudo docker commit asherah_dev asherah_dev

IMG_SIZE=1000000000 #2.6 GB size of the image, shell for container contents

#Create Image file
sudo docker exec -it asherah_dev sh -c "dd if=/dev/zero of=/os/asherah.img bs=$IMG_SIZE count=3"

#Then need to image the VM
sudo docker exec -it asherah_dev sh -c "sfdisk /os/asherah.img <<EOF
label: dos
label-id: 0x5d8b75fc
device: new.img
unit: sectors
asherah.img1 : start=2048, size=5857327, type=83, bootable
EOF"
#Determine image size

# size 4190208 is 2G need to leave enough space for container contents and generated content
#needs to be smaller than image size

#Get Container ID
CID=$(sudo docker run -d asherah_dev /bin/true)

#Create tar file of container contents
sudo docker export -o ./tars/asherah.tar ${CID}

# size 419028 is 2G need to leave enough space for container contents and generated content
# Create OFFSET
OFFSET=$(expr 512 \* 2048)

# Connect asherah to loop device
# Need to check what loop devices are free
LOOP=$(losetup -f)
echo $LOOP

sudo losetup -o $OFFSET $LOOP ./asherah.img #need to make sure the img path is right

#Format the loop device
sudo mkfs.ext4 $LOOP

#Mount the loop device to mnt dir
sudo mount -t auto $LOOP /mnt

#Empty container contents into mounted image
sudo tar -xvf ./tars/asherah.tar -C /mnt

#Install ext linux
sudo extlinux --install /mnt/boot/

#Add syslinux config file
cat > syslinux.txt <<EOF
DEFAULT linux
  SAY Now booting the kernel from SYSLINUX...
 LABEL linux
  KERNEL /boot/vmlinuz
  APPEND ro root=/dev/sda1 initrd=/boot/initrd.img
EOF
sudo cp syslinux.txt /mnt/boot/syslinux.cfg

#Needed for VM to boot adds syslinux bytes to file
sudo dd if=/usr/lib/syslinux/mbr/mbr.bin of=./asherah.img bs=440 count=1 conv=notrunc

sudo umount $LOOP
sudo losetup -d $LOOP

sudo docker container stop asherah_dev
sudo docker container rm asherah_dev