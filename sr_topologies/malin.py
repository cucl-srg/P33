'''
File:   malin.py
Date:   6 September MMXIII
Author: Matthew Ireland, University of Cambridge
        <mti20@cam.ac.uk>

malin - designed for use in part 1 of the TCP window sizing practical
        (flow control)
      - simple tree topology, but with asymmetric links
      - TODO more description
      

TODO usage
TODO actually make the links asymmetric
TODO input validation
TODO make links configurable (e.g. scaling factor)

The only difference between
  $ sudo mn --topo=tree
and
  $ sudo mn -custom ~/<path>/malin.py --topo malin
is the fact that the latter will introduce links with asymmetric upload and
download speeds and use a slightly different IP assignment/naming scheme.


                       eth0:10.0.1.1  
                       +======================+   Processes received data  
                      (h0-eth0)  server       |   at a variable rate
                     / |         h0           |   High-speed link to switch  
                    /  +======================+   
                   /   
                  /   Uplink:   1Gbps
                 /    Downlink: 1Gbps 
                /      
 +=========(s0-eth0)==+
 |  switch            |
 |  s0                |
 +=========(s0-eth1)==+
                \     
                 \    Uplink:   10Mbps (TODO make this 1)
                  \   Downlink: 10Mbps
                   \
                    \ 
                     \ +======================+   Sends data to server to
                      (h1-eth0)  client       |   be processed. Reasonable 
                       |         h1           |   downlink, slow uplink.
                       +======================+     
                       eth1:10.0.1.2  
'''

from mininet.net import Mininet
from mininet.topo import Topo
from mininet.cli import CLI
from mininet.log import lg, info
from mininet.util import ensureRoot

class Malin ( Topo ):
    "Tree topology with asymmetric links for the TCP buffer sizing practical"
    
    def __init__ (self):
        "Creates malin - the simple tree topology with asymmetric links."
        
        # Initialise topology
        Topo.__init__(self)
        
        # Add the hosts
        info( '*** Creating switch\n')
        s0 = self.addSwitch( 's0' )

        info( '*** Creating hosts\n')        
        h0 = self.addHost( 'h0', ip='10.0.1.1')
        h1 = self.addHost( 'h1', ip='10.0.2.2')

        # Add the links
        info ('*** Creating links\n')
        #linkopts_h0 = dict(bw=1000, delay='2ms')
        #linkopts_h1 = dict(bw=10, delay='2ms')
        linkopts_h0 = dict(bw=1000)
        linkopts_h1 = dict(bw=10)
        self.addLink( s0, h0, 's0-eth0', 'h0-eth0', **linkopts_h0 )
        self.addLink( s0, h1, 's0-eth1', 'h1-eth0', **linkopts_h1 )

# Populate the list of topologies
topos = { 'malin': ( lambda: Malin() ) }

if __name__ == '__main__':
    ensureRoot()
    
    lg.setLogLevel( 'info' )
    
    # Create net
    malin = Malin()
    net = Mininet(topo=malin)

    info( '*** Starting mininet\n' )
    net.start()
    
    info( '*** Initialisation complete\n' )
    CLI( net )

    info( '*** Stopping mininet\n' )
    net.stop()
