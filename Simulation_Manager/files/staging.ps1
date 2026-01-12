$UID = (Get-Content C:\tmp\uid.txt -Raw).Trim()
cp C:\tmp\miniccc\files\test.txt "C:\tmp\miniccc\files\${UID}_test.txt"
cp C:\Users\<USERNAME>\Desktop\Release\pub_data.csv "C:\tmp\miniccc\files\${UID}_pub_data.csv"
cp C:\Users\<USERNAME>\Desktop\Release\up_data.csv "C:\tmp\miniccc\files\${UID}_up_data.csv"
