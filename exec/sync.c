/*
 * Copyright (c) 2005-2006 MontaVista Software, Inc.
 * Copyright (c) 2006-2007 Red Hat, Inc.
 *
 * All rights reserved.
 *
 * Author: Steven Dake (sdake@redhat.com)
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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <corosync/saAis.h>
#include <corosync/swab.h>
#include <corosync/totem/totempg.h>
#include <corosync/totem/totem.h>
#include <corosync/lcr/lcr_ifact.h>
#include <corosync/engine/logsys.h>

#include "main.h"
#include "sync.h"
#include "vsf.h"

LOGSYS_DECLARE_SUBSYS ("SYNC", LOG_INFO);

#define MESSAGE_REQ_SYNC_BARRIER 0
#define MESSAGE_REQ_SYNC_REQUEST 1

struct barrier_data {
	unsigned int nodeid;
	int completed;
};

static struct memb_ring_id *sync_ring_id;

static int vsf_none = 0;

static int (*sync_callbacks_retrieve) (int sync_id, struct sync_callbacks *callack);

static struct sync_callbacks sync_callbacks;

static int sync_processing = 0;

static void (*sync_synchronization_completed) (void);

static int sync_recovery_index = 0;

static void *sync_callback_token_handle = 0;
static void *sync_request_token_handle;

static struct barrier_data barrier_data_confchg[PROCESSOR_COUNT_MAX];

static int barrier_data_confchg_entries;

static struct barrier_data barrier_data_process[PROCESSOR_COUNT_MAX];

static struct corosync_vsf_iface_ver0 *vsf_iface;

static int sync_barrier_send (struct memb_ring_id *ring_id);

static int sync_start_process (enum totem_callback_token_type type, void *data);

static void sync_service_init (struct memb_ring_id *ring_id);

static int sync_service_process (enum totem_callback_token_type type, void *data);

static void sync_deliver_fn (
	unsigned int nodeid,
	struct iovec *iovec,
	int iov_len,
	int endian_conversion_required);

static void sync_confchg_fn (
	enum totem_configuration_type configuration_type,
	unsigned int *member_list, int member_list_entries,
	unsigned int *left_list, int left_list_entries,
	unsigned int *joined_list, int joined_list_entries,
	struct memb_ring_id *ring_id);

static void sync_primary_callback_fn (
	unsigned int *view_list,
	int view_list_entries,
	int primary_designated,
	struct memb_ring_id *ring_id);

static struct totempg_group sync_group = {
    .group      = "sync",
    .group_len  = 4
};

static totempg_groups_handle sync_group_handle;
static char *service_name;
static unsigned int current_members[PROCESSOR_COUNT_MAX];
static unsigned int current_members_cnt;

struct req_exec_sync_barrier_start {
	mar_req_header_t header;
	struct memb_ring_id ring_id;
};

struct sync_request {
	uint32_t name_len;
	char name[0] __attribute__((aligned(8)));
};

typedef struct sync_msg {
	mar_req_header_t header;
	struct memb_ring_id ring_id;
	struct sync_request sync_request;
} sync_msg_t;


/*
 * Send a barrier data structure
 */
static int sync_barrier_send (struct memb_ring_id *ring_id)
{
	struct req_exec_sync_barrier_start req_exec_sync_barrier_start;
	struct iovec iovec;
	int res;

	req_exec_sync_barrier_start.header.size = sizeof (struct req_exec_sync_barrier_start);
	req_exec_sync_barrier_start.header.id = MESSAGE_REQ_SYNC_BARRIER;

	memcpy (&req_exec_sync_barrier_start.ring_id, ring_id,
		sizeof (struct memb_ring_id));

	iovec.iov_base = (char *)&req_exec_sync_barrier_start;
	iovec.iov_len = sizeof (req_exec_sync_barrier_start);

	res = totempg_groups_mcast_joined (sync_group_handle, &iovec, 1, TOTEMPG_AGREED);

	return (res);
}

void sync_start_init (struct memb_ring_id *ring_id)
{
	totempg_callback_token_create (
		&sync_callback_token_handle,
		TOTEM_CALLBACK_TOKEN_SENT,
		0, /* don't delete after callback */
		sync_start_process,
		(void *)ring_id);
}

static void sync_service_init (struct memb_ring_id *ring_id)
{
	sync_callbacks.sync_init ();
	totempg_callback_token_destroy (&sync_callback_token_handle);

	/*
	 * Create the token callback for the processing
	 */
	totempg_callback_token_create (
		&sync_callback_token_handle,
		TOTEM_CALLBACK_TOKEN_SENT,
		0, /* don't delete after callback */
		sync_service_process,
		(void *)ring_id);
}

static int sync_start_process (enum totem_callback_token_type type, void *data)
{
	int res;
	struct memb_ring_id *ring_id = (struct memb_ring_id *)data;

	res = sync_barrier_send (ring_id);
	if (res == 0) {
		/*
		 * Delete the token callback for the barrier
		 */
		totempg_callback_token_destroy (&sync_callback_token_handle);
	}
	return (0);
}

void sync_callbacks_load (void)
{
	int res;

	for (;;) {
		res = sync_callbacks_retrieve (sync_recovery_index,
			&sync_callbacks);

		/*
		 * No more service handlers have sync callbacks at this time
	`	 */
		if (res == -1) {
			sync_processing = 0;
			break;
		}
		sync_recovery_index += 1;
		if (sync_callbacks.sync_init) {
			break;
		}
	}
}

static int sync_service_process (enum totem_callback_token_type type, void *data)
{
	int res;
	struct memb_ring_id *ring_id = (struct memb_ring_id *)data;

	
	/*
	 * If process operation not from this ring id, then ignore it and stop
	 * processing
	 */
	if (memcmp (ring_id, sync_ring_id, sizeof (struct memb_ring_id)) != 0) {
		return (0);
	}
	
	/*
	 * If process returns 0, then its time to activate
	 * and start the next service's synchronization
	 */
	res = sync_callbacks.sync_process ();
	if (res != 0) {
		return (0);
	}
	totempg_callback_token_destroy (&sync_callback_token_handle);

	sync_start_init (ring_id);

	return (0);
}

int sync_register (
	int (*callbacks_retrieve) (int sync_id, struct sync_callbacks *callack),
	void (*synchronization_completed) (void),
	char *vsf_type)
{
	unsigned int res;
	unsigned int vsf_handle;
	void *vsf_iface_p;
	char corosync_vsf_type[1024];

	res = totempg_groups_initialize (
		&sync_group_handle,
		sync_deliver_fn,
		sync_confchg_fn);
	if (res == -1) {
		log_printf (LOG_LEVEL_ERROR,
			"Couldn't initialize groups interface.\n");
		return (-1);
	}

	res = totempg_groups_join (
		sync_group_handle,
		&sync_group,
		1);
	if (res == -1) {
		log_printf (LOG_LEVEL_ERROR, "Couldn't join group.\n");
		return (-1);
	}
		
	if (strcmp (vsf_type, "none") == 0) {
		log_printf (LOG_LEVEL_NOTICE,
			"Not using a virtual synchrony filter.\n");
		vsf_none = 1;
	} else {
		vsf_none = 0;

		sprintf (corosync_vsf_type, "corosync_vsf_%s", vsf_type);
		res = lcr_ifact_reference (
			&vsf_handle,
			corosync_vsf_type,
			0,
			&vsf_iface_p,
			0);

		if (res == -1) {
			log_printf (LOG_LEVEL_NOTICE,
				"Couldn't load virtual synchrony filter %s\n",
				vsf_type);
			return (-1);
		}

		log_printf (LOG_LEVEL_NOTICE,
			"Using virtual synchrony filter %s\n", corosync_vsf_type);

		vsf_iface = (struct corosync_vsf_iface_ver0 *)vsf_iface_p;
		vsf_iface->init (sync_primary_callback_fn);
	}

	sync_callbacks_retrieve = callbacks_retrieve;
	sync_synchronization_completed = synchronization_completed;
	return (0);
}

static void sync_primary_callback_fn (
	unsigned int *view_list,
	int view_list_entries,
	int primary_designated,
	struct memb_ring_id *ring_id)
{
	int i;

	if (primary_designated) {
		log_printf (LOG_LEVEL_NOTICE, "This node is within the primary component and will provide service.\n");
	} else {
		log_printf (LOG_LEVEL_NOTICE, "This node is within the non-primary component and will NOT provide any services.\n");
		return;
	}

	/*
	 * Execute configuration change for synchronization service
	 */
	sync_processing = 1;

	totempg_callback_token_destroy (&sync_callback_token_handle);

	sync_recovery_index = 0;
	memset (&barrier_data_confchg, 0, sizeof (barrier_data_confchg));
	for (i = 0; i < view_list_entries; i++) {
		barrier_data_confchg[i].nodeid = view_list[i];
		barrier_data_confchg[i].completed = 0;
	}
	memcpy (barrier_data_process, barrier_data_confchg,
		sizeof (barrier_data_confchg));
	barrier_data_confchg_entries = view_list_entries;
	sync_start_init (sync_ring_id);
}

static struct memb_ring_id deliver_ring_id;

void sync_endian_convert (struct req_exec_sync_barrier_start *req_exec_sync_barrier_start)
{
	totemip_copy_endian_convert(&req_exec_sync_barrier_start->ring_id.rep,
		&req_exec_sync_barrier_start->ring_id.rep);
	req_exec_sync_barrier_start->ring_id.seq = swab64 (req_exec_sync_barrier_start->ring_id.seq);

}

static void sync_deliver_fn (
	unsigned int nodeid,
	struct iovec *iovec,
	int iov_len,
	int endian_conversion_required)
{
	struct req_exec_sync_barrier_start *req_exec_sync_barrier_start =
		(struct req_exec_sync_barrier_start *)iovec[0].iov_base;
	sync_msg_t *msg = (sync_msg_t *)iovec[0].iov_base;

	int i;

	if (endian_conversion_required) {
		sync_endian_convert (req_exec_sync_barrier_start);
	}

	int barrier_completed = 1;

	memcpy (&deliver_ring_id, &req_exec_sync_barrier_start->ring_id,
		sizeof (struct memb_ring_id));

	/*
	 * Is this barrier from this configuration, if not, ignore it
	 */
	if (memcmp (&req_exec_sync_barrier_start->ring_id, sync_ring_id,
		sizeof (struct memb_ring_id)) != 0) {
		return;
	}

	if (msg->header.id == MESSAGE_REQ_SYNC_REQUEST) {
		if (endian_conversion_required) {
			swab_mar_uint32_t (&msg->sync_request.name_len);
		}	
		/*
		 * If there is an ongoing sync, abort it. A requested sync is
		 * only allowed to abort other requested synchronizations,
		 * not full synchronizations.
		 */
		if (sync_processing && sync_callbacks.sync_abort) {
			sync_callbacks.sync_abort();
			sync_callbacks.sync_activate = NULL;
			sync_processing = 0;
			assert (service_name != NULL);
			free (service_name);
			service_name = NULL;
		}

		service_name = malloc (msg->sync_request.name_len);
		strcpy (service_name, msg->sync_request.name);

		/*
		 * Start requested synchronization
		 */
		sync_primary_callback_fn (current_members, current_members_cnt,	1,
			sync_ring_id);

		return;
	}

	/*
	 * Set completion for source_addr's address
	 */
	for (i = 0; i < barrier_data_confchg_entries; i++) {
		if (nodeid == barrier_data_process[i].nodeid) {
			barrier_data_process[i].completed = 1;
			log_printf (LOG_LEVEL_DEBUG,
				"Barrier Start Recieved From %d\n",
				barrier_data_process[i].nodeid);
			break;
		}
	}

	/*
	 * Test if barrier is complete
	 */
	for (i = 0; i < barrier_data_confchg_entries; i++) {
		log_printf (LOG_LEVEL_DEBUG,
			"Barrier completion status for nodeid %d = %d. \n", 
			barrier_data_process[i].nodeid,
			barrier_data_process[i].completed);
		if (barrier_data_process[i].completed == 0) {
			barrier_completed = 0;
		}
	}
	if (barrier_completed) {
		log_printf (LOG_LEVEL_DEBUG,
			"Synchronization barrier completed\n");
	}
	/*
	 * This sync is complete so activate and start next service sync
	 */
	if (barrier_completed && sync_callbacks.sync_activate) {
		sync_callbacks.sync_activate ();
	
		log_printf (LOG_LEVEL_DEBUG,
			"Committing synchronization for (%s)\n",
			sync_callbacks.name);
	}

	/*
	 * Start synchronization if the barrier has completed
	 */
	if (barrier_completed) {
		memcpy (barrier_data_process, barrier_data_confchg,
			sizeof (barrier_data_confchg));

		sync_callbacks_load();

		/*
		 * if sync service found, execute it
		 */
		if (sync_processing && sync_callbacks.sync_init) {
			log_printf (LOG_LEVEL_DEBUG,
				"Synchronization actions starting for (%s)\n",
				sync_callbacks.name);
			sync_service_init (&deliver_ring_id);
		}
	}
	return;
}

static void sync_confchg_fn (
	enum totem_configuration_type configuration_type,
	unsigned int *member_list, int member_list_entries,
	unsigned int *left_list, int left_list_entries,
	unsigned int *joined_list, int joined_list_entries,
	struct memb_ring_id *ring_id)
{
	int i;
	sync_ring_id = ring_id;

	if (configuration_type != TOTEM_CONFIGURATION_REGULAR) {
		return;
	}
	if (sync_processing && sync_callbacks.sync_abort != NULL) {
		sync_callbacks.sync_abort ();
		sync_callbacks.sync_activate = NULL;
	}
	/*
	 * Save current members and ring ID for later use
	 */
	for (i = 0; i < member_list_entries; i++) {
		current_members[i] = member_list[i];
	}
	current_members_cnt = member_list_entries;

	/*
	 * If no virtual synchrony filter configured, then start
	 * synchronization process
	 */
	if (vsf_none == 1) {
		sync_primary_callback_fn (
			member_list,
			member_list_entries,
			1,
			ring_id);
	}
}
/**
 * TOTEM callback function used to multicast a sync_request
 * message
 * @param type
 * @param _name
 *
 * @return int
 */
static int sync_request_send (
	enum totem_callback_token_type type, void *_name)
{
	int res;
	char *name = _name;
	sync_msg_t msg;
	struct iovec iovec[2];
	int name_len;

	ENTER("'%s'", name);

	name_len = strlen (name) + 1;
	msg.header.size = sizeof (msg) + name_len;
	msg.header.id = MESSAGE_REQ_SYNC_REQUEST;

	if (sync_ring_id == NULL) {
		log_printf (LOG_LEVEL_ERROR,
			"%s sync_ring_id is NULL.\n", __func__);
		return 1;
	}
	memcpy (&msg.ring_id, sync_ring_id,	sizeof (struct memb_ring_id));
	msg.sync_request.name_len = name_len;

	iovec[0].iov_base = (char *)&msg;
	iovec[0].iov_len = sizeof (msg);
	iovec[1].iov_base = _name;
	iovec[1].iov_len = name_len;

	res = totempg_groups_mcast_joined (
		sync_group_handle, iovec, 2, TOTEMPG_AGREED);

	if (res == 0) {
		/*
		 * We managed to multicast the message so delete the token callback
		 * for the sync request.
		 */
		totempg_callback_token_destroy (&sync_request_token_handle);
	}

	/*
	 * if we failed to multicast the message, this function will be called
	 * again.
	 */

	return (0);
}

int sync_in_process (void)
{
	return (sync_processing);
}

int sync_primary_designated (void)
{
	if (vsf_none == 1) {
		return (1);
	} else {
		return (vsf_iface->primary());
	}
}

/**
 * Execute synchronization upon request for the named service
 * @param name
 *
 * @return int
 */
int sync_request (char *name)
{
	assert (name != NULL);

	ENTER("'%s'", name);

	if (sync_processing) {
		return -1;
	}

	totempg_callback_token_create (&sync_request_token_handle,
		TOTEM_CALLBACK_TOKEN_SENT, 0, /* don't delete after callback */
		sync_request_send, name);

	LEAVE("");

	return 0;
}
