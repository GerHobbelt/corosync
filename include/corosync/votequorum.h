/*
 * Copyright (c) 2009-2012 Red Hat, Inc.
 *
 * All rights reserved.
 *
 * Authors: Christine Caulfield (ccaulfie@redhat.com)
 *          Fabio M. Di Nitto   (fdinitto@redhat.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTIBUTORS "AS IS"
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

#ifndef COROSYNC_VOTEQUORUM_H_DEFINED
#define COROSYNC_VOTEQUORUM_H_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t votequorum_handle_t;

#define VOTEQUORUM_INFO_FLAG_TWONODE            1
#define VOTEQUORUM_INFO_FLAG_QUORATE            2
#define VOTEQUORUM_INFO_WAIT_FOR_ALL            4
#define VOTEQUORUM_INFO_LAST_MAN_STANDING       8
#define VOTEQUORUM_INFO_AUTO_TIE_BREAKER       16
#define VOTEQUORUM_INFO_LEAVE_REMOVE           32
#define VOTEQUORUM_INFO_QDEVICE                64

#define VOTEQUORUM_NODEID_QDEVICE 0
#define VOTEQUORUM_MAX_QDEVICE_NAME_LEN 255

#define NODESTATE_MEMBER     1
#define NODESTATE_DEAD       2
#define NODESTATE_LEAVING    3

/** @} */

struct votequorum_info {
	unsigned int node_id;
	unsigned int node_state;
	unsigned int node_votes;
	unsigned int node_expected_votes;
	unsigned int highest_expected;
	unsigned int total_votes;
	unsigned int quorum;
	unsigned int flags;
};

#ifdef EXPERIMENTAL_QUORUM_DEVICE_API
struct votequorum_qdevice_info {
	unsigned int votes;
	unsigned int state;
	char name[VOTEQUORUM_MAX_QDEVICE_NAME_LEN];
};
#endif

typedef struct {
	uint32_t nodeid;
	uint32_t state;
} votequorum_node_t;

typedef void (*votequorum_notification_fn_t) (
	votequorum_handle_t handle,
	uint64_t context,
	uint32_t quorate,
	uint32_t node_list_entries,
	votequorum_node_t node_list[]);

typedef void (*votequorum_expectedvotes_notification_fn_t) (
	votequorum_handle_t handle,
	uint64_t context,
	uint32_t expected_votes);

typedef struct {
	votequorum_notification_fn_t votequorum_notify_fn;
	votequorum_expectedvotes_notification_fn_t votequorum_expectedvotes_notify_fn;
} votequorum_callbacks_t;


/**
 * Create a new quorum connection
 */
cs_error_t votequorum_initialize (
	votequorum_handle_t *handle,
	votequorum_callbacks_t *callbacks);

/**
 * Close the quorum handle
 */
cs_error_t votequorum_finalize (
	votequorum_handle_t handle);


/**
 * Dispatch messages and configuration changes
 */
cs_error_t votequorum_dispatch (
	votequorum_handle_t handle,
	cs_dispatch_flags_t dispatch_types);

/**
 * Get a file descriptor on which to poll.
 *
 * @note votequorum_handle_t is NOT a file descriptor and may not be
 *       used directly.
 */
cs_error_t votequorum_fd_get (
	votequorum_handle_t handle,
	int *fd);

/**
 * Get quorum information.
 */
cs_error_t votequorum_getinfo (
	votequorum_handle_t handle,
	unsigned int nodeid,
	struct votequorum_info *info);

/**
 * set expected_votes
 */
cs_error_t votequorum_setexpected (
	votequorum_handle_t handle,
	unsigned int expected_votes);

/**
 * set votes for a node
 */
cs_error_t votequorum_setvotes (
	votequorum_handle_t handle,
	unsigned int nodeid,
	unsigned int votes);

/**
 * Track node and quorum changes
 */
cs_error_t votequorum_trackstart (
	votequorum_handle_t handle,
	uint64_t context,
	unsigned int flags );

cs_error_t votequorum_trackstop (
	votequorum_handle_t handle);

/**
 * Save and retrieve private data/context
 */
cs_error_t votequorum_context_get (
	votequorum_handle_t handle,
	void **context);

cs_error_t votequorum_context_set (
	votequorum_handle_t handle,
	void *context);

#ifdef EXPERIMENTAL_QUORUM_DEVICE_API
/**
 * Register a quorum device
 *
 * it will be DEAD until polled
 */
cs_error_t votequorum_qdevice_register (
	votequorum_handle_t handle,
	const char *name);

/**
 * Unregister a quorum device
 */
cs_error_t votequorum_qdevice_unregister (
	votequorum_handle_t handle,
	const char *name);

/**
 * Update registered name of a quorum device
 */
cs_error_t votequorum_qdevice_update (
	votequorum_handle_t handle,
	const char *oldname,
	const char *newname);

/**
 * Poll a quorum device
 */
cs_error_t votequorum_qdevice_poll (
	votequorum_handle_t handle,
	const char *name,
	unsigned int state);

/**
 * Get quorum device information
 */
cs_error_t votequorum_qdevice_getinfo (
	votequorum_handle_t handle,
	unsigned int nodeid,
	struct votequorum_qdevice_info *info);

#endif

#ifdef __cplusplus
}
#endif
#endif /* COROSYNC_VOTEQUORUM_H_DEFINED */
