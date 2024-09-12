#Modify maniPio script
def modify(value1, value2, value3):
        valueList = [value1, value2, value3]
        with open ("/home/tagray/mini-python/files/manipScript.txt", 'r') as file:
                lines = file.readlines()
        i = 0
        for j in range(len(lines)):
                if "values" in lines[j]:
                        lines[j] = "values:" + str(valueList[i]) + "\n"
                        i += 1

        with open ("/home/tagray/mini-python/files/manipScript.txt", 'w') as file:
                file.writelines(lines)


