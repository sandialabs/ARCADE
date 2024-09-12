#First need to download all the images
#First do for a single image
#Run in /opt/minimega dir

### SCADA ###
#Pull the image down from dockerhub
sudo docker pull scadalts/scadalts:release-2.7.6
#Then need to add the desired packages and files

#Stop and delete old container
sudo docker container stop scada_dev
sudo docker container rm scada_dev

#Start Docker Image as Container in background
sudo docker run -it -d --name scada_dev -v `pwd`:/os:rw --cap-add SYS_ADMIN --device /dev/loop0  scadalts/scadalts:release-2.7.6

#add /etc/fstab file
sudo docker exec -it scada_dev sh -c "cat > /etc/fstab <<EOF
/dev/sda2 / ext4 defaults 0 0
EOF"

#change password of the root user for container
sudo docker exec -it scada_dev sh -c "echo 'root:root' | chpasswd"

#Apt update
sudo docker exec -it scada_dev sh -c "apt update -y"

#Download the mysql deb
wget http://downloads.mysql.com/archives/get/p/23/file/mysql-server_5.7.37-1ubuntu18.04_amd64.deb-bundle.tar

#Create mysql dir
mkdir mysql

#Extract to its own folder
tar -xvf mysql-server_5.7.37-1ubuntu18.04_amd64.deb-bundle.tar -C mysql

#Install mysql package
sudo docker exec -it scada_dev sh -c "apt update -y"
sudo docker exec -it scada_dev sh -c "apt install libaio1 libmecab2 libnuma1 psmisc libtinfo5"

sudo docker exec -it scada_dev sh -c "dpkg -i /os/mysql/mysql-common_5.7.37-1ubuntu18.04_amd64.deb"
sudo docker exec -it scada_dev sh -c "dpkg -i /os/mysql/mysql-community-client_5.7.37-1ubuntu18.04_amd64.deb"
sudo docker exec -it scada_dev sh -c "dpkg -i /os/mysql/mysql-client_5.7.37-1ubuntu18.04_amd64.deb"
sudo docker exec -it scada_dev sh -c "dpkg -i /os/mysql/mysql-community-server_5.7.37-1ubuntu18.04_amd64.deb"


sudo docker exec -it scada_dev sh -c "service mysql start"

#Create scadalts database
sudo docker exec -it scada_dev sh -c "mysql -u root -p -D mysql -e 'CREATE DATABASE IF NOT EXISTS scadalts'"

#Copy the correct rc.local into container
sudo docker exec -it scada_dev sh -c "cp /os/rc.local/rc.local.scada /etc/rc.local"

#Apt update
sudo docker exec -it scada_dev sh -c "apt update -y"

#Install ifconfig to container
sudo docker exec -it scada_dev sh -c "apt install -y net-tools"

#install lightweight TE
sudo docker exec -it scada_dev sh -c "apt install -y nano"

#Install ip tool to container
sudo docker exec -it scada_dev sh -c "apt install -y iproute2"

#Install init
sudo docker exec -it scada_dev sh -c "apt install -y init"

#Install fdisk
sudo docker exec -it scada_dev sh -c "apt install fdisk -y"

#Copy miniccc to container
sudo docker exec -it scada_dev sh -c "cp /os/bin/miniccc /bin/miniccc"

#Copy miniccc to container
sudo docker exec -it scada_dev sh -c "cp /os/services/miniccc.service /etc/systemd/system/miniccc.service"

#Copy context config to container
sudo docker exec -it scada_dev sh -c "cp /os/scada/context.xml /usr/local/tomcat/conf/context.xml"

#Create symbolic link to file in multi-user.target
sudo docker exec -it scada_dev sh -c "ln -s /etc/systemd/system/miniccc.service /etc/systemd/system/multi-user.target.wants/miniccc.service"

#Install linux-image-amd64 boot files
sudo docker exec -it scada_dev sh -c "apt install -y linux-image-amd64"

#Commit docker container to image
sudo docker commit scada_dev scada_dev

#Determine image size
#IMG_SIZE=1300000000 #2.6 GB size of the image, shell for container contents
IMG_SIZE=1000000000 #2.6 GB size of the image, shell for container contents

#Create Image file
sudo docker exec -it scada_dev sh -c "dd if=/dev/zero of=/os/scada.img bs=$IMG_SIZE count=7"

#Then need to image the VM
sudo docker exec -it scada_dev sh -c "sfdisk /os/scada.img <<EOF
label: dos
label-id: 0x5d8b75fc
device: new.img
unit: sectors
scada.img1 : start=2048, size=13669827, type=83, bootable
EOF"

#scada.img1 : start=4096, size=4190208, type=83, bootable
# size 4190208 is 2G need to leave enough space for container contents and generated content
#needs to be smaller than image size

#Get Container ID
CID=$(sudo docker run -d scada_dev /bin/true)

#Create tar file of container contents
sudo docker export -o ./tars/scada.tar ${CID}

# size 419028 is 2G need to leave enough space for container contents and generated content
# Create OFFSET
OFFSET=$(expr 512 \* 2048)

# Connect scada to loop device
# Need to check what loop devices are free
LOOP=$(losetup -f)
echo $LOOP

sudo losetup -o $OFFSET $LOOP ./scada.img #need to make sure the img path is right

#Format the loop device
sudo mkfs.ext3 $LOOP

#Mount the loop device to mnt dir
sudo mount -t auto $LOOP /mnt

#Empty container contents into mounted image
sudo tar -xvf ./tars/scada.tar -C /mnt

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
sudo dd if=/usr/lib/syslinux/mbr/mbr.bin of=./scada.img bs=440 count=1 conv=notrunc

sudo umount $LOOP
sudo losetup -d $LOOP

#stop and clean up docker
sudo docker container stop scada_dev
sudo docker container rm scada_dev
