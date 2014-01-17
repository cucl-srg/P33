#!/usr/bin/python

'''
File: humber.py
Date: 20 August MMXIII
Author: Matthew Ireland, University of Cambridge
        <mti20@cam.ac.uk>

humber - import of the VNS PWOSPF topology
       - three routers arranged in a triangle
       - r0 is also connected to the internet
       - r1 and r2 have (different) application servers connected directly
         to them
         
TODO usage
TODO input validation
TODO check accuracy of routing table entries


                          eth0:10.0.2.2
                          +======================+         App Server 1
                          |  router #1       (eth1) ====== (h1, 10.0.4.2)
                          |  r1                  | eth1:10.0.4.1
                          +=====(eth0)=====(eth2)+
                                  /          ||    eth2:10.0.6.1
                                 /           ||
                                /            ||
                               /             ||  r0:
                +============(eth1)==+       ||    eth0: 10.0.1.1
App Server 0 = (eth0)  router #0     |       ||    eth1: 10.0.2.1
(h0, 10.0.1.2)  |      r0            |       ||    eth2: 10.0.3.1
                +============(eth2)==+       ||
                               \             ||
                                \            ||
                                 \           ||
                                  \          ||    eth2:10.0.6.2
                          +=====(eth0)=====(eth2)+
                          |  router #2           | eth1:10.0.5.1
                          |  r2              (eth1) ====== App Server 2
                          +======================+         (h2, 10.0.5.2)
                          eth0:10.0.3.2
                          
If started in degenerate switch mode, link r1 <--> r2 is omitted, so that h0
and h1 can still ping each other and access the internet with the default
controller.
                          
Without the -s switch, ifaces/MACs/IPs:
    interface        MAC                      IP
    r0-eth0          00:00:00:00:00:01        10.0.1.1
    r0-eth1          00:00:00:00:00:02        10.0.2.1
    r0-eth2          00:00:00:00:00:03        10.0.3.1
    r1-eth0          00:00:00:00:01:01        10.0.2.2
    r1-eth1          00:00:00:00:01:02        10.0.4.1
    r1-eth2          00:00:00:00:01:03        10.0.6.1
    r2-eth0          00:00:00:00:02:01        10.0.3.2
    r2-eth1          00:00:00:00:02:02        10.0.5.1
    r2-eth2          00:00:00:00:02:03        10.0.6.2
    h0-eth0          00:00:00:00:10:01        10.0.1.2
    h1-eth0          00:00:00:00:10:02        10.0.4.2
    h2-eth0          00:00:00:00:10:03        10.0.5.2
        

Further development:
   Extract itable and rtable information either directly from mininet or
   create our own network data structure from which we can easily generate them
   and which we import to mininet automatically.

'''

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.cli import CLI
from mininet.log import lg, info
from mininet.util import ensureRoot
from mode import Mode
from itable import Itable
from rtable import Rtable
from nat import connectToInternet, stopNAT
from sys import argv
import argparse

__author__ = 'Matthew Ireland <mti20@cam.ac.uk>'
__version__ = '1.0'
__status__ = 'Development'

class Humber ( Topo ):
    "Structure of the VNS PWOSPF topology"
    
    def __init__ (self, mode=Mode.ROUTER):
        "Creates the topology."
        
        # Initialise topology
        Topo.__init__(self)
        
        # Add the hosts
        
        # s0 = self.addSwitch( 's0' )
        if (mode == Mode.ROUTER):
            info( '*** Creating routers\n')
            r0 = self.addHost( 'r0', ip='10.0.1.1')   # ip of r0-eth0
            r1 = self.addHost( 'r1', ip='10.0.2.2')   # ip of r1-eth0
            r2 = self.addHost( 'r2', ip='10.0.3.2' )  # ip of r2-eth0
        elif (mode == Mode.SWITCH):
            info( '*** Creating switches\n')
            r0 = self.addSwitch( 's1' )
            r1 = self.addSwitch( 's2' )
            r2 = self.addSwitch( 's3' )
        else:
            raise Exception('Mode unsupported in humber\n')
            
        info('*** Creating hosts\n')
        h0 = self.addHost( 'h0', ip='10.0.1.2' )
        h1 = self.addHost( 'h1', ip='10.0.4.2' )
	h2 = self.addHost( 'h2', ip='10.0.5.2' )        

        # Add the links
        info ('*** Creating links\n')
        self.addLink( h0, r0 )      # r0 <==> s0 (NAT)
        self.addLink( r0, r1 )      # r0 <==> r1
        self.addLink( r0, r2 )      # r0 <==> r2
        self.addLink( r1, h1 )      # r1 <==> h0 (app server 1) 
        self.addLink( r2, h2 )      # r2 <==> h1 (app server 2)
        if (mode == Mode.ROUTER): 
            self.addLink( r1, r2 )  # r1 <==> r2 (complete the routing loop) 
        
# Populate the list of topologies (so mininet can be started with
#    $ sudo mn --custom ~/<path>/forth.py --topo forth
# if so desired
topos = { 'humber': ( lambda: Humber() ) }

if __name__ == '__main__':
    ensureRoot()
    
    # Parse command line arguments (to decide whether or not to generate an
    # itable file and whether to start in router or switch mode)
    parser = argparse.ArgumentParser( description = 'Humber sr topology')
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
    
    # Create net
    if (not args.switch):
        humber = Humber()
    else:
        humber = Humber(Mode.SWITCH)
    net = Mininet(topo=humber)

    # Create itable.conf for student's router code, if requested
    if (args.itable is not None):
        info( '*** Creating itable for r0\n' )
        r0_itable = Itable()
        r0_itable.add('r0-eth0', '10.0.1.1', '00:00:00:00:00:01 ', '255.255.255.0')
        r0_itable.add('r0-eth1', '10.0.2.1', '00:00:00:00:00:02 ', '255.255.255.0')
        r0_itable.add('r0-eth2', '10.0.3.1', '00:00:00:00:00:03 ', '255.255.255.0')
        r0_itable.createFile('r0-'+args.itable) 
        info( '*** Creating itable for r1\n' )
        r1_itable = Itable()
        r1_itable.add('r1-eth0', '10.0.2.2', '00:00:00:00:01:01 ', '255.255.255.0')
        r1_itable.add('r1-eth1', '10.0.4.1', '00:00:00:00:01:02 ', '255.255.255.0')
        r1_itable.add('r1-eth2', '10.0.6.1', '00:00:00:00:01:03 ', '255.255.255.0')
        r1_itable.createFile('r1-'+args.itable) 
        info( '*** Creating itable for r2\n' )
        r2_itable = Itable()
        r2_itable.add('r2-eth0', '10.0.3.2', '00:00:00:00:02:01 ', '255.255.255.0')
        r2_itable.add('r2-eth1', '10.0.5.1', '00:00:00:00:02:02 ', '255.255.255.0')
        r2_itable.add('r2-eth2', '10.0.6.2', '00:00:00:00:02:03 ', '255.255.255.0')
        r2_itable.createFile('r2-'+args.itable)
        
    # Create rtable.conf for each router, if requested
    # If --static switch is set, generate full entries, otherwise generate the
    # bare minimum and let pwospf do the rest
    if (args.rtable is not None):
        if ((args.extgw is None) and (not args.nointernet)):
            raise Exception('Connecting to internet, but extgw is not set. \
                             Cannot generate rtable.conf \
                             (If external router is using PWOSPF, then static \
                             rtable entry is not necessary.)')
        # r0
        r0_rtable = Rtable()
        if (args.static):
            info( '*** Creating full static rtable for r0\n')
            r0_rtable.add('10.0.2.0', '10.0.2.2', '255.255.255.0', 'r0-eth1')
            r0_rtable.add('10.0.3.0', '10.0.3.2', '255.255.255.0', 'r0-eth2')
            r0_rtable.add('10.0.4.0', '10.0.2.2', '255.255.255.0', 'r0-eth1')
            r0_rtable.add('10.0.5.0', '10.0.3.2', '255.255.255.0', 'r0-eth2')
            r0_rtable.add('10.0.6.1', '10.0.2.2', '255.255.255.255', 'r0-eth1')
            r0_rtable.add('10.0.6.2', '10.0.3.2', '255.255.255.255', 'r0-eth2')
        else:
            if (not args.nointernet):
                info( '*** Adding external gateway to rtable for r0\n')
        if (not args.nointernet):
            r0_rtable.add('0.0.0.0', args.extgw, '0.0.0.0', 'r0-eth0')   # default
        r0_rtable.createFile('r0-'+args.rtable)                          # route
        
        # r1
        r1_rtable = Rtable()
        if (args.static):
            info( '*** Creating full static rtable for r1\n')
            r1_rtable.add('0.0.0.0', '10.0.2.1', '0.0.0.0', 'r1-eth0')   # default route
            r1_rtable.add('10.0.1.0', '10.0.2.1', '255.255.255.0', 'r1-eth0')
            r1_rtable.add('10.0.2.0', '10.0.2.1', '255.255.255.0', 'r1-eth0')
            r1_rtable.add('10.0.3.0', '10.0.2.1', '255.255.255.0', 'r1-eth0')
            r1_rtable.add('10.0.5.0', '10.0.6.2', '255.255.255.0', 'r1-eth2')
            r1_rtable.add('10.0.6.2', '10.0.6.2', '255.255.255.255', 'r1-eth2')
        else:
            info( '*** Adding h0 to rtable for r1\n')
        r1_rtable.add('10.0.4.0', '10.0.4.2', '255.255.255.0', 'r1-eth1')
        r1_rtable.createFile('r1-'+args.rtable)
        
        # r2
        r2_rtable = Rtable()
        if (args.static):
            info( '*** Creating full static rtable for r2\n')
            r2_rtable.add('0.0.0.0', '10.0.3.1', '0.0.0.0', 'r2-eth0')   # default route
            r2_rtable.add('10.0.1.0', '10.0.3.1', '255.255.255.0', 'r2-eth0')
            r2_rtable.add('10.0.2.0', '10.0.3.1', '255.255.255.0', 'r2-eth0')
            r2_rtable.add('10.0.3.0', '10.0.3.1', '255.255.255.0', 'r2-eth0')
            r2_rtable.add('10.0.6.1', '10.0.6.1', '255.255.255.255', 'r2-eth2')
            r2_rtable.add('10.0.4.0', '10.0.6.1', '255.255.255.0', 'r2-eth2')
        else:
            info( '*** Adding h1 to rtable for r2\n')
        r2_rtable.add('10.0.5.0', '10.0.5.2', '255.255.255.0', 'r2-eth1')
        r2_rtable.createFile('r2-'+args.rtable)        

    # NAT to _the internet_
    #if (not args.nointernet):
    #    info( '*** Connecting to the internet (think dial-up modem sound)\n')
    #    rootnode = connectToInternet( net, switch='s0' )
    
    # Start mininet and configure each of the hosts
    # (order in list is alphabetical order)
    info( '*** Starting mininet\n' )
    
    # Do router mode configuration
    if (not args.switch):
        r0, r1, r2, h0, h1, h2 = net.hosts[3], net.hosts[4], \
                             net.hosts[5], net.hosts[0], net.hosts[1], net.hosts[2] 

        # Configure r0 (let the student's code override the Linux network stack &
        #               set MAC addresses of interfaces)
        info( '*** Configuring router network stacks\n' )
        j=0
        for r in [r0, r1, r2]:
            for i in range(3):
                r.cmd( 'ip addr flush dev r%(rid)d-eth%(ethid)d' \
                       % {'rid':j, 'ethid':i} )
                r.cmd( 'ip -6 addr flush dev r%(rid)d-eth%(ethid)d' \
                       % {'rid':j, 'ethid':i} )
                iface_mac = '00:00:00:00:0'+str(j)+':0'+str(i+1)
                iface_name = 'r'+str(j)+'-eth'+str(i)
                r.intf( iface_name ).setMAC( iface_mac )
            j=j+1
    
        # Configure hosts (ifconfig + set correct router as gw)
        info( '*** Configuring application server network stacks\n' )
        hosts = [h0, h1, h2]
        g=0
        for h in hosts:
	    if g == 0:
		iface_name = 'h'+str(g)+'-eth0'
                host_mac = '00:00:00:00:10:0'+str(g+1)
                host_ip = '10.0.'+str(g+1)+'.2'
                router_ip = '10.0.'+str(g+1)+'.1'
                h.intf(iface_name).setMAC( host_mac )
                h.cmd('ifconfig %(iface)s %(ip)s netmask 255.255.255.255 broadcast 10.255.255.255'\
                  % {'iface': iface_name, 'ip': host_ip})
                h.cmd( 'route add -host %(ip)s dev %(iface)s' \
                   % {'ip': router_ip, 'iface': iface_name})
                h.cmd( 'route add default gw %(ip)s %(iface)s' \
                   % {'ip': router_ip, 'iface': iface_name})
	    else:
                iface_name = 'h'+str(g)+'-eth0'
                host_mac = '00:00:00:00:10:0'+str(g+1)
                host_ip = '10.0.'+str(g+3)+'.2'
                router_ip = '10.0.'+str(g+3)+'.1'
                h.intf(iface_name).setMAC( host_mac )
                h.cmd('ifconfig %(iface)s %(ip)s netmask 255.255.255.255 broadcast 10.255.255.255'\
                  % {'iface': iface_name, 'ip': host_ip})
                h.cmd( 'route add -host %(ip)s dev %(iface)s' \
                   % {'ip': router_ip, 'iface': iface_name})
                h.cmd( 'route add default gw %(ip)s %(iface)s' \
                   % {'ip': router_ip, 'iface': iface_name})
            g=g+1

    # Give student a mininet prompt (so they can start xterms in each host and
    # their router code, etc)
    info( '*** Initialisation complete\n' )
    CLI( net )

    info( '*** Stopping mininet\n' )
    net.stop()
