#!/usr/bin/env/ python3
from selenium import webdriver
from selenium.webdriver.firefox.service import Service as FirefoxService
from webdriver_manager.firefox import GeckoDriverManager
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
import time
import os, inspect
import argparse
from argparse import ArgumentParser

#check if file passed in argparse exists or not
def extant_file(f):
    if not os.path.isfile(f):
        raise argparse.ArgumentTypeError("{0} file does not exist!".format(f))
    return f

def Safe_Click(clickable):
    click_wait=False
    while click_wait==False:
        try:
            clickable.click()
            click_wait=True
        except:
            time.sleep(0.5)


def Web_Auto(IP, File, Name):
    #Find out where I am, what my name is, and assume I'm with the Geckodriver
    File_path = __file__
    path = os.path.dirname(File_path)
    service = FirefoxService(executable_path=path+"/geckodriver")
    fireFoxOptions = webdriver.FirefoxOptions()
    fireFoxOptions.add_argument("--headless")
    driver = webdriver.Firefox(service=service, options=fireFoxOptions)
    driver.set_page_load_timeout(15)
    driver.implicitly_wait(10)

    def Safe_Get(URL):
        finished = False
        while finished == False:
            try:
                driver.get(URL)
                finished = True
            except:
                time.sleep(1)
 
    #get the login page loop if infinite load happens
    Safe_Get("http://"+IP+":8080")


    #input username and password (Default for OpenPLC)
    username = driver.find_element(By.NAME, "username")
    username.send_keys("openplc")
    password = driver.find_element(By.NAME, "password")
    password.send_keys("openplc")
    login = driver.find_element(By.TAG_NAME, "button")

    #Click the login in a safe way
    Safe_Click(login)

    #load the programs page
    Safe_Get("http://"+IP+":8080/programs")

    #set file path and upload
    files = driver.find_element(By.ID, "file")
    files.send_keys(File)
    upload = driver.find_element(By.NAME, "submit")

    Safe_Click(upload)

    #setup program name and submit
    prog_name = driver.find_element(By.ID, "prog_name")
    prog_name.send_keys(Name)
    upload_prog = driver.find_element(By.CSS_SELECTOR, ".button[type='submit']")

    Safe_Click(upload_prog)
    
    time.sleep(2)

    #wait to return to dashboard
    ret_to_dash = driver.find_element(By.ID, "dashboard_button")

    Safe_Click(ret_to_dash)
    
    time.sleep(2)
    
    #start the PLC
    start_plc = driver.find_element(By.LINK_TEXT, "Start PLC")

    Safe_Click(start_plc)

    driver.refresh()
    

if __name__ == "__main__":
    #parse arguments
    parser = ArgumentParser(description="Web clint to automate OpenPLC startup")
    parser.add_argument("-st", dest="st_file", required=True, type=extant_file, help=".st input file for PLC",metavar="FILE")
    parser.add_argument("-n", "--name", dest="name", required=False, type=str, help="Name for PLC",default="WishIHadAName")
    parser.add_argument("-ip", dest="ip", required=False, type=str, help="IP of PLC",default="localhost")
    
    args = parser.parse_args()
    print("file: {0}".format(args.st_file))
    print("Name: {0}".format(args.name))
    print("IP: {0}".format(args.ip))

    Web_Auto(args.ip, args.st_file, args.name)
    
    
    
