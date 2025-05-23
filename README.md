# SYNCHRONIZATION ALGORITHM

The synchronization algorithm is used for every service in corosync to
synchronize state of the system.

There are 4 events of the synchronization algorithm.  These events are in fact
functions that are registered in the service handler data structure.  They
are called by the synchronization system whenever a network partitions or
merges.

- **init**:
  Within the init event a service handler should record temporary state variables
  used by the process event.

- **process**:
  The process event is responsible for executing synchronization.  This event
  will return a state as to whether it has completed or not.  This allows for
  synchronization to be interrupted and recontinue when the message queue buffer
  is full.  The process event will be called again by the synchronization service
  if requested to do so by the return variable returned in process.

- **abort**:
  The abort event occurs when during synchronization a processor failure occurs.

- **activate**:
  The activate event occurs when process has returned no more processing is
  necessary for any node in the cluster and all messages originated by process
  have completed.

# CHECKPOINT SYNCHRONIZATION ALGORITHM

The purpose of the checkpoint synchronization algorithm is to synchronize
checkpoints after a partition or merge of two or more partitions.  The
secondary purpose of the algorithm is to determine the cluster-wide reference
count for every checkpoint.

Every cluster contains a group of checkpoints.  Each checkpoint has a
checkpoint name and checkpoint number.  The number is used to uniquely reference
an unlinked but still open checkpoint in the cluster.

Every checkpoint contains a reference count which is used to determine when
that checkpoint may be released.  The algorithm rebuilds the reference count
information each time a partition or merge occurs.

## local variables

`my_sync_state` may have the values SYNC_CHECKPOINT, SYNC_REFCOUNT

`my_current_iteration_state` contains any data used to iterate the checkpoints and sections.

## checkpoint data

`refcount_set` contains reference count for every node consisting of number of opened connections to checkpoint and node identifier `refcount` contains a summation of every reference count in the `refcount_set`.

## pseudocode executed

pseudocode executed by a processor when the synchronization service calls
the init event

	call process_checkpoints_enter

pseudocode executed by a processor when the synchronization service calls
the process event in the SYNC_CHECKPOINT state

	if lowest processor identifier of old ring in new ring
		transmit checkpoints or sections starting from my_current_iteration_state
	if all checkpoints and sections could be queued
		call sync_refcounts_enter
	else
		record my_current_iteration_state

	require process to continue

pseudocode executed by a processor when the synchronization service calls
the process event in the SYNC_REFCOUNT state

```
	if lowest processor identifier of old ring in new ring
		transmit checkpoint reference counts
	if all checkpoint reference counts could be queued
		require process to not continue
	else
		record my_current_iteration_state for checkpoint reference counts

sync_checkpoints_enter:
	my_sync_state = SYNC_CHECKPOINT
	my_current_iteration_state set to start of checkpoint list

sync_refcounts_enter:
	my_sync_state = SYNC_REFCOUNT

on event receipt of foreign ring id message
	ignore message
```

pseudocode executed on event receipt of checkpoint update

	if checkpoint exists in temporary storage
		ignore message
	else
		create checkpoint
		reset checkpoint refcount array

pseudocode executed on event receipt of checkpoint section update

	if checkpoint section exists in temporary storage
		ignore message
	else
		create checkpoint section

pseudocode executed on event receipt of reference count update

	update temporary checkpoint data storage reference count set by adding
	any reference counts in the temporary message set to those from the
	event
	update that checkpoint's reference count
	set the global checkpoint id to the current checkpoint id + 1 if it
	would increase the global checkpoint id

pseudocode called when the synchronization service calls the activate event:

```
for all checkpoints
	free all previously committed checkpoints and sections
	convert temporary checkpoints and sections to regular sections
copy my_saved_ring_id to my_old_ring_id
```

pseudocode called when the synchronization service calls the abort event:

	free all temporary checkpoints and temporary sections
 
