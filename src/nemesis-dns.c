/*
 * $Id: nemesis-dns.c,v 1.2 2005/09/27 19:46:19 jnathan Exp $
 *
 * THE NEMESIS PROJECT
 * Copyright (C) 1999, 2000, 2001 Mark Grimes <mark@stateful.net>
 * Copyright (C) 2001 - 2003 Jeff Nathan <jeff@snort.org>
 *
 * nemesis-dns.c (DNS Packet Injector)
 *
 */

#include "nemesis-dns.h"
#include "nemesis.h"
#if defined(WIN32)
    #include <pcap.h>
#endif

static ETHERhdr etherhdr;
static IPhdr iphdr;
static TCPhdr tcphdr;
static UDPhdr udphdr;
static DNShdr dnshdr;
static FileData pd, ipod, tcpod;
static int got_payload;
static char *payloadfile = NULL;	/* payload file name */
static char *ipoptionsfile = NULL;	/* TCP options file name */
static char *tcpoptionsfile = NULL;	/* IP options file name */
static char *device = NULL;		/* Ethernet device */
#if defined(WIN32)
    static char *ifacetmp = NULL;
#endif

static void dns_cmdline(int argc, char **argv);
static int dns_exit(int code);
static void dns_initdata(void);
static void dns_usage(char *arg);
static void dns_validatedata(void);
static void dns_verbose(void);

void
nemesis_dns(int argc, char **argv)
{
	const char *module = "DNS Packet Injection";

	nemesis_maketitle(title, module, version);

	if (argc > 1 && !strncmp(argv[1], "help", 4))
		dns_usage(argv[0]);

	if (nemesis_seedrand() < 0)
		fprintf(stderr, "ERROR: Unable to seed random number "
		    "generator.\n");

	dns_initdata();
	dns_cmdline(argc, argv);
	dns_validatedata();
	dns_verbose();

	if (got_payload) {
		if (state) {
#if defined(WIN32)
			if (builddatafromfile(DNSTCP_LINKBUFFSIZE, &pd,
			    (const char *)payloadfile, 
			    (const uint32_t)PAYLOADMODE) < 0) {
#else
			if (builddatafromfile(((got_link == 1) ? 
			    DNSTCP_LINKBUFFSIZE : DNSTCP_RAWBUFFSIZE), &pd, 
			    (const char *)payloadfile, 
			    (const uint32_t)PAYLOADMODE) < 0) {
#endif
				dns_exit(1);
			}
        	} else {
#if defined(WIN32)
			if (builddatafromfile(DNSUDP_LINKBUFFSIZE, &pd, 
			    (const char *)payloadfile, 
			    (const uint32_t)PAYLOADMODE) < 0) {
#else
			if (builddatafromfile(((got_link == 1) ? 
			    DNSUDP_LINKBUFFSIZE :
			    DNSUDP_RAWBUFFSIZE), &pd, 
			    (const char *)payloadfile, 
			    (const uint32_t)PAYLOADMODE) < 0) {
#endif
				dns_exit(1);
			}
        	}
	}

	if (got_ipoptions) {
		if (builddatafromfile(OPTIONSBUFFSIZE, &ipod, 
		    (const char *)ipoptionsfile, 
		    (const uint32_t)OPTIONSMODE) < 0) {
			dns_exit(1);
		}
	}

	if (state && got_tcpoptions) {
		if (builddatafromfile(OPTIONSBUFFSIZE, &tcpod, 
		    (const char *)tcpoptionsfile, 
		    (const uint32_t)OPTIONSMODE) < 0) {
			dns_exit(1);
		}
	}
       
	if (builddns(&etherhdr, &iphdr, &tcphdr, &udphdr, &dnshdr, &pd, &ipod, 
	    &tcpod, device) < 0) {
		puts("\nDNS Injection Failure");
		dns_exit(1);
	} else {
		puts("\nDNS Packet Injected");
		dns_exit(0);
	}
}

static void
dns_initdata(void)
{
	/* defaults */
	etherhdr.ether_type = ETHERTYPE_IP;	/* Ethernet type IP */
	memset(etherhdr.ether_shost, 0, 6);	/* Ethernet src address */
	memset(etherhdr.ether_dhost, 0xff, 6);	/* Ethernet dst address */
	memset(&iphdr.ip_src.s_addr, 0, 4);	/* IP source address */
	memset(&iphdr.ip_dst.s_addr, 0, 4);	/* IP destination address */
	iphdr.ip_tos = IPTOS_LOWDELAY;		/* IP type of service */
	iphdr.ip_id = (uint16_t)libnet_get_prand(PRu16);	/* IP ID */
	iphdr.ip_p = IPPROTO_UDP;
	iphdr.ip_off = 0;			/* IP fragmentation offset */
	iphdr.ip_ttl = 255;			/* IP TTL */
	tcphdr.th_sport = (uint16_t)libnet_get_prand(PRu16);
						/* TCP source port */
	tcphdr.th_dport = 53;			/* TCP destination port */
	tcphdr.th_seq = (uint32_t)libnet_get_prand(PRu32);
						/* randomize sequence number */
	tcphdr.th_ack = (uint32_t)libnet_get_prand(PRu32);
						/* randomize ack number */
	tcphdr.th_flags = 0;			/* TCP flags */
	tcphdr.th_win = 4096;			/* TCP window size */
	udphdr.uh_sport = (uint16_t)libnet_get_prand(PRu16);
						/* UDP source port */
	udphdr.uh_dport = 53;			/* UDP destination port */
	pd.file_mem = NULL;
	pd.file_s = 0;
	ipod.file_mem = NULL;
	ipod.file_s = 0;
	tcpod.file_mem = NULL;
	tcpod.file_s = 0;
	return;
}

static void
dns_validatedata(void)
{
	struct sockaddr_in sin;

    /* validation tests */
	if (iphdr.ip_src.s_addr == 0)
		iphdr.ip_src.s_addr = (uint32_t)libnet_get_prand(PRu32);
	if (iphdr.ip_dst.s_addr == 0)
		iphdr.ip_dst.s_addr = (uint32_t)libnet_get_prand(PRu32);

	/* 
	 * If the user has supplied a source hardware addess but not a device
	 * try to select a device automatically.
	 */
	if (memcmp(etherhdr.ether_shost, zero, 6) && !got_link && !device) {
		if (libnet_select_device(&sin, &device, (char *)&errbuf) < 0) {
			fprintf(stderr, "ERROR: Device not specified and "
			    "unable to automatically select a device.\n");
			dns_exit(1);
		} else {
#ifdef DEBUG
			printf("DEBUG: automatically selected device: "
			    "       %s\n", device);
#endif
			got_link = 1;
		}
	}

	/* 
	 * If a device was specified and the user has not specified a source 
	 * hardware address, try to determine the source address automatically.
	 */
	if (got_link) {
		if ((nemesis_check_link(&etherhdr, device)) < 0) {
			fprintf(stderr, "ERROR: cannot retrieve hardware "
			    "address of %s.\n", device);
			dns_exit(1);
		}
	}

	/* 
	 * Attempt to send valid packets if the user hasn't decided to craft an
	 * anomolous packet.
	 */
	if (state && tcphdr.th_flags == 0)
		tcphdr.th_flags |= TH_SYN;
	return;
}

static void
dns_usage(char *arg)
{
	nemesis_printtitle((const char *)title);

	printf("DNS usage:\n  %s [-v (verbose)] [options]\n\n", arg);
	printf("DNS options: \n"
	       "  -i <DNS ID>\n"
	       "  -g <DNS flags>\n"
               "  -q <# of Questions>\n"
               "  -W <# of Answer RRs>\n"
               "  -A <# of Authority RRs>\n"
               "  -r <# of Additional RRs>\n"
               "  -P <Payload file>\n"
               "  -k (Enable TCP transport)\n\n");
	printf("TCP options (with -k): \n"
               "  -x <Source port>\n"
               "  -y <Destination port>\n"
               "  -f <TCP flags>\n"
               "     -fS (SYN), -fA (ACK), -fR (RST), -fP (PSH), -fF (FIN),"
               " -fU (URG)\n"
               "     -fE (ECE), -fC (CWR)\n"
               "  -w <Window size>\n"
               "  -s <SEQ number>\n"
               "  -a <ACK number>\n"
               "  -u <Urgent pointer offset>\n"
               "  -o <TCP options file>\n\n");
	printf("UDP options (without -k): \n"
               "  -x <Source port>\n"
               "  -y <Destination port>\n\n");
	printf("IP options: \n"
               "  -S <Source IP address>\n"
               "  -D <Destination IP address>\n"
               "  -I <IP ID>\n"
               "  -T <IP TTL>\n"
               "  -t <IP TOS>\n"
               "  -F <IP fragmentation options>\n"
               "     -F[D],[M],[R],[offset]\n"
               "  -O <IP options file>\n\n");
	printf("Data Link Options: \n"
#if defined(WIN32)
               "  -d <Ethernet device number>\n"
#else
               "  -d <Ethernet device name>\n"
#endif
               "  -H <Source MAC address>\n"
               "  -M <Destination MAC address>\n");
#if defined(WIN32)
	printf("  -Z (List available network interfaces by number)\n");
#endif
	putchar('\n');
	dns_exit(1);
}

static void
dns_cmdline(int argc, char **argv)
{
	int opt, i;
	uint32_t addr_tmp[6];
	char *dns_options;
	extern char *optarg;
	extern int optind;

#if defined(ENABLE_PCAPOUTPUT)
  #if defined(WIN32)
	dns_options = "a:A:b:d:D:f:F:g:H:i:I:M:o:O:P:q:r:s:S:t:T:u:w:W:x:y:kvZ?";
  #else
	dns_options = "a:A:b:d:D:f:F:g:H:i:I:M:o:O:P:q:r:s:S:t:T:u:w:W:x:y:kv?";
  #endif
#else
  #if defined(WIN32)
	dns_options = "a:A:b:d:D:f:F:g:H:i:I:M:o:O:P:q:r:s:S:t:T:u:w:x:y:kvZ?";
  #else
	dns_options = "a:A:b:d:D:f:F:g:H:i:I:M:o:O:P:q:r:s:S:t:T:u:w:x:y:kv?";
  #endif
#endif

	while ((opt = getopt(argc, argv, dns_options)) != -1) {
		switch (opt) {
		case 'a':	/* ACK window */
			if (getint32(optarg, &tcphdr.th_ack) < 0)
				dns_usage(argv[0]);
			break;
		case 'A':	/* number of authoritative resource records */
			if (getint16(optarg, &dnshdr.num_auth_rr) < 0)
				dns_usage(argv[0]);
			break;
		case 'b':	/* number of answers */
			if (getint16(optarg, &dnshdr.num_answ_rr) < 0)
				dns_usage(argv[0]);
			break;
		case 'd':	/* Ethernet device */
#if defined(WIN32)
			if (nemesis_getdev(atoi(optarg), &device) < 0) {
				fprintf(stderr, "ERROR: Unable to lookup "
				    "device: '%d'.\n", atoi(optarg));
				dns_usage(argv[0]);
			}
#else
			if (strlen(optarg) < 256) {
				    device = strdup(optarg);
				    got_link = 1;
			} else {
				fprintf(stderr, "ERROR: device %s > 256 "
				    "characters.\n", optarg);
				dns_usage(argv[0]);
			}
#endif
			break;
		case 'D':	/* destination IP address */
			if ((nemesis_name_resolve(optarg, 
			    (uint32_t *)&iphdr.ip_dst.s_addr)) < 0) {
				fprintf(stderr, "ERROR: Invalid destination IP "
				    "address: \"%s\".\n", optarg);
				dns_usage(argv[0]);
			}
			break;
		case 'f':	/* TCP flags */
			if (parsetcpflags(optarg, &tcphdr) < 0)
				dns_usage(argv[0]);
			break;
		case 'F':	/* IP fragmentation options */
			if (parsefragoptions(optarg, &iphdr) < 0)
				dns_usage(argv[0]);
			break;
		case 'g':	/* DNS flags */
			if (getint16(optarg, &dnshdr.flags) < 0)
				dns_usage(argv[0]);
			break;
		case 'H':	/* Ethernet source address */
			memset(addr_tmp, 0, sizeof(addr_tmp));
			sscanf(optarg, "%02X:%02X:%02X:%02X:%02X:%02X", 
			    &addr_tmp[0], &addr_tmp[1], &addr_tmp[2], 
			    &addr_tmp[3], &addr_tmp[4], &addr_tmp[5]);
			for (i = 0; i < 6; i++)
				etherhdr.ether_shost[i] = (uint8_t)addr_tmp[i];
			break;
		case 'i':	/* DNS ID */
			if (getint16(optarg, &dnshdr.id) < 0)
				dns_usage(argv[0]);
			break;
		case 'I':	/* IP ID */
			if (getint16(optarg, &iphdr.ip_id) < 0)
				dns_usage(argv[0]);
			break;
		case 'k':	/* use TCP */
			iphdr.ip_tos = 0;
			iphdr.ip_p = IPPROTO_TCP;
			state = 1;
			break;
		case 'M':    /* Ethernet destination address */
			memset(addr_tmp, 0, sizeof(addr_tmp));
			sscanf(optarg, "%02X:%02X:%02X:%02X:%02X:%02X", 
			    &addr_tmp[0], &addr_tmp[1], &addr_tmp[2], 
			    &addr_tmp[3], &addr_tmp[4], &addr_tmp[5]);
			for (i = 0; i < 6; i++)
				etherhdr.ether_dhost[i] = (uint8_t)addr_tmp[i];
			break;
		case 'o':	/* TCP options file */
			if (strlen(optarg) < 256) {
				tcpoptionsfile = strdup(optarg);
				got_tcpoptions = 1;
			} else {
				fprintf(stderr, "ERROR: TCP options file "
				    "%s > 256 characters.\n", optarg);
				dns_usage(argv[0]);
			}
			break;
		case 'O':	/* IP options file */
			if (strlen(optarg) < 256) {
				ipoptionsfile = strdup(optarg);
				got_ipoptions = 1;
			} else {
				fprintf(stderr, "ERROR: IP options file %s "
				    "> 256 characters.\n", optarg);
				dns_usage(argv[0]);
			}
			break;
		case 'P':	/* payload file */
                	if (strlen(optarg) < 256) {
				payloadfile = strdup(optarg);
				got_payload = 1;
                	} else {
				fprintf(stderr, "ERROR: payload file %s > 256 "
				    "characters.\n", optarg);
				dns_usage(argv[0]);
			}
			break;
		case 'q':	/* number of questions */
			if (getint16(optarg, &dnshdr.num_q) < 0)
			    dns_usage(argv[0]);
			break;
		case 'r':	/* number of additional resource records */
			if (getint16(optarg, &dnshdr.num_addi_rr) < 0)
			    dns_usage(argv[0]);
			break;
		case 's':	/* TCP sequence number */
			if (getint32(optarg, &tcphdr.th_seq) < 0)
			    dns_usage(argv[0]);
			break;
		case 'S':	/* source IP address */
			if ((nemesis_name_resolve(optarg, 
			    (uint32_t *)&iphdr.ip_src.s_addr)) < 0) {
				fprintf(stderr, "ERROR: Invalid source IP "
				    "address: \"%s\".\n", optarg);
				dns_usage(argv[0]);
			}
			break;
		case 't':	/* IP type of service */
			if (getint8(optarg, &iphdr.ip_tos) < 0)
			    dns_usage(argv[0]);
			break;
		case 'T':	/* IP time to live */
			if (getint8(optarg, &iphdr.ip_ttl) < 0)
			    dns_usage(argv[0]);
			break;
		case 'u':	/* TCP urgent pointer */
			if (getint16(optarg, &tcphdr.th_urp) < 0)
			    dns_usage(argv[0]);
			break;
		case 'v':
			if (++verbose >= 1)
			    nemesis_printtitle((const char *)title);
			break;
		case 'w':    /* TCP window size */
			if (getint16(optarg, &tcphdr.th_win) < 0)
			    dns_usage(argv[0]);
			break;
		case 'x':    /* TCP/UDP source port */
			if (getint16(optarg, &tcphdr.th_sport) < 0)
			    dns_usage(argv[0]);
			else
			    udphdr.uh_sport = tcphdr.th_sport;
			break;
		case 'y':    /* TCP/UDP destination port */
			if (getint16(optarg, &tcphdr.th_dport) < 0)
			    dns_usage(argv[0]);
			else
			    udphdr.uh_dport = tcphdr.th_dport;
			break;
#if defined(WIN32)
		case 'Z':
			if ((ifacetmp = pcap_lookupdev(errbuf)) == NULL)
				perror(errbuf);
			else
				PrintDeviceList(ifacetmp);

			dns_exit(1);
			/* NOTREACHED */
			break;
#endif
		case '?':	/* FALLTHROUGH */
		default:
			dns_usage(argv[0]);
			/* NOTREACHED */
			break;
		}
	}
	argc -= optind;
	argv += optind;
	return;
}

static int
dns_exit(int code)
{
	if (got_payload)
		free(pd.file_mem);

	if (got_ipoptions)
		free(ipod.file_mem);

	if (got_tcpoptions)
		free(tcpod.file_mem);

	if (device != NULL)
		free(device);

	if (tcpoptionsfile != NULL)
		free(tcpoptionsfile);

	if (ipoptionsfile != NULL)
		free (ipoptionsfile);

	if (payloadfile != NULL);
		free(payloadfile);

#if defined(WIN32)
	if (ifacetmp != NULL)
		free(ifacetmp);
#endif

	exit(code);
}

static void
dns_verbose(void)
{
	if (verbose) {
		if (got_link)
			nemesis_printeth(&etherhdr);

		nemesis_printip(&iphdr);

		if (state)
			nemesis_printtcp(&tcphdr);
		else
			nemesis_printudp(&udphdr);

		printf("   [DNS # Questions] %hu\n", dnshdr.num_q);
		printf("  [DNS # Answer RRs] %hu\n", dnshdr.num_answ_rr);
		printf("    [DNS # Auth RRs] %hu\n", dnshdr.num_auth_rr);
		printf("  [DNS # Addtnl RRs] %hu\n\n", dnshdr.num_addi_rr);
	}
	return;
}
