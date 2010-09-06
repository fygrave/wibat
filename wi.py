#!/usr/bin/env python

import sys
import string
sys.path.append('pyiw')
import time
import pyiw


class WiBruter:
    def __init__(self, iface=""):
        self.probe = None
        self.dict = []
        self.essids = []
        self.essid = ""
        self.interface = iface
        self.cmds = [["brute", self.brute, 1, "help message"]
        ]

    def interface(self, iface):
        self.interface = iface

    def registerProbe (self, probe):
        self.probe = probe

    def addDictionary(self, dfile):
        self.dict.append(dfile)

    def getResponse(self):

     try:
        if sys.stdin.isatty():
            return raw_input('>')
        else:
            if sys.platform[:3] == 'win':
                import msvcrt
                msvcrt.putch('>')
                key = msvcrt.getche()
                msvcrt.putch('\n')
                return key
            elif sys.platform[:5] == 'linux':
                print '>'
                console = open('/dev/tty')
                line = console.readline()[:-1]
                return line
            else:
                print '[pause]'
                import time
                time.sleep(5)
                return 'y'
     except:
         return 'done'


    def brute(self, net):
        print "launching brutez"

    def cmdp(self):
        print "\nEntor yer commend or help for umm.. ;-)"
        done = 0
        while not done:
            rez = self.getResponse()
            try:
                cmds = string.split(rez)
                cmd = cmds[0]
                if (cmd == 'help'):
                    print "available commands: scan, list, brute, dictionary, done, interface and help"
                elif (cmd == 'scan'):
                    self.scan()
                elif (cmd == 'list'):
                    self.list()
                elif (cmd == 'interface'):
                    self.interface = cmds[1]
                    self.init()
                elif (cmd == 'brute'):
                    self.brute(cmds[1])
                elif (cmd == 'clear'):
                    self.clear()
                elif (cmd == 'dictonary'):
                    self.dictonary(cmds[1])
                elif(cmd == 'done'):
                    done = 1
                else:
                    print "use help, droid!"
            except:
                print "woops!"

    def start(self):
        self.init()
        self.cmdp()
        print "bye"

    def init(self):
        if self.interface == "":
            print "No interface set\n"
            return
	print "Using device:", self.interface, "\n"
	print "Version [ PyIW      ]:", pyiw.version()
	print "Version [ libiw/WE  ]:", pyiw.iw_version()
	print "Version [ kernel/WE ]:", pyiw.we_version(), "\n"
	try:
		self.iface = pyiw.WirelessInterface(self.interface)
		for val in self.iface:
                    print "%-9s = %s" % (val, self.iface[val])
                self.essid = self.iface["essid"]

	except pyiw.error, error:
		print "Error:", error
		sys.exit(1)

    def clear(self):
        self.essids = []
    def scan(self):
	print "\nScanning...\n"

	for net in self.iface.Scan():
                self.essids.append(net)

        print "Discovered Networks:"
        counter = 0
        print "Open Networks"
        for n in self.essids:
            if n["wpa"] == None and n["wep"] == False:
		print "[%i] %s AP[%s] Quality[%s] Channel[%s]"% (\
                counter, n["essid"], \
                n["ap_mac"], n["quality"], n["channel"])
                n["counter"] = counter
                counter+=1

        print "WEP Networks"
        for n in self.essids:
            if n["wep"] == True:
		print "[%i] %s AP[%s] Quality[%s] Channel[%s]"% (\
                counter, n["essid"], \
                n["ap_mac"], n["quality"], n["channel"])
                n["counter"] = counter
                counter+=1


        print "WPA Networks"
        for n in self.essids:
            if n["wpa"] != None:
		print "[%i] %s AP[%s] Quality[%s] Channel[%s]"% (\
                counter, n["essid"], \
                n["ap_mac"], n["quality"], n["channel"])
                n["counter"] = counter
                counter+=1
    def list(self):
        print "Discovered Networks:"
        print "Open Networks"
        for n in self.essids:
            if n["wpa"] == None and n["wep"] == False:
		print "[%i] %s AP[%s] Quality[%s] Channel[%s]"% (\
                n["counter"], n["essid"], \
                n["ap_mac"], n["quality"], n["channel"])

        print "WEP Networks"
        for n in self.essids:
            if n["wep"] == True:
		print "[%i] %s AP[%s] Quality[%s] Channel[%s]"% (\
                n['counter'], n["essid"], \
                n["ap_mac"], n["quality"], n["channel"])


        print "WPA Networks"
        for n in self.essids:
            if n["wpa"] != None:
		print "[%i] %s AP[%s] Quality[%s] Channel[%s]"% (\
                n['counter'], n["essid"], \
                n["ap_mac"], n["quality"], n["channel"])

