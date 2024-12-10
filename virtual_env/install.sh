#!/bin/bash

# Copyright 2024 National Technology & Engineering Solutions of Sandia, LLC (NTESS). 
# Under the terms of Contract DE-NA0003525 with NTESS, the U.S. Government retains 
# certain rights in this software.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

#edit paths in minimega script
mm_path="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
#sed -i "s+/home/usp/INS_MM+$mm_path+g" script_BR.mm

#record system info for debug/support if needed
hostnamectl &> system_info.txt
sudo lshw &>> system_info.txt

#update systems and install dependancies
sudo apt update
sudo apt upgrade -y
sudo apt install -y qemu-kvm libvirt-daemon-system libvirt-clients bridge-utils extlinux debootstrap genisoimage
sudo apt install -y openvswitch-switch qemu-kvm qemu-utils dnsmasq ntfs-3g iproute2 curl libpcap-dev vim htop dbus-x11 wget git gcc wireshark

#check if Go 1.21.13 is installed
GoFlag=0
if [ -f /usr/local/go/bin/go ]; then
    export PATH=$PATH:/usr/local/go/bin
    if go version | grep -q "go1.21.13"; then
            echo "Go is correct version"
            GoFlag=1
    fi
fi
if [ $GoFlag = 0 ]; then
   echo "Removing any old go packages"
   sudo rm -r /usr/local/go
   echo "Installing GoLang V1.21.13"
   wget https://go.dev/dl/go1.21.13.linux-amd64.tar.gz
   sudo tar -C /usr/local -xzf go1.21.13.linux-amd64.tar.gz
   sudo rm go1.21.13.linux-amd64.tar.gz
   export PATH=$PATH:/usr/local/go/bin
fi

#check if Minimega is installed
MiniMegaFlag=0
if [ -f /opt/minimega/bin/minimega ]; then
   read -r -p "Minimega is already installed. Do you want to reinstall it? This will delete the /opt/minimega folder! [y/N] " response
   response=${response,,}    # tolower
   if [[ "$response" =~ ^(yes|y)$ ]]; then
           MiniMegaFlag=1
   fi
else
   MiniMegaFlag=1
   echo "MiniMega will be installed."
fi

if [ $MiniMegaFlag = 1 ]; then 
   #remove old minimega build if present
   if [ -d minimega ]; then
      sudo rm -r minimega
   fi
   git clone https://github.com/sandia-minimega/minimega.git
   cd minimega/scripts/
   bash ./all.bash
   cd ../..
   sudo mv minimega/ /opt/minimega 
fi

#make sure that permissions are correct for rc.local
sudo chmod ugo+x rc.local/*

#copy install files
for folder in asherah_sim_vm_files Capstone DB openplc_vm_files rc.local scada services; do sudo cp -r $mm_path/$folder /opt/minimega/; done
for files in auto_install* script* scada_setup.mm sim_fix.mm reset_simulator.sh setup_*; do sudo cp -r $mm_path/$files /opt/minimega/; done

#Make dirs if they dont exist
if [ ! -d /opt/minimega/tars ]; then
   sudo mkdir /opt/minimega/tars ;
fi

if [ ! -d /opt/minimega/images ]; then
   sudo mkdir /opt/minimega/images ;
fi

#check for docker and install if not there
if [[ $(which docker) && $(docker --version) ]]; then
   echo "Docker already installed"
else
   echo "Installing Docker"

   #install docker
   # remove any unofficial packages
   for pkg in docker.io docker-doc docker-compose podman-docker containerd runc; do sudo apt remove $pkg; done

   #install certs and keys
   sudo apt install -y ca-certificates curl gnupg

   sudo install -m 0755 -d /etc/apt/keyrings
   curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
   sudo chmod a+r /etc/apt/keyrings/docker.gpg

   echo \
   "deb [arch="$(dpkg --print-architecture)" signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
   "$(. /etc/os-release && echo "$VERSION_CODENAME")" stable" | \
   sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

   #update apt and install
   sudo apt update

   sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin ;
fi

echo "clean up Docker images (optional)"
sudo docker system prune

#start making VMs
echo "Making VMs"

#get into the right dir
cd /opt/minimega

#make the router image
if [ ! -f /opt/minimega/images/minirouter.kernel ]; then
   sudo ./bin/vmbetter -mirror "http://ftp.us.debian.org/debian/" -branch buster -level debug misc/vmbetter_configs/minirouter.conf
   sudo mv minirouter.* images/ ;
fi

#make the scripts executable
sudo chmod +x ./*.sh

#Make the images
if [ ! -f /opt/minimega/images/openplc.img ]; then
   sudo ./auto_install_openplc.sh
   sudo mv openplc.img images/ ;
fi

if [ ! -f /opt/minimega/images/asherah.img ]; then
   sudo ./auto_install_asherah.sh
   sudo mv asherah.img images/ ;
fi

if [ ! -f /opt/minimega/images/scada.img ]; then
   sudo ./auto_install_scada.sh
   sudo mv scada.img images/ 
   sudo cp images/scada.img images/scada_2.img ;
fi

if [ ! -f /opt/minimega/images/kali.img ]; then
   read -r -p "Install full Kali Linux instead of limited version? Requires +42Gb. [y/N] " response
   response=${response,,}    # tolower
   if [[ "$response" =~ ^(yes|y)$ ]]; then
        sudo ./auto_install_kali_all_gui.sh
        sudo mv kali.img images/ ;
   else
        sudo ./auto_install_kali_gui.sh
        sudo mv kali.img images/ ;
   fi   
fi

echo "cleaning up"
for folder in asherah_sim_vm_files Capstone DB openplc_vm_files rc.local scada services tars auto_install* mysql*; do sudo rm -r /opt/minimega/$folder; done

echo "Done"
