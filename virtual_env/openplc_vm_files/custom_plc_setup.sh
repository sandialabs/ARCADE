#!/bin/bash
cd /workdir/webserver/scripts
cp /workdir/webserver/custom_st_files/* /workdir/webserver/st_files
bash compile_program.sh "$1"

sed -i 's/Blank Program/PLC program/g'  /workdir/webserver/check_openplc_db.py
sed -i 's/Dummy empty program/PLC Program/g' /workdir/webserver/check_openplc_db.py
sed -i "s/blank_program.st/$1/g" /workdir/webserver/check_openplc_db.py
sed -i "s/checkSettingExists(conn, 'Start_run_mode', 'false')/checkSettingExists(conn, 'Start_run_mode', 'true')/g" /workdir/webserver/check_openplc_db.py

cd /workdir/webserver/
rm -rf /workdir/webserver/build
python3 check_openplc_db.py
cp /workdir/webserver/build/openplc.db /workdir/webserver

cd /workdir
bash /workdir/start_openplc.sh &
