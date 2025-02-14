clear vm config
vm config kernel /opt/minimega/images/minirouter.kernel
vm config initrd /opt/minimega/images/minirouter.initrd
vm config snapshot true
vm config net HMI
vm launch kvm router
vm start router

vm config net MGT
vm launch kvm MGTR
vm start MGTR

router router interface 0 20.0.1.1/24
router router dhcp 20.0.1.1 range 20.0.1.2 20.0.1.254
router router commit

tap create HMI ip 20.0.1.3/24 nat0

router MGTR interface 0 30.0.1.1/24
router MGTR dhcp 30.0.1.1 range 30.0.1.2 30.0.1.254
router MGTR commit

clear vm config
vm config disk /opt/minimega/images/openplc.img
vm config vcpus 2
vm config memory 4096
vm config net HMI MGT
vm launch kvm SG_PLC
vm launch kvm RCP_PLC
vm config disk /opt/minimega/images/asherah.img
vm config net MGT
vm launch kvm SIM

vm config disk /opt/minimega/images/scada.img
vm config net HMI
vm launch kvm SCADA

vm config disk /opt/minimega/images/kali.img 
vm config vcpus 4
vm config memory 8192
vm launch kvm ATTACK

vm start all

cc filter name=SIM
cc exec ip addr add 30.0.1.5/24 dev ens1
cc filter name=RCP_PLC
cc exec ip addr add 30.0.1.10/24 dev ens2
cc exec ip addr add 20.0.1.10/24 dev ens1
cc filter name=SG_PLC
cc exec ip addr add 30.0.1.15/24 dev ens2
cc exec ip addr add 20.0.1.15/24 dev ens1
cc filter name=SCADA
cc exec ip addr add 20.0.1.50/24 dev ens1
cc filter name=ATTACK
cc exec ip addr flush dev eth0
cc exec ip addr add 20.0.1.120/24 dev eth0
cc exec ip link set eth0 up
cc exec service NetworkManager stop

clear cc filter
cc exec ip link set ens1 down
cc exec ip link set ens1 up
cc exec ip link set ens2 down
cc exec ip link set ens2 up

cc filter name=SIM
cc exec ip route add default via 30.0.1.1 dev ens1

cc filter name=RCP_PLC
cc exec ip route add default via 30.0.1.1 dev ens2
cc exec bash /workdir/custom_plc_setup.sh RCP_PLC.st

cc filter name=SG_PLC
cc exec ip route add default via 30.0.1.1 dev ens2
cc exec bash /workdir/custom_plc_setup.sh SG_P_PLC.st

cc filter name=ATTACK
cc exec su - kali -c 'export DISPLAY=:0 && gnome-terminal -- xrandr --output Virtual-1 --mode 1920x1080 && gnome-terminal'

