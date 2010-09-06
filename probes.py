#!/usr/bin/python
import sys
import time
import threading
import random
sys.path.append('anemonclient')
sys.path.append('anemonclient/anemonclient')
#from anemonclient import packet_generator
#from anemonclient import sysconf
#from anemonclient import event_logger
#from anemonclient import dhcp_client
from pydhcplib import interface
from pydhcplib.type_ipv4 import ipv4
from pydhcplib.type_strlist import strlist



from pydhcplib.dhcp_packet import *
from pydhcplib.dhcp_network import *

dhcp_thread_instance = None

netopt = {'client_listen_port':68,
           'server_listen_port':67,
           'listen_address':"eth1"}

class DhcpProbe:

    def probe():
        receiver = dhcp_client.DhcpClientReceiver(sysconf.network_interface,
                                              sysconf.client_listen_port,
                                              sysconf.server_listen_port)
 
class IcmpProbe:
    
    def probe():
        print "Icmp Probe"

class Client(DhcpClient):
    def __init__(self, options):
        DhcpClient.__init__(self,options["listen_address"],
                            options["client_listen_port"],
                            options["server_listen_port"])

    def HandleDhcpOffer(self, packet):
        print packet.str()
    def HandleDhcpAck(self, packet):
        print packet.str()
    def HandleDhcpNack(self, packet):
        print packet.str()        

if __name__ == "__main__":
    client = Client(netopt)
     # Use BindToAddress if you want to emit/listen to an internet address (like 192.168.1.1)
     # or BindToDevice if you want to emit/listen to a network device (like eth0)
#client.BindToAddress()
#client.BindToAddress()
    client.BindToDevice()

    while True :
        print "next pack"
        client.GetNextDhcpPacket()

