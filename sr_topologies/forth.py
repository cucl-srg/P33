#!/usr/bin/python

'''
File:   forth.py
Date:   20 August MMXIII
Author: Matthew Ireland, University of Cambridge
        <mti20@cam.ac.uk>
Usage: python forth.py [--itable  <path/name of itable.conf to generate>]
                       [--switch] (degenerates topology to simple tree)

forth - the most simple of mininet topologies for the sr assignment.
      - one router, r0, with four attached hosts.
      
                                 App Server 0
                                 (h0, 10.0.1.2)
                                       ||
                                       ||
                                       ||
                      +=============(r0-eth0)=============+
                      |         Student's router (r0)     |
                      |         r0-eth0: 10.0.1.1         |
 App Server 1  ===== (r0-eth1)  r0-eth1: 10.0.2.1         |
(h1, 10.0.2.2)        |         r0-eth2: 10.0.3.1         |        
                      |                                   |
                      +=============(r0-eth2)=============+
                                       ||
                                       ||
                                       ||
                                 App Server 2
                                 (h0, 10.0.3.2)

The student's router code should run in r0. This file can be modified to give
different behaviour to h0, h1, h2 and h3 (by default they all ping the router
and each other twice).

Without the -s switch, ifaces/MACs/IPs:
    interface        MAC                      IP
    r0-eth0          00:00:00:00:01:01        10.0.1.1
    r0-eth1          00:00:00:00:01:02        10.0.2.1
    r0-eth2          00:00:00:00:01:03        10.0.3.1
    h0-eth0          00:00:00:00:00:01        10.0.1.2
    h1-eth0          00:00:00:00:00:02        10.0.2.2
    h2-eth0          00:00:00:00:00:03        10.0.3.2

Caveats:
   Don't just type 
       $ ping r0
   from a host, since r0 has four interfaces and this will just ping r0-eth0.
   
TODO input validation

Further development:
   Extract itable and rtable information either directly from mininet or
   create our own network data structure from which we can easily generate them
   and which we import to mininet automatically.

'''

__author__ = 'Matthew Ireland <mti20@cam.ac.uk>'
__version__ = '1.0'
__status__ = 'Development'

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.cli import CLI
from mininet.log import lg, info
from mininet.util import ensureRoot
from mode import Mode
from itable import Itable
#from mntools.mode import Mode
#from mntools.itable import Itable
from sys import argv
import argparse

class Forth ( Topo ):
    "Structure of our most simple router topology"
    
    def __init__ (self, mode=Mode.ROUTER):
        "Creates forth - the most simple 'sr' topology."
        
        # Initialise topology
        Topo.__init__(self)
        
        # Add the hosts
        if (mode == Mode.ROUTER):
            info( '*** Creating router\n')
            r0 = self.addHost( 'r0', ip='10.0.1.1')
        elif (mode == Mode.SWITCH):
            info( '*** Creating switch\n')
            r0 = self.addSwitch('s0')
        else:
            raise Exception('Mode unsupported in forth\n')
        
        info( '*** Creating hosts\n')        
        h0 = self.addHost( 'h0', ip='10.0.1.2')
        h1 = self.addHost( 'h1', ip='10.0.2.2')
        h2 = self.addHost( 'h2', ip='10.0.3.2')
                
        # Add the links
        info ('*** Creating links\n')
        self.addLink( r0, h0 )
        self.addLink( r0, h1 )
        self.addLink( r0, h2 )
                
# Populate the list of topologies (so mininet can be started with
#    $ sudo mn --custom ~/<path>/forth.py --topo forth
# if so desired (be careful with sys.path)
topos = { 'forth': ( lambda: Forth() ) }

if __name__ == '__main__':
    ensureRoot()
    
    # Parse command line arguments (to decide whether or not to generate an
    # itable file and whether to start in router or switch mode)
    parser = argparse.ArgumentParser( description = 'Forth sr topology')
    parser.add_argument( '-i', '--itable', default=None, \
                         help='generate itable.conf for sr assignment')
    parser.add_argument( '-s', '--switch', action='store_true', \
                         help="turn forth into a simple tree topology with four hosts and a switch")  # TODO split line
    args, unknown = parser.parse_known_args( argv )
    
    lg.setLogLevel( 'info' )
    
    # Create net
    if (not args.switch):
        forth = Forth()
    else:
        forth = Forth(Mode.SWITCH)
    net = Mininet(topo=forth)

    # Create itable.conf for student's r0 code, if requested
    if (args.itable is not None):
        info( '*** Creating itable for r0\n' )
        itable = Itable()
        itable.add('r0-eth0', '10.0.1.1', '00:00:00:00:01:01', '255.255.255.0') 
        itable.add('r0-eth1', '10.0.2.1', '00:00:00:00:01:02', '255.255.255.0') 
        itable.add('r0-eth2', '10.0.3.1', '00:00:00:00:01:03', '255.255.255.0') 
        # itable.add('r0-eth3', '10.0.4.1', '00:00:00:00:01:04', '255.255.255.0')
        itable.createFile(args.itable) 

    # Start mininet and configure each of the hosts
    # (order in list is alphabetical)
    info( '*** Starting mininet\n' )
    net.start()
    
    # Do router mode configuration
    if (not args.switch):
        r0, h0, h1, h2 = net.hosts[3], net.hosts[0], net.hosts[1], net.hosts[2] 
	
        # Configure r0 (let the student's code override the Linux network stack &
        #               set MAC addresses of interfaces)
        info( '*** Configuring router network stack\n' )
        for i in range(3):
            r0.cmd( 'ip addr flush dev r0-eth%d' % i )
            r0.cmd( 'ip -6 addr flush dev r0-eth%d' % i )
            iface_mac = '00:00:00:00:01:'+str(i+1)
            iface_name = 'r0-eth'+str(i)
            r0.intf(iface_name).setMAC( iface_mac )

        # Configure hosts (ifconfig + set r0 as gw)
        info( '*** Configuring host network stacks\n' )
        hosts = [h0, h1, h2]
        i=0
        for h in hosts:
            iface_name = 'h'+str(i)+'-eth0'
            host_mac = '00:00:00:00:00:0'+str(i+1)
            host_ip = '10.0.'+str(i+1)+'.2'
            router_ip = '10.0.'+str(i+1)+'.1'
            h.intf(iface_name).setMAC( host_mac )
            h.cmd('ifconfig %(iface)s %(ip)s netmask 255.255.255.255 broadcast 10.255.255.255'\
                  % {'iface': iface_name, 'ip': host_ip})
            h.cmd( 'route add -host %(ip)s dev %(iface)s' \
                   % {'ip': router_ip, 'iface': iface_name})
            h.cmd( 'route add default gw %(ip)s %(iface)s' \
                   % {'ip': router_ip, 'iface': iface_name})
            i = i+1
    
    # Give student a mininet prompt (so they can start xterms in each host and
    # their router code, etc)
    info( '*** Initialisation complete\n' )
    CLI( net )

    info( '*** Stopping mininet\n' )
    net.stop()
