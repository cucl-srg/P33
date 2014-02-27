#!/usr/bin/python

'''
File: ouse.py
Author: Georgina Kalogeridou University of Cambridge


                                 
                                 +======================+         App Server 1	    r1:
                                 |  router #1       (eth1) ====== (h1, 10.0.8.2)      eth0: 10.0.2.2
                                 |  r1                  | 			      eth1: 10.0.8.1
 r0:                             +=(eth0)=(eth3)=(eth2)=+			      eth2: 10.0.5.1
   eth0: 10.0.1.1                   /       |        \  			      eth3: 10.0.7.1
   eth1: 10.0.2.1                  /        |         \
   eth2: 10.0.3.1                 /         |          \
   eth3: 10.0.4.1                /          |           \
                +============(eth1)==+      |        +==(eth0)============+      
App Server 0 = (eth0) router #0  (eth3)--------------(eth3) router #3 (eth2) ====== App Server 3    
(h0, 10.0.1.2)  |     r0             |      |        |      r3  	  |	   (h3, 10.0.10.2)
                +============(eth2)==+      |        +==(eth1)============+  	
                                 \          |           /			    r3:
                                  \         |          /			      eth0: 10.0.5.2
                                   \        |         /				      eth1: 10.0.6.1
                                    \       |        /       			      eth2: 10.0.10.1
             r1:                 +=(eth0)=(eth3)=(eth2)=+			      eth3: 10.0.4.2
               eth0: 10.0.3.2    |  router #2           | 		
               eth1: 10.0.9.1    |  r2              (eth1) ====== App Server 2
               eth2: 10.0.6.2    +======================+         (h2, 10.0.9.2)
               eth3: 10.0.7.2                  
                          
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

class Ouse ( Topo ):
     
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
            r2 = self.addHost( 'r2', ip='10.0.3.2')  # ip of r2-eth0
	    r3 = self.addHost( 'r3', ip='10.0.5.2')   # ip of r3-eth0
	elif (mode == Mode.SWITCH):
            info( '*** Creating switches\n')
            r0 = self.addSwitch( 's1' )
            r1 = self.addSwitch( 's2' )
            r2 = self.addSwitch( 's3' )
        else:
            raise Exception('Mode unsupported in ouse\n')
            
        info('*** Creating hosts\n')
        h0 = self.addHost( 'h0', ip='10.0.1.2' )
        h1 = self.addHost( 'h1', ip='10.0.8.2' )
	h2 = self.addHost( 'h2', ip='10.0.9.2' ) 
	h3 = self.addHost( 'h3', ip='10.0.10.2' )
	
        # Add the links
        info ('*** Creating links\n')
        self.addLink( h0, r0 )      
        self.addLink( r0, r1 )      # r0 <==> r1
        self.addLink( r0, r2 )      # r0 <==> r2
        self.addLink( r1, h1 )      # r1 <==> h1 (app server 1) 
        self.addLink( r2, h2 )      # r2 <==> h2 (app server 2)
        self.addLink( r1, r3 )      # r1 <==> r3  
	self.addLink( r2, r3 )
	self.addLink( h3, r3 ) 
	self.addLink( r0, r3 )
	self.addLink( r1, r2 )       
	
# Populate the list of topologies (so mininet can be started with
#    $ sudo mn --custom ~/<path>/forth.py --topo forth
# if so desired
topos = { 'ouse': ( lambda: Ouse() ) }

if __name__ == '__main__':
    ensureRoot()
    
    # Parse command line arguments (to decide whether or not to generate an
    # itable file and whether to start in router or switch mode)
    parser = argparse.ArgumentParser( description = 'Ouse sr topology')
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
        ouse = Ouse()
    else:
        ouse = Ouse(Mode.SWITCH)
    net = Mininet(topo=ouse)

    # NAT to _the internet_
    #if (not args.nointernet):
    #    info( '*** Connecting to the internet (think dial-up modem sound)\n')
    #    rootnode = connectToInternet( net, switch='s0' )
    
    # Start mininet and configure each of the hosts
    # (order in list is alphabetical order)
    info( '*** Starting mininet\n' )
    
    # Do router mode configuration
    if (not args.switch):
        r0, r1, r2, r3, h0, h1, h2, h3 = net.hosts[4], net.hosts[5], net.hosts[6], net.hosts[7], net.hosts[0], net.hosts[1], net.hosts[2], net.hosts[3] 

        # Configure r0 (let the student's code override the Linux network stack &
        #               set MAC addresses of interfaces)
        info( '*** Configuring router network stacks\n' )
        j=0
        for r in [r0, r1, r2, r3]:
            for i in range(4):
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
        hosts = [h0, h1, h2, h3]
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
                host_ip = '10.0.'+str(g+7)+'.2'
                router_ip = '10.0.'+str(g+7)+'.1'
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
