whoami > /tmp/whoami.txt
echo $RANDOM >> /tmp/whoami.txt
su - root -c 'python3 /home/kali/Capstone/ManiPIO.py /tmp/miniccc/files/manipScript.txt &'
