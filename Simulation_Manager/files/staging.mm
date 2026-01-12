cc filter name=flownex
cc exec powershell -c 'echo hello > C:\\tmp\\miniccc\\files\\test.txt'
cc exec powershell -c 'echo hello > C:\\Users\\<USERNAME>\\Desktop\\Release\\up_data.csv'
cc exec powershell -c 'echo hello > C:\\Users\\<USERNAME>\\Desktop\\Release\\pub_data.csv'
cc exec powershell -c 'powershell -c Set-ExecutionPolicy Bypass; powershell -c C:\\tmp\\miniccc\\files\\staging.ps1'