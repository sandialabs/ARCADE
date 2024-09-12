import sys
sys.path.insert(0, '/home/kali/Capstone')
import ManiPIO

test = ManiPIO.MB_PLC("20.0.1.10", 502)
res = test.read(2054)

with open('/tmp/miniccc/files/result.txt', 'w') as f:
	f.write(str(res))
