#!/usr/bin/python

'''
File: itable.py
Date: 20 August MMXIII
Author: Matthew Ireland, University of Cambridge
        <mti20@cam.ac.uk>

Used for creating an itable configuration file for a student's router.
'''

class Interface:
    __name = None
    __ip = None
    __mac = None
    __netmask = None
    
    def __init__(self, name, ip, mac, netmask):
        self.__name = name
        self.__ip = ip
        self.__mac = mac
        self.__netmask = netmask
        
    def toString(self):
        iface_str = self.__name+" "+self.__ip+" "+self.__netmask+" "+self.__mac
        return iface_str

class Itable:
    def addInterface(self, name, ip, mac, netmask):
        iface = Interface(name, ip, mac, netmask)
        try:
            self.__interfaces.append(iface)
        except AttributeError:
            self.__interfaces = [iface]
            
    def add(self, name, ip, mac, netmask):
        self.addInterface(name, ip, mac, netmask)
            
    def createFile(self, filename):
        "Caution: contents of filename will be overwritten without warning"
        
        f = open(filename, 'w')
        for iface in self.__interfaces:
            f.write(iface.toString()+"\n")
        f.close()
