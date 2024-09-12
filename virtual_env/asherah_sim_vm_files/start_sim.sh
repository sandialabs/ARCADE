#!/bin/bash
cd /opt/DB
python3 /opt/DB/EndPoint.py &> /opt/DB/EndPoint.log &
/opt/DB/DB &> /dev/null
