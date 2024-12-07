# ARCADE Virtual Environment

This installer will setup Minimega and produce generic virtual machines which are useful for ARCADE. The scripts included are examples of environments which use the Asherah NPP simulator (not included). To recieve the Asherah executable for this environment please obtain a license from the IAEA for the simulink program and contact the developers of ARCADE.

## INSTALLING

**Tested on Ubuntu 20.04, 22.04, and 24.04**

Open a terminal in the environment folderse and run the following:

    $ chmod ugo+x *.sh
    $ ./install.sh

Look for prompts during install.
- Reinstalling minimega will delete all the VM images,
- Docker clearing is optional if a VM is not building correctly more than once.
- When asked to set a password for OMySQL set it to 'root', you will be asked 3 times for this password. They must all be **root**.

After the install run `setup_env_#.sh` when you want to run the environment. 

To shut down the envirionment run `shutdown.sh`

All VM passwords are `root:root`

## Configure Scada LTS
To configure the Scada LTS server to boot with a config, use the `setup_scada.sh` and configure the Scada LTS via the browser (http://20.0.1.50:8080/Scada-LTS)

Password for Scada web interfaces is `admin:admin`

Load the HMI_1.zip for http://20.0.1.50:8080/Scada-LTS

Load the HMI_2.zip for http://20.0.1.51:8080/Scada-LTS

When complete, safely shut down the Scada server by logging in to the Scada image's CLI from the minimega web server (http://localhost:9001)

The username and password for Scada VM CLI is `root:root`

On the VM command line execute: `shutdown 0`

Now shutdown minimega using `shutdown.sh` from the host CLI

## Troubleshooting

- 7 VM's should start with `setup_env_1.sh` and `setup_env_2.sh`
- 8 VM's should start with `setup_env_2.sh`

If a VM is broken, the typical proceedure should be to delete that image and run the installer again. The installer checks for the VM images and will only build the missing ones.

    $ sudo rm /opt/minimega/images/[image_name].img
    $ ./install.sh

Do not reinstall minimega unless you are specifically having issues with minimega.