#!/usr/bin/python

'''
File: rtable.py
Date: 20 August MMXIII
Author: Matthew Ireland, University of Cambridge
        <mti20@cam.ac.uk>

Used for creating an rtable configuration file for a student's router.
'''

class RoutingEntry:
    __prefix = None
    __nexthop = None
    __netmask = None
    __iface = None
    
    def __init__(self, prefix, nexthop, netmask, iface):
        self.__prefix = prefix
        self.__nexthop = nexthop
        self.__netmask = netmask
        self.__iface = iface
        
    def toString(self):
        iface_str = self.__prefix+" "+self.__nexthop+" "+self.__netmask+" "+self.__iface
        return iface_str

class Rtable:
    def addRoutingEntry(self, prefix, nexthop, netmask, iface):
        entry = RoutingEntry(prefix, nexthop, netmask, iface)
        try:
            self.__entries.append(entry)
        except AttributeError:
            self.__entries = [entry]
            
    def add(self, prefix, nexthop, netmask, iface):
        self.addRoutingEntry(prefix, nexthop, netmask, iface)
            
    def createFile(self, filename):
        "Caution: contents of filename will be overwritten without warning"
        f = open(filename, 'w')
        for entry in self.__entries:
            f.write(entry.toString()+"\n")
        f.close()
