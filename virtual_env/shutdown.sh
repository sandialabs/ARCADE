#!/bin/bash
#shutdown script
cd /opt/minimega
sudo service minimega stop
sudo ./bin/minimega -e clear all
sudo pkill miniweb
sudo pkill minimega
sudo fuser -k 9000/tcp

