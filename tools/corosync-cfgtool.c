/*
 * Copyright (c) 2006-2007 Red Hat, Inc.
 *
 * All rights reserved.
 *
 * Author: Steven Dake <sdake@redhat.com>
 *
 * This software licensed under BSD license, the text of which follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the MontaVista Software, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <corosync/saAis.h>
#include <corosync/cfg.h>

static void ringstatusget_do (void)
{
	SaAisErrorT result;
	corosync_cfg_handle_t handle;
	unsigned int interface_count;
	char **interface_names;
	char **interface_status;
	unsigned int i;

	printf ("Printing ring status.\n");
	result = corosync_cfg_initialize (&handle, NULL);
	if (result != SA_AIS_OK) {
		printf ("Could not initialize corosync configuration API error %d\n", result);
		exit (1);
	}

	corosync_cfg_ring_status_get (handle,
		&interface_names,
		&interface_status,
		&interface_count);

	for (i = 0; i < interface_count; i++) {
		printf ("RING ID %d\n", i);
		printf ("\tid\t= %s\n", interface_names[i]);
		printf ("\tstatus\t= %s\n", interface_status[i]);
	}

	corosync_cfg_finalize (handle);
}

static void ringreenable_do (void)
{
	SaAisErrorT result;
	corosync_cfg_handle_t handle;

	printf ("Re-enabling all failed rings.\n");
	result = corosync_cfg_initialize (&handle, NULL);
	if (result != SA_AIS_OK) {
		printf ("Could not initialize corosync configuration API error %d\n", result);
		exit (1);
	}

	result = corosync_cfg_ring_reenable (handle);
	if (result != SA_AIS_OK) {
		printf ("Could not reenable ring error %d\n", result);
	}

	corosync_cfg_finalize (handle);
}

void service_load_do (char *service, unsigned int version)
{
	SaAisErrorT result;
	corosync_cfg_handle_t handle;

	printf ("Loading service '%s' version '%d'\n", service, version);
	result = corosync_cfg_initialize (&handle, NULL);
	if (result != SA_AIS_OK) {
		printf ("Could not initialize corosync configuration API error %d\n", result);
		exit (1);
	}
	result = corosync_cfg_service_load (handle, service, version);
	if (result != SA_AIS_OK) {
		printf ("Could not load service (error = %d)\n", result);
	}
	corosync_cfg_finalize (handle);
}

void service_unload_do (char *service, unsigned int version)
{
	SaAisErrorT result;
	corosync_cfg_handle_t handle;

	printf ("Unloading service '%s' version '%d'\n", service, version);
	result = corosync_cfg_initialize (&handle, NULL);
	if (result != SA_AIS_OK) {
		printf ("Could not initialize corosync configuration API error %d\n", result);
		exit (1);
	}
	result = corosync_cfg_service_unload (handle, service, version);
	if (result != SA_AIS_OK) {
		printf ("Could not unload service (error = %d)\n", result);
	}
	corosync_cfg_finalize (handle);
}

void usage_do (void)
{
	printf ("corosync-cfgtool [-s] [-r] [-l] [-u] [service_name] [-v] [version]\n\n");
	printf ("A tool for displaying and configuring active parameters within corosync.\n");
	printf ("options:\n");
	printf ("\t-s\tDisplays the status of the current rings on this node.\n");
	printf ("\t-r\tReset redundant ring state cluster wide after a fault to\n");
	printf ("\t\tre-enable redundant ring operation.\n");
	printf ("\t-l\tLoad a service identified by name.\n");
	printf ("\t-u\tUnload a service identified by name.\n");
}

int main (int argc, char *argv[]) {
	const char *options = "srl:u:v:";
	int opt;
	int service_load = 0;
	int service_unload = 0;
	char *service;
	unsigned int version;

	if (argc == 1) {
		usage_do ();
	}
	while ( (opt = getopt(argc, argv, options)) != -1 ) {
		switch (opt) {
		case 's':
			ringstatusget_do ();
			break;
		case 'r':
			ringreenable_do ();
			break;
		case 'l':
			service_load = 1;
			service = strdup (optarg);
			break;
		case 'u':
			service_unload = 1;
			service = strdup (optarg);
			break;
		case 'v':
			version = atoi (optarg);
		}
	}

	if (service_load) {
		service_load_do (service, version);
	} else 
	if (service_unload) {
		service_unload_do (service, version);
	}
		
	return (0);
}
