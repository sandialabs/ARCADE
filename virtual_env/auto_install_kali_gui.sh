
#First need to download all the images
#First do for a single image
#Run in /opt/minimega dir

### SCADA ###
#Pull the image down from dockerhub
sudo docker pull kalilinux/kali-rolling
#Then need to add the desired packages and files

#Stop and delete old container
sudo docker container stop kali_dev
sudo docker container rm kali_dev

#Start Docker Image as Container in background
sudo docker run -it -d --name kali_dev -v `pwd`:/os:rw --cap-add SYS_ADMIN --device /dev/loop0  kalilinux/kali-rolling

#add /etc/fstab file
sudo docker exec -it kali_dev sh -c "cat > /etc/fstab <<EOF
/dev/sda2 / ext4 defaults 0 0
EOF"

#change password of the root user for container
sudo docker exec -it kali_dev sh -c "echo 'root:root' | chpasswd"

#Apt update
sudo docker exec -it kali_dev sh -c "apt update -y && DEBIAN_FRONTEND=noninteractive"

#Install wireshark and ettercap
sudo docker exec -it kali_dev sh -c "apt install -y gnome && DEBIAN_FRONTEND=noninteractive"
sudo docker exec -it kali_dev sh -c "apt install -y wireshark ettercap-common ettercap-graphical"

#Install ifconfig to container
sudo docker exec -it kali_dev sh -c "apt install -y net-tools"

#Copy the correct rc.local into container
sudo docker exec -it kali_dev sh -c "cp /os/rc.local/rc.local.kali /etc/rc.local"

#install lightweight TE
sudo docker exec -it kali_dev sh -c "apt install -y nano vim"

#Install ip tool to container
sudo docker exec -it kali_dev sh -c "apt install -y iproute2"

#Install init
sudo docker exec -it kali_dev sh -c "apt install -y init"

#Install fdisk
sudo docker exec -it kali_dev sh -c "apt install -y fdisk"

#Copy miniccc to container
sudo docker exec -it kali_dev sh -c "cp /os/bin/miniccc /bin/miniccc"

#Copy miniccc to container
sudo docker exec -it kali_dev sh -c "cp /os/services/miniccc.service /etc/systemd/system/miniccc.service"

#Create symbolic link to file in multi-user.target
sudo docker exec -it kali_dev sh -c "ln -s /etc/systemd/system/miniccc.service /etc/systemd/system/multi-user.target.wants/miniccc.service"

#Install linux-image-amd64 boot files
sudo docker exec -it kali_dev sh -c "apt install -y linux-image-amd64"

#create kali user and set pw
sudo docker exec -it kali_dev sh -c "adduser --gecos '' --disabled-password kali"
sudo docker exec -it kali_dev sh -c "echo 'kali:kali' | chpasswd"

#disable gnome-initial-setup
sudo docker exec -it kali_dev sh -c "mkdir /home/kali/.config"
sudo docker exec -it kali_dev sh -c "echo 'yes' >> /home/kali/.config/gnome-initial-setup-done"

#auto login enable
sudo docker exec -it kali_dev sh -c "sed -i 's/#  AutomaticLoginEnable = true/  AutomaticLoginEnable = true/g' /etc/gdm3/daemon.conf"
sudo docker exec -it kali_dev sh -c "sed -i 's/#  AutomaticLogin = user1/  AutomaticLogin = kali/g' /etc/gdm3/daemon.conf"

#Copy capstone folder to container
sudo docker exec -it kali_dev sh -c "cp -r /os/Capstone /home/kali/"
sudo docker exec -it kali_dev sh -c "cp /os/Capstone/MB.ef /usr/share/ettercap/"
sudo docker exec -it kali_dev sh -c "chown -R kali /home/kali/Capstone"

#Install pymodbus
sudo docker exec -it kali_dev sh -c "apt install -y python3-pip iputils-ping vim"
sudo docker exec -it kali_dev sh -c "pip install pymodbus --break-system-packages"
sudo docker exec -it kali_dev sh -c "pip install pymodbus"

#Commit docker container to image
sudo docker commit kali_dev kali_dev

#Create Image file
sudo docker exec -it kali_dev sh -c "dd if=/dev/zero of=/os/kali.img bs=1G seek=7 count=0"

#Then need to image the VM
sudo docker exec -it kali_dev sh -c "sfdisk /os/kali.img <<EOF
label: dos
label-id: 0x5d8b75fc
device: new.img
unit: sectors
kali.img1 : start=2048, type=83, bootable
EOF"
#last 7570905

#kali.img1 : start=4096, size=4190208, type=83, bootable
# size 4190208 is 2G need to leave enough space for container contents and generated content
#needs to be smaller than image size

#Get Container ID
CID=$(sudo docker run -d kali_dev /bin/true)

#Create tar file of container contents
sudo docker export -o ./tars/kali.tar ${CID}

# size 419028 is 2G need to leave enough space for container contents and generated content
# Create OFFSET
OFFSET=$(expr 512 \* 2048)

# Connect kali to loop device
# Need to check what loop devices are free
LOOP=$(losetup -f)
echo $LOOP

sudo losetup -o $OFFSET $LOOP ./kali.img #need to make sure the img path is right

#Format the loop device
sudo mkfs.ext4 $LOOP

#Mount the loop device to mnt dir
sudo mount -t auto $LOOP /mnt

#Empty container contents into mounted image
sudo tar -xvf ./tars/kali.tar -C /mnt

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
sudo dd if=/usr/lib/syslinux/mbr/mbr.bin of=./kali.img bs=440 count=1 conv=notrunc

sudo umount $LOOP
sudo losetup -d $LOOP

sudo docker container stop kali_dev
sudo docker container rm kali_dev
