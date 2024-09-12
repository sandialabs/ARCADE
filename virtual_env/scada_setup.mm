clear vm config
vm config kernel /opt/minimega/images/minirouter.kernel
vm config initrd /opt/minimega/images/minirouter.initrd
vm config snapshot true
vm config net HMI
vm launch kvm router
vm start router

router router interface 0 20.0.1.1/24
router router dhcp 20.0.1.1 range 20.0.1.2 20.0.1.254
router router commit

tap create HMI ip 20.0.1.3/24 nat0

clear vm config
vm config snapshot false
vm config vcpus 2
vm config memory 4096
vm config disk /opt/minimega/images/scada.img
vm config net HMI
vm launch kvm SCADA

vm config disk /opt/minimega/images/scada_2.img
vm config net HMI
vm launch kvm SCADA_2

vm start all

cc filter name=SCADA
cc exec ip addr add 20.0.1.50/24 dev ens1
cc exec ip link set ens1 down
cc exec ip link set ens1 up

cc filter name=SCADA_2
cc exec ip addr add 20.0.1.51/24 dev ens1
cc exec ip link set ens1 down
cc exec ip link set ens1 up