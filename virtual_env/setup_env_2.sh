#!/bin/bash

cd /opt/minimega

sudo ./bin/miniweb &
sudo ./bin/minimega -base /tmp/minimega -force -nostdin &
sleep 3
sudo ./bin/minimega -e clear all
sudo ./bin/minimega -e read /opt/minimega/script_2.mm
