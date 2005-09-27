/*
 * $Id: nemesis-tcp.h,v 1.2 2005/09/27 19:46:19 jnathan Exp $
 *
 * THE NEMESIS PROJECT
 * Copyright (C) 1999, 2000, 2001 Mark Grimes <mark@stateful.net>
 * Copyright (C) 2001 - 2003 Jeff Nathan <jeff@snort.org>
 *
 * nemesis-tcp.h (TCP Packet Injector)
 *
 */

#ifndef __NEMESIS_TCP_H__
#define __NEMESIS_TCP_H__

#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#if defined(WIN32)
    #include <pcap.h>
#endif
#if defined(HAVE_NETINET_IN_H)
    #include <netinet/in.h>
#elif defined(WIN32)
    #include <winsock2.h>
#endif
#include <libnet.h>
#include "nemesis.h"

int buildtcp(ETHERhdr *eth, IPhdr *ip, TCPhdr *tcp, FileData *pd,
    FileData *ipod, FileData *tcpod, char *device);

#endif /* __NEMESIS_TCP_H__ */
