#!/usr/bin/python

'''
File:   tyne.py
Date:   20 August MMXIII
Author: Matthew Ireland, University of Cambridge
        <mti20@cam.ac.uk>
Usage: python tyne.py [-i <path/name of itable.conf to generate>]
                      [-s] (used to replace r0 with s1, a switch)
                      [-n] (disables the internet connection)
                      
This is a direct import of the 'Simple' template (/template1/) in the old
Cambridge VNS system.
                                                            +--------------------+
                                                            |                    |
                                                            |      Web server 0  |
                                          10.0.2.1          |      Name: "h0"    |
 +---------------+       +---------------------+      /-----|eth0                |
 |               |       |                 eth1|-----/      +--------------------+
 |  Internet NAT |       |      Router         |           10.0.2.2
 |  Name: "s0"   |-------|eth0  Name: "r0"     |           10.0.3.2
 |               |       |                 eth2|-----\      +--------------------+
 +---------------+       +---------------------+      \-----|eth0                |
                        10.0.1.1         10.0.3.1           |      Web server 1  |
                                                            |      Name: "h1"    |
                                                            |                    |
                                                            +--------------------+

As inferred above, use the -s switch to replace r0 with a switch, so that you
can access the internet directly from the two hosts without running any router
code in r0.
'''


from mininet.net import Mininet
from mininet.topo import Topo
from mininet.cli import CLI
from mininet.log import lg, info
from mininet.util import ensureRoot
#from mntools.mode import Mode
from mode import Mode
from itable import Itable
from nat import connectToInternet, stopNAT
from sys import argv
import argparse


class Tyne ( Topo ):
    "Simplest CaMT that can connect to the internet with GG's NAT script"
    
    def __init__ (self, mode = Mode.ROUTER):
        "Creates the topology."
        
        # Initialise topology
        Topo.__init__(self)

	s0 = self.addSwitch( 's0' )
        info( '*** Creating router\n')
        r0 = self.addHost( 'r0', ip='10.0.1.1')   # ip of r0-eth0
        
        # TODO add the option of making this a switch
        
        info( '*** Creating hosts\n')
	h0 = self.addHost ( 'h0', ip='10.0.2.2')
	h1 = self.addHost ( 'h1', ip='10.0.3.2')

	# Add the links
        info('*** Creating links\n')
        self.addLink( s0, r0 )
	self.addLink( r0, h0 )
        self.addLink( r0, h1 )
        
        
# Populate the list of topologies (so mininet can be started with
#    $ sudo mn --custom ~/<path>/forth.py --topo forth
# if so desired
topos = { 'tyne': ( lambda: Tyne() ) }

if __name__ == '__main__':
    ensureRoot()

    # Parse command line arguments (to decide whether or not to generate an
    # itable file and whether to start in router or switch mode)
    parser = argparse.ArgumentParser( description = 'Tune sr topology')
    parser.add_argument( '-i', '--itable', default=None, \
                         help='generate itable.conf for each router')
    parser.add_argument( '-g', '--extgw', default=None, \
                         help='router to which the NAT is attached (if unknown, start in switch mode and traceroute from r0)')  # TODO split line
    parser.add_argument( '-r', '--rtable', default=None, \
                         help='generate rtable entries containing hosts and extgw')
    parser.add_argument( '--static', action='store_true', \
                         help='pwospf not employed, so generate full rtable.conf\'s for all routers')   # TODO split line
    parser.add_argument( '-s', '--switch', action='store_true', \
                         help="turn forth into a simple tree topology with four hosts and a switch")  # TODO split line
    parser.add_argument( '-n', '--nointernet', action='store_true', \
                         help="don't connect to the internet")
    args, unknown = parser.parse_known_args( argv )

    lg.setLogLevel( 'info' )
    tyne = Tyne()
    net = Mininet(topo=tyne)

    # NAT to _the internet_
    if (not args.nointernet):
        info( '*** Connecting to the internet (think dial-up modem sound)\n')
        rootnode = connectToInternet( net, switch='s0' )

    # Start mininet and configure each of the hosts
    # (order in list is alphabetical order)
    info( '*** Starting mininet\n' )
    
    # Do router mode configuration
    r0, h0, h1 = net.hosts[2], net.hosts[0], net.hosts[1] 

    # Configure r0 (let the student's code override the Linux network stack &
    #               set MAC addresses of interfaces)
    info( '*** Configuring router network stacks\n' )
    for i in range(3):
        r0.cmd( 'ip addr flush dev r0-eth%d' % i )
        r0.cmd( 'ip -6 addr flush dev r0-eth%d' % i )
        iface_mac = '00:00:00:00:01:'+str(i+1)
        iface_name = 'r0-eth'+str(i)
        r0.intf( iface_name ).setMAC( iface_mac )
    
    # Configure hosts (ifconfig + set correct router as gw)
    info( '*** Configuring application server network stacks\n' )
    hosts = [h0, h1]
    i=0
    for h in hosts:
        iface_name = 'h'+str(i)+'-eth0'
        host_mac = '00:00:00:00:00:0'+str(i+1)
        host_ip = '10.0.'+str(i+2)+'.2'
        router_ip = '10.0.'+str(i+2)+'.1'
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

    info( '*** Stopping net\n' )
    if (not args.nointernet):
        stopNAT( rootnode )
    net.stop()

