/* ------------------------------------------------------------------------------------------------
jeremy@emperorlinux.com

This module was inspired by (and uses) the iw library that comes with the Linux Wireless Tools
package. The scanning portions of this module are moderately modified versions of those already
found in iwlist.c; however, in the future I plan to clean them up (especially the goto, wtf?)

Please run "pydoc pyiw" for more thorough, nicely formatted information on this module.

TODO: DO NOT USE DICTIONARY ACCESS! THIS IS RETARDED! :)

TODO: Refine WirelessInterface_Scan(). Right now there's a goto in there that makes me feel
      pretty uncomfortable. This is probably the first thing I'll work on when I have time.

TODO: Add better (more robust) support for setting the various parameters on the interface.
      The wireless world has changed a lot since I worked on this last, and there's room
      for me to certainly clean things up.
------------------------------------------------------------------------------------------------ */

#include <Python.h>
#include <structmember.h>
#include <iwlib.h>

#define PYIW_VERSION_MAJOR  0
#define PYIW_VERSION_MINOR  3
#define PYIW_VERSION_BUGFIX 3
#define PYIW_VERSION_STRING "0.3.3"

/* --------------------------------------------------------------------------- WirelessInterface */
typedef struct {
	PyObject_HEAD
	wireless_info info;
	char*	      ifname;
	int	      sock;
} WirelessInterface;

typedef enum {
	PYIW_KE_ESSID,
	PYIW_KE_WEP,
	PYIW_KE_WPA,
	PYIW_KE_MODE,
	PYIW_KE_CHANNEL,
	PYIW_KE_FREQUENCY,
	PYIW_KE_PROTOCOL,
	PYIW_KE_QUALITY,
	PYIW_KE_BITRATE,
	PYIW_KE_AP_MAC
} WirelessInterfaceKeyEnum;

typedef struct {
	WirelessInterfaceKeyEnum keys[2];
	PyObject*		 objs[2];
	int			 num;
} WirelessInterfaceScanData;
	
static char* WirelessInterfaceKeys[] = {
	"essid",
	"wep",
	"wpa",
	"mode",
	"channel",
	"frequency",
	"protocol",
	"quality",
	"bitrate",
	"ap_mac"
};

int WirelessInterfaceNumKeys(void) {
	return sizeof(WirelessInterfaceKeys) / sizeof(char*);
}

/* ------------------------------------------------------------------------ Function Definitions */
typedef struct iw_event           iwevent;
typedef struct iwreq              iwreq;
typedef struct ifreq              ifreq;
typedef struct timeval            timeval;
typedef WirelessInterface         wiface;
typedef WirelessInterfaceScanData wifacesd;

static PyObject* pyiw_version    (PyObject*, PyObject*);
static PyObject* pyiw_iw_version (PyObject*, PyObject*);
static PyObject* pyiw_we_version (PyObject*, PyObject*);

static int	 WirelessInterface_init    (wiface*, PyObject*, PyObject*);
static void	 WirelessInterface_dealloc (wiface*);
static void      WirelessInterface_refresh (wiface*);
static int	 WirelessInterface_len	   (PyObject*);
static PyObject* WirelessInterface_mapget  (PyObject*, PyObject*);
static int	 WirelessInterface_mapset  (PyObject*, PyObject*, PyObject*);
static PyObject* WirelessInterface_seqitem (PyObject*, int);

static PyObject* WirelessInterface_Scan     (wiface*);
static int	 WirelessInterface_ScanItem (iwevent*, iwrange*, wifacesd*);

static int Py_SetValInt    (PyObject*, int*, int, int);
static int Py_SetValDouble (PyObject*, double*, double, double);
static int Py_SetValString (PyObject*, char*, int);

static PyObject* PyIWError;

/* ----------------------------------------------------------------------------- PYIW_DOC_STRING */
static char* PYIW_DOC_STRING =
	"PYIW defines a single class, WirelessInterface, that must be instantiated\n"
	"with the name of the real wireless interface you wish to operate on. If\n"
	"the interface you specify doesn't exist, or if it doesn't support wireless\n"
	"extensions, object creation will fail and pyiw.error will be raised.. As an\n" 
	"important side note: PYIW requires at least Wireless Extensions 17. It may\n"
	"work in other cases, depending on how much has changed in future versions.\n\n"
	"The WirelessInterface object behaves very similarly to a dictionary and has\n"
	"the following keys (note that values that can be reassigned are indicated\n"
	"with the text RW); changed take effect immediately:\n\n"
	"- essid     (RW) [string]: The AP's ESSID.\n"
	"- wep       (RW) [string]: The currently used key; must be root.\n"
	"- wpa            [bool  ]: Whether or not WPA was detected.\n"
	"- mode      (RW) [int   ]: Auto, Ad-Hoc, Mangaged, Master,\n"
	"                           Repeater, Secondary, Monitor\n"
	"- channel   (RW) [double]: US standard channels 1-12. (index 0-11)\n"
	"- frequency      [double]: The GHz freq level.\n"
	"- protocol       [string]: The name representing A, B, or G WiFi.\n"
	"- quality        [int   ]: Signal quality, 1-100%.\n"
	"- bitrate        [int   ]: Number of BPS; 1:1 ratio.\n"
	"- ap_mac         [string]: The address of the current AP.\n\n"
	"--- EXAMPLE USAGE --------------------------------------\n\n"
	"try:\n"
	"	wi = pyiw.WirelessInterface(\"wlan0\")\n\n"
	"except pyiw.error, error:\n"
	"	print error\n"
	"	sys.exit(1)\n\n"
	"for param in wi:\n"
	"	print param, \"-->\", wi[param]\n\n"
	"wi[\"channel\"] = 6.0\n"
	"wi[\"essid\"  ] = \"Xenomorph\"\n"
	"wi[\"mode\"   ] = 3\n"
	"wi[\"wep\"    ] = \"AD0F44310CEF\"\n\n"
	"nets = wi.Scan()\n\n"
	"for net in nets:\n"
	"	print net[\"essid\"]\n"
;

/* -------------------------------------------------------------------------------- Py_SetValInt */
static int Py_SetValInt(PyObject* arg, int* val, int min, int max) {
	if(PyInt_Check(arg)) {
		int tmp = (int)(PyInt_AsLong(arg));

		if(tmp >= min && tmp <= max) {	
			(*val) = tmp;
			
			return 0;
		}

		else {
			PyErr_SetString(PyExc_ValueError, "Int too big/small in SetValInt");
			
			return -1;
		}
	}
		
	else {
		PyErr_SetString(PyExc_TypeError, "Non-int argument passed to ArgToInt");
		
		return -1;
	}
}

/* ----------------------------------------------------------------------------- Py_SetValDouble */
static int Py_SetValDouble(PyObject* arg, double* val, double min, double max) {
	if(PyFloat_Check(arg)) {
		double tmp = PyFloat_AsDouble(arg);
		
		if(tmp >= min && tmp <= max) {
			(*val) = tmp;
			
			return 0;
		}

		else {
			PyErr_SetString(PyExc_ValueError, "Double too big/small in SetValDouble");
			
			return -1;
		}
	}
		
	else {
		PyErr_SetString(PyExc_TypeError, "Non-double argument passed to ArgToDouble");
		
		return -1;
	}
}

/* ----------------------------------------------------------------------------- Py_SetValString */
static int Py_SetValString(PyObject* arg, char* val, int maxsize) {
	if(PyString_Check(arg)) {
		strncpy(val, PyString_AsString(arg), maxsize);
		
		return 0;
	}

	else {
		PyErr_SetString(PyExc_TypeError, "Non-string argument passed to ArgToString");
		
		return -1;
	}
}

/* ---------------------------------------------------------------------- WirelessInterface_init */
static int WirelessInterface_init(wiface* self, PyObject* args, PyObject* kargs) {
	const char*  ifname;
	const size_t ifnamesize;

	memset(&(self->info), 0, sizeof(wireless_info));
	
	self->ifname = NULL;
	self->sock   = 0;
	
	if(PyArg_ParseTuple(args, "s#", &ifname, &ifnamesize)) {	
		self->sock = iw_sockets_open();

		if(self->sock != -1) {
			ifreq frq;

			self->ifname = malloc(ifnamesize + 1);

			strncpy(self->ifname, ifname, ifnamesize + 1);
			strncpy(frq.ifr_name, self->ifname, IFNAMSIZ);
			
			if(!ioctl(self->sock, SIOCGIFFLAGS, &frq)) {
				frq.ifr_flags |= IFF_UP | IFF_RUNNING;
			
				ioctl(self->sock, SIOCSIFFLAGS, &frq);
			
				WirelessInterface_refresh(self);
				
				return 0;
			}

			else PyErr_SetString(PyIWError, "Failed to find device");
		}

		else PyErr_SetString(PyIWError, "Failed to connect to libiw");
	}

	return -1;
}

/* ------------------------------------------------------------------- WirelessInterface_dealloc */
static void WirelessInterface_dealloc(wiface* self) {
	if(self->ifname) free(self->ifname);

	iw_sockets_close(self->sock);
	self->ob_type->tp_free((PyObject*)(self));
}

/* ------------------------------------------------------------------- WirelessInterface_refresh */
static void WirelessInterface_refresh(wiface* self) {
	iwreq wrq;
	
	iw_get_basic_config(self->sock, self->ifname, &(self->info.b));
	iw_get_range_info(self->sock, self->ifname, &(self->info.range));

	iw_get_ext(self->sock, self->ifname, SIOCGIWRATE, &wrq);
	memcpy(&(self->info.bitrate), &wrq.u.bitrate, sizeof(iwparam));

	iw_get_ext(self->sock, self->ifname, SIOCGIWAP, &wrq);
	memcpy(&(self->info.ap_addr), &wrq.u.ap_addr, sizeof (sockaddr));

	iw_get_stats(
		self->sock, self->ifname, &(self->info.stats), 
		&(self->info.range), self->info.has_range
	);
}

/* ----------------------------------------------------------------------- WirelessInterface_len */
static int WirelessInterface_len(PyObject* self) {
	return WirelessInterfaceNumKeys();
}

/* -------------------------------------------------------------------- WirelessInterface_mapget */
static PyObject* WirelessInterface_mapget(PyObject* self, PyObject* arg) {
	if(PyString_Check(arg)) {
		wiface* wi  = (wiface*)(self);
		char*   key = PyString_AsString(arg);
		
		static char buf[128];

		WirelessInterface_refresh(wi);
		
		/* ESSID */
		if(!strncmp(key, "essid", 5)) return Py_BuildValue(
			"s", wi->info.b.essid
		);

		/* WEP */
		else if(!strncmp(key, "wep", 3)) {
			iw_print_key(
				buf, 
				sizeof(buf) / sizeof(char),
				wi->info.b.key,
				wi->info.b.key_size,
				wi->info.b.key_flags
			);
			
			if(!strncmp(buf, "00", 2)) return Py_BuildValue("s", "MUST_BE_ROOT");
			else return Py_BuildValue("s", buf);
		}

		/* WPA */
		else if(!strncmp(key, "wpa", 3)) return Py_BuildValue(
			"s", "Unsupported in pyiw version " PYIW_VERSION_STRING
		);

		/* PROTOCOL */
		else if(!strncmp(key, "protocol", 8)) return Py_BuildValue(
			"s", wi->info.b.name
		);
		
		/* FREQUENCY */
		else if(!strncmp(key, "frequency", 9)) {
			double freq = wi->info.b.freq;
		
			if(freq <= 14.0) iw_channel_to_freq((int)(freq), &freq, &(wi->info.range));
			
			return Py_BuildValue("d", freq);
		}

		/* CHANNEL */
		else if(!strncmp(key, "channel", 7)) {
			double freq = wi->info.b.freq;

			if(freq >= 14.0) {
				freq = (double)(iw_freq_to_channel(freq, &(wi->info.range)));
			}
				
			return Py_BuildValue("d", freq);
		}
		
		/* MODE */
		else if(!strncmp(key, "mode", 7)) return Py_BuildValue(
			"s", iw_operation_mode[wi->info.b.mode]
		);
		
		/* BITRATE */
		else if(!strncmp(key, "bitrate", 7)) return Py_BuildValue(
			"i", wi->info.bitrate.value
		);
		
		/* QUALITY */
		else if(!strncmp(key, "quality", 7)) return Py_BuildValue(
			"i", wi->info.stats.qual.qual
		);
		
		/* AP_MAC */
		else if(!strncmp(key, "ap_mac", 6)) {
			/* iw_pr_ether(buf, wi->info.ap_addr.sa_data); */
			/* iw_ether_ntop((const struct ether_addr *) addr, bufp); */
			iw_ether_ntop(
				(const struct ether_addr*)(wi->info.ap_addr.sa_data), 
				buf
			);
			
			return Py_BuildValue("s", buf);
		}
		
		else {
			PyErr_SetString(PyExc_ValueError, "Bad key in WirelessInterface_mapget");
			return NULL;
		}
	}

	else {
		PyErr_SetString(PyExc_TypeError, "Bad key type in WirelessInterface_mapget");
		return NULL;
	}
}
/* -------------------------------------------------------------------- WirelessInterface_mapset */
static int WirelessInterface_mapset(PyObject* self, PyObject* arg, PyObject* val) {
	if(PyString_Check(arg)) {
		wiface*      wi  = (wiface*)(self);
		char*        key = PyString_AsString(arg);
		int          ret = 0;
		struct iwreq wreq;
		
		memset(&wreq, 0, sizeof(struct iwreq));
		
		/* ESSID */
		if(!strncmp(key, "essid", 5)) {
			if(!Py_SetValString(val, wi->info.b.essid, sizeof(wi->info.b.essid))) {
				wreq.u.essid.flags   = 1;
				wreq.u.essid.pointer = wi->info.b.essid;
				wreq.u.essid.length  = strlen(wi->info.b.essid) + 1;

				#ifdef _PYIW_DEBUG_
				printf("PYIW DEBUG: iw_set_ext (ESSID) with %s\n", wi->info.b.essid);
				#endif
				
				ret = iw_set_ext(wi->sock, wi->ifname, SIOCSIWESSID, &wreq);
			
				#ifdef _PYIW_DEBUG_
				printf("PYIW DEBUG: iw_set_ext (ESSID) returned %d\n", ret);
				#endif
				
				return 0;
			}

			else return -1;
		}
		
		/* WEP */
		else if(!strncmp(key, "wep", 3)) {
			if(PyString_Check(arg)) {
				int len;
				
				memset(wi->info.b.key, 0, IW_ENCODING_TOKEN_MAX);
				
				len = iw_in_key(
					PyString_AsString(val), wi->info.b.key
				);
				
				wreq.u.data.length  = len;
				wreq.u.data.pointer = wi->info.b.key;
				
				ret = iw_set_ext(wi->sock, wi->ifname, SIOCSIWENCODE, &wreq);

				wi->info.b.has_key  = 1;
				wi->info.b.key_size = len;

				#ifdef _PYIW_DEBUG_
				printf("PYIW DEBUG: iw_in_key returned: %d\n", len);
				printf("PYIW DEBUG: iw_set_ext (ENCODE) returned: %d\n", ret);
				#endif
				
				return 0;
			}

			else {
				PyErr_SetString(PyExc_TypeError, "Key must be a string");
				return -1;
			}
		}

		/* CHANNEL */
		else if(!strncmp(key, "channel", 7)) {
			if(!Py_SetValDouble(val, &(wi->info.b.freq), 1.0, 12.0)) {
				iw_float2freq(wi->info.b.freq, &(wreq.u.freq));

				if(iw_set_ext(wi->sock, wi->ifname, SIOCSIWFREQ, &wreq)) return -1;
				
				else return 0;
			}

			else return -1;
		}
		
		/* MODE */
		else if(!strncmp(key, "mode", 4)) return Py_SetValInt(
			val, &(wi->info.b.mode), 0, 6
		);
		
		else {
			PyErr_SetString(PyExc_ValueError, "Bad key in WirelessInterface_mapset");
			return -1;
		}
	}

	else {
		PyErr_SetString(PyExc_TypeError, "Bad key type in WirelessInterface_mapset");
		return -1;
	}
}

/* ------------------------------------------------------------------- WirelessInterface_seqitem */
static PyObject* WirelessInterface_seqitem(PyObject* self, int index) {
	if(index >= 0 && index < WirelessInterfaceNumKeys()) return Py_BuildValue(
		"s", WirelessInterfaceKeys[index]
	);

	else return NULL;
}

/* ---------------------------------------------------------------------- WirelessInterface_Scan */
static PyObject* WirelessInterface_Scan(wiface* self) {
	iwreq          wrq;
	unsigned char* buffer     = NULL;
	int            buflen     = IW_SCAN_MAX_DATA;
	iwrange        range;
	int            has_range;
	timeval        tv;
	int            timeout    = 10000000;
	PyObject*      scan_list  = NULL;

	has_range = (iw_get_range_info(self->sock, self->ifname, &range) >= 0);

	tv.tv_sec          = 0;
	tv.tv_usec         = 250000;
	wrq.u.data.pointer = NULL;
	wrq.u.data.flags   = 0;
	wrq.u.data.length  = 0;

	if(iw_set_ext(self->sock, self->ifname, SIOCSIWSCAN, &wrq) < 0) {
		if(errno != EPERM) {
			PyErr_SetString(PyIWError, "Interface doesn't support scanning");
			
			return NULL;
		}
		
		tv.tv_usec = 0;
	}

	timeout -= tv.tv_usec;

	while(1) {
		fd_set rfds;
		int    last_fd;
		int    ret;

		FD_ZERO(&rfds);
		last_fd = -1;

		ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);

		if(ret < 0) {
			if(errno == EAGAIN || errno == EINTR) continue;
			
			else {
				PyErr_SetString(PyIWError, "Unknown scanning error");
				
				return NULL;
			}
		}

		if(!ret) {
			unsigned char* newbuf;

			realloc:
			newbuf = realloc(buffer, buflen);
			
			if(!newbuf) {
				if(buffer) free(buffer);
				
				PyErr_SetString(PyIWError, "Memory allocation failure in scan");
				
				return NULL;
			}
			
			buffer = newbuf;

			wrq.u.data.pointer = buffer;
			wrq.u.data.flags   = 0;
			wrq.u.data.length  = buflen;
			
			if(iw_get_ext(self->sock, self->ifname, SIOCGIWSCAN, &wrq) < 0) {
				if((errno == E2BIG)) {
					if(wrq.u.data.length > buflen) buflen = wrq.u.data.length;
					else buflen *= 2;

					goto realloc;
				}

				if(errno == EAGAIN) {
					tv.tv_sec   = 0;
					tv.tv_usec  = 100000;
					timeout    -= tv.tv_usec;
					
					if(timeout > 0) continue;
				}

				free(buffer);
				
				PyErr_SetString(PyIWError, "Unable to read scan data");

				return NULL;
			}
			
			else break;
		}
	}

	if(wrq.u.data.length)	{
		iwevent      iwe;
		stream_descr stream;
		int	     ret;
		PyObject*    scan_dict = NULL;
		
		scan_list = PyList_New(0);
		
		iw_init_event_stream(&stream, (char*)(buffer), wrq.u.data.length);
		
		do {
			ret = iw_extract_event_stream(&stream, &iwe, range.we_version_compiled);
		
			if(ret > 0) {
				wifacesd sd;
				int sr = WirelessInterface_ScanItem(&iwe, &range, &sd);

				if(sr) {
					int i;
					
					if(scan_dict) {
						PyList_Append(scan_list, scan_dict);	
						Py_DECREF(scan_dict);
					}

					scan_dict = PyDict_New();

					for(i = 0; i < WirelessInterfaceNumKeys(); i++) {
						PyMapping_SetItemString(
							scan_dict,
							WirelessInterfaceKeys[i],
							Py_BuildValue("")
						);
					}
				}
					
				if(sd.num) {
					int i;

					for(i = 0; i < sd.num; i++) {
						PyMapping_SetItemString(
							scan_dict, 
							WirelessInterfaceKeys[sd.keys[i]], 
							sd.objs[i]
						);
						
						Py_DECREF(sd.objs[i]);
					}
				}
			}
		}
		
		while(ret > 0);

		PyList_Append(scan_list, scan_dict);
		Py_XDECREF(scan_dict);
	}
	
	else return Py_BuildValue("[]");

	free(buffer);

	return scan_list;
}

/* ------------------------------------------------------------------ WirelessInterface_ScanItem */
static int WirelessInterface_ScanItem(iwevent* event, iwrange* range, wifacesd* data) {
	static char buf[128];
	
	memset(data, 0, sizeof(wifacesd));
	
	switch(event->cmd) {
		case SIOCGIWAP: {
			iw_ether_ntop(
				(const struct ether_addr*)(event->u.ap_addr.sa_data), 
				buf
			);		
			
			data->keys[0] = PYIW_KE_AP_MAC;
			data->objs[0] = Py_BuildValue("s", buf);
			data->num     = 1;
			
			return 1;
		}	

		case SIOCGIWFREQ: {
			double freq = iw_freq2float(&(event->u.freq));
			int    channel;
			
			if(freq <= 14.0) channel = iw_channel_to_freq((int)(freq), &freq, range);
			else channel = iw_freq_to_channel(freq, range);
			
			data->keys[0] = PYIW_KE_FREQUENCY;
			data->keys[1] = PYIW_KE_CHANNEL;
			data->objs[0] = Py_BuildValue("d", freq);
			data->objs[1] = Py_BuildValue("i", channel);
			data->num     = 2;
			
			return 0;
		}
		
		case SIOCGIWMODE: {
			data->keys[0] = PYIW_KE_MODE;
			data->objs[0] = Py_BuildValue("s", iw_operation_mode[event->u.mode]);
			data->num     = 1;

			return 0;
		}

		case SIOCGIWNAME: {
			data->keys[0] = PYIW_KE_PROTOCOL;
			data->objs[0] = Py_BuildValue("s", event->u.name);
			data->num     = 1;

			return 0;
		}
				  
		case SIOCGIWESSID: {
			memcpy(buf, event->u.essid.pointer, event->u.essid.length);
			buf[event->u.essid.length] = 0x0;
			 
			data->keys[0] = PYIW_KE_ESSID;
			data->objs[0] = Py_BuildValue("s", buf);
			data->num     = 1;

			return 0;
		}

		case SIOCGIWENCODE: {
			PyObject* pybool;
			
			if(event->u.data.flags & IW_ENCODE_DISABLED) pybool = Py_False;
			else pybool = Py_True;
				
			Py_INCREF(pybool);
			
			data->keys[0] = PYIW_KE_WEP;
			data->objs[0] = pybool;
			data->num     = 1;

			return 0;
		}

		case SIOCGIWRATE: {
			data->keys[0] = PYIW_KE_BITRATE;
			data->objs[0] = Py_BuildValue("i", event->u.bitrate.value);
			data->num     = 1;
					  
			return 0;
		}

		case IWEVQUAL: {
			data->keys[0] = PYIW_KE_QUALITY;
			data->objs[0] = Py_BuildValue("i", event->u.qual.qual);
			data->num     = 1;

			return 0;
		}
	
		case IWEVGENIE: {
			PyObject* pytrue = Py_True;
			Py_INCREF(pytrue);
					
			data->keys[0] = PYIW_KE_WPA;
			data->objs[0] = pytrue;
			data->num     = 1;

			return 0;
		}
			       
		case IWEVCUSTOM: {
			memcpy(buf, event->u.data.pointer, event->u.data.length);
			buf[event->u.data.length] = 0x0;
			
			if(strstr(buf, "wpa_ie")) {
				PyObject* pytrue = Py_True;
				Py_INCREF(pytrue);

				data->keys[0] = PYIW_KE_WPA;
				data->objs[0] = pytrue;
				data->num     = 1;
			}

			memset(buf, 0, sizeof(buf));
		}
		
		default: return 0;
	}
}

/* -------------------------------------------------------------------- Member/Method Structures */
static PyMethodDef module_methods[] = {
	{ 
		"version", pyiw_version, METH_NOARGS,
		"Returns the current PyIW version."
	},
	{
		"iw_version", pyiw_iw_version, METH_NOARGS,
		"Returns the current Wireless Extnesions (libiw WE) version."
	},
	{
		"we_version", pyiw_we_version, METH_NOARGS,
		"Returns the current Wireless Extensions (kernel-level WE) version."
	},
	{ NULL, NULL, 0, NULL }
};

static PyMethodDef WirelessInterface_methods[] = {
	{
		"Scan", (PyCFunction)(WirelessInterface_Scan), METH_NOARGS,
		"This function will attempt to scan any local AP's and return a tuple\n"
		"of objectes representing the data contained therein."
	},
	{ NULL, NULL, 0, NULL }
};

static PyMappingMethods WirelessInterface_mapping_methods = {
	WirelessInterface_len,    /* length */
	WirelessInterface_mapget, /* getitem */
	WirelessInterface_mapset  /* setitem */
};

static PySequenceMethods WirelessInterface_sequence_methods = {
	WirelessInterface_len,     /* sq_length */
	0,                         /* sq_concat */
	0,                         /* sq_repeat */
	WirelessInterface_seqitem, /* sq_item */
	0,                         /* sq_slice */
	0,                         /* sq_ass_item */
	0,                         /* sq_ass_slice */
	0,                         /* sq_contains */
	0,                         /* sq_inplace_concat */
	0                          /* sq_inplace_repeat */
};

/* -------------------------------------------------------------------- PyType_WirelessInterface */
static PyTypeObject PyType_WirelessInterface = {
	PyObject_HEAD_INIT(NULL)
	0,                                        /* ob_size */
	"pyiw.WirelessInterface",                 /* tp_name */
	sizeof(WirelessInterface),                /* tp_basicsize */
	0,                                        /* tp_itemsize */
	(destructor)(WirelessInterface_dealloc),  /* tp_dealloc */
	0,                                        /* tp_print */
	0,                                        /* tp_getattr */
	0,                                        /* tp_setattr */
	0,                                        /* tp_compare */
	0,                                        /* tp_repr */
	0,                                        /* tp_as_number */
	&WirelessInterface_sequence_methods,      /* tp_as_sequence */
	&WirelessInterface_mapping_methods,       /* tp_as_mapping */
	0,                                        /* tp_hash */
	0,                                        /* tp_call */
	0,                                        /* tp_str */
	0,                                        /* tp_getattro */
	0,                                        /* tp_setattro */
	0,                                        /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
	0,                                        /* tp_doc */
	0,                                        /* tp_traverse */
	0,                                        /* tp_clear */
	0,                                        /* tp_richcompare */
	0,                                        /* tp_weaklistoffset */
	0,                                        /* tp_iter */
	0,                                        /* tp_iternext */
	WirelessInterface_methods,                /* tp_methods */
	0,                                        /* tp_members */
	0,                                        /* tp_getset */
	0,                                        /* tp_base */
	0,                                        /* tp_dict */
	0,                                        /* tp_descr_get */
	0,                                        /* tp_descr_set */
	0,                                        /* tp_dictoffset */
	(initproc)(WirelessInterface_init),       /* tp_init */
	0,                                        /* tp_alloc */
	PyType_GenericNew,                        /* tp_new */
	0,                                        /* tp_free */
	0,                                        /* tp_is_gc */
	0,                                        /* tp_bases */
	0,                                        /* tp_mro */
	0,                                        /* tp_cache */
	0,                                        /* tp_subclasses */
	0,                                        /* tp_weaklist */
	0                                         /* tp_del */
};

/* ------------------------------------------------------------------------------------ initpyiw */
PyMODINIT_FUNC initpyiw(void) {
	PyObject* module;
	
	PyType_Ready(&PyType_WirelessInterface);

	module    = Py_InitModule3("pyiw", module_methods, PYIW_DOC_STRING);
	PyIWError = PyErr_NewException("pyiw.error", NULL, NULL);

	Py_INCREF(&PyType_WirelessInterface);
	Py_INCREF(PyIWError);
	
	PyModule_AddObject(module, "WirelessInterface", (PyObject*)(&PyType_WirelessInterface));
	PyModule_AddObject(module, "error", PyIWError);
}

static PyObject* pyiw_version(PyObject* u1, PyObject* u2) {
	return Py_BuildValue("(iii)", 
		PYIW_VERSION_MAJOR,
		PYIW_VERSION_MINOR,
		PYIW_VERSION_BUGFIX
	);
}

static PyObject* pyiw_iw_version(PyObject* u1, PyObject* u2) {
	return Py_BuildValue("i", WE_VERSION);
}

static PyObject* pyiw_we_version(PyObject* u1, PyObject* u2) {
	return Py_BuildValue("i", iw_get_kernel_we_version());
}
