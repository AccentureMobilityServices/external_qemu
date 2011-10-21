/*
 *  gles2emulator_utils.c
 *
 *  Memory allocator and shared mapping functions.
 *
 *  Copyright (c) 2011 Accenture Ltd	
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gles2emulator_utils.h"
#include "qemu_file.h"
#include "qemu_debug.h"
#include "android/globals.h"
#include "qemu_debug.h"


//#define DEBUG 1

#if DEBUG
#  define  DBGPRINT(...) dprintn(__VA_ARGS__)
#else
#  define  DBGPRINT(...) ((void)0)
#endif

extern void  dprintn (const char*  fmt, ...);



/* Create a shared memory file descriptor. */
void
gles2emulator_utils_create_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct)
{

	DBGPRINT ("[INFO (%s)] : Creating shared memory object descriptor '%s'.\n", __FUNCTION__, theSharedMemoryStruct->sharedMemoryObjectName);

	theSharedMemoryStruct->fileDescriptor = shm_open (theSharedMemoryStruct->sharedMemoryObjectName, (O_CREAT | O_TRUNC | O_RDWR), (ALLPERMS));
	if (theSharedMemoryStruct->fileDescriptor < 0) {
		DBGPRINT ("    (ERROR) : Failed to create shared memory descriptor.\n");
		theSharedMemoryStruct->actualAddress = 0;
		return;
	}

	if (!ftruncate (theSharedMemoryStruct->fileDescriptor, theSharedMemoryStruct->size)) {
		DBGPRINT ("    (more) : Successfully ftruncated() object to size %d, done.\n", theSharedMemoryStruct->size);
	}
}


/* Close a shared memory file descriptor. */
void
gles2emulator_utils_close_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct)
{

	DBGPRINT ("[INFO (%s)] : Closing shared memory object descriptor '%s'.\n", __FUNCTION__, theSharedMemoryStruct->sharedMemoryObjectName);

	if (!(close (theSharedMemoryStruct->fileDescriptor))) {
		DBGPRINT ("    (ERROR) : Failed to close shared memory descriptor.\n");
		return;
	}
	if (!(shm_unlink (theSharedMemoryStruct->sharedMemoryObjectName))) {
		DBGPRINT ("    (ERROR) : Failed to unlink shared memory descriptor.\n");
		return;
	}

	DBGPRINT ("    (more) : Successfully closed shared memory descriptor.\n");
}


/* Allocate and map shared memory to file descriptor. */
void
gles2emulator_utils_map_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct)
{

	DBGPRINT ("[INFO (%s)] : mmap() on shared memory descriptor '%s'\n", __FUNCTION__, theSharedMemoryStruct->sharedMemoryObjectName);

    if (!theSharedMemoryStruct->size) {
		DBGPRINT ("    (ERROR) : Null memory size.\n");
		theSharedMemoryStruct->actualAddress = 0;
		return;
    }

	theSharedMemoryStruct->actualAddress = mmap (theSharedMemoryStruct->requiredAddress, theSharedMemoryStruct->size, PROT_READ | PROT_WRITE, MAP_SHARED, theSharedMemoryStruct->fileDescriptor, 0);
	if (theSharedMemoryStruct->actualAddress == MAP_FAILED) {
		DBGPRINT ("    (ERROR) : Failed to mmap memory.\n");
		theSharedMemoryStruct->actualAddress = 0;
		return;
	}

	DBGPRINT ("    (more) : Successfully mmap'ed region size %d to (host) region: 0x%x.\n", theSharedMemoryStruct->size, theSharedMemoryStruct->actualAddress);
}


/* Unmap memory. */
void
gles2emulator_utils_unmap_sharedmemory (struct hostSharedMemoryStruct *theSharedMemoryStruct)
{

	DBGPRINT ("[INFO (%s)] : munmap() on shared memory descriptor '%s' at address: 0x%x.\n", __FUNCTION__, theSharedMemoryStruct->sharedMemoryObjectName, theSharedMemoryStruct->actualAddress);

    if (!theSharedMemoryStruct->size) {
		DBGPRINT ("    (ERROR) : Null memory size.\n");
		return;
    }

	if (munmap (theSharedMemoryStruct->actualAddress, theSharedMemoryStruct->size)) {
		DBGPRINT ("    (ERROR) : Failed to munmap memory.\n");
		return;
	}

	DBGPRINT ("    (more) : Successfully munmap'ed region.\n");
}


/* Create a shared semaphore. */
void
gles2emulator_utils_create_sharedmemory_semaphore (struct hostSharedMemoryStruct *theSharedMemoryStruct)
{

	DBGPRINT ("[INFO (%s)] : Create semaphore descriptor '%s'\n", __FUNCTION__, theSharedMemoryStruct->sharedMemorySemaphoreName);

	theSharedMemoryStruct->semaphore = sem_open (theSharedMemoryStruct->sharedMemorySemaphoreName, O_CREAT, 0666, 0);
	if (theSharedMemoryStruct->semaphore == SEM_FAILED) {
		DBGPRINT ("    (ERROR) : Failed to open semaphore.\n");
		sem_unlink (theSharedMemoryStruct->sharedMemorySemaphoreName);
		theSharedMemoryStruct->actualAddress = 0;
		return;
    }

	DBGPRINT ("    (more) : Created semaphore id: 0x%x.\n", theSharedMemoryStruct->semaphore);
}


/* Close and unlink a shared semaphore. */
void
gles2emulator_utils_remove_sharedmemory_semaphore (struct hostSharedMemoryStruct *theSharedMemoryStruct)
{

	DBGPRINT ("[INFO (%s)] : Closing semaphore descriptor '%s'\n", __FUNCTION__, theSharedMemoryStruct->sharedMemorySemaphoreName);

	sem_unlink (theSharedMemoryStruct->sharedMemorySemaphoreName);

	if (sem_close (theSharedMemoryStruct->semaphore)) {
		DBGPRINT ("    (ERROR) : Failed to close semaphore.\n");
		return;
    }
	if (sem_unlink (theSharedMemoryStruct->sharedMemorySemaphoreName)) {
		DBGPRINT ("    (ERROR) : Failed to unlink semaphore.\n");
		return;
	}

	DBGPRINT ("    (more) : Succesfully closed and unlinked semaphore.\n");
}


/* Create and allocate a shared memory segment. */
void
gles2emulator_utils_create_sharedmemory_segment (struct hostSharedMemoryStruct *theSharedMemoryStruct)
{

	DBGPRINT ("[INFO (%s)] : Creating shared memory segment key: 0x%x\n", __FUNCTION__, theSharedMemoryStruct->sharedMemorySegmentKey);

	theSharedMemoryStruct->sharedMemoryID = shmget (theSharedMemoryStruct->sharedMemorySegmentKey, theSharedMemoryStruct->size, IPC_CREAT | 0666);
	if(theSharedMemoryStruct->sharedMemoryID < 0) {
		DBGPRINT ("    (ERROR) : Failed to create shared memory segment.\n");
		theSharedMemoryStruct->actualAddress = 0;
		return;
    	}
	
	theSharedMemoryStruct->actualAddress = shmat (theSharedMemoryStruct->sharedMemoryID, NULL, 0);

	if ((int)theSharedMemoryStruct->actualAddress != -1) {
		DBGPRINT ("    (more) : Successfully created shared memory segment of size %d with key: 0x%x at address 0x%x.\n", theSharedMemoryStruct->size, theSharedMemoryStruct->sharedMemorySegmentKey, theSharedMemoryStruct->actualAddress);
	} else {
		DBGPRINT ("    (ERROR) : Could not attach process memory to segment key, error: %s.\n", strerror ((int)theSharedMemoryStruct->actualAddress));
		theSharedMemoryStruct->actualAddress = 0;
		return;
	}
	

	/* Test */
//	sprintf (theSharedMemoryStruct->actualAddress, "DEADBEEFDEADBEEFDEADBEEF\n");
}



mqd_t
gles2emulator_utils_ipc_open (struct hostIPCStruct *theIPCStruct)
{
	theIPCStruct->theMessageQueue = mq_open (theIPCStruct->theMessageQueueName, (O_RDWR));
	if (theIPCStruct->theMessageQueue > 0)
	{
		gles2emulator_utils_ipc_get_attributes(theIPCStruct);
		theIPCStruct->theMessageBufferSize = theIPCStruct->theQueueAttributes.mq_msgsize;
	} else {
		DBGPRINT ("Could not open message queue.\n");
	}

	return theIPCStruct->theMessageQueue;
}

mqd_t
gles2emulator_utils_ipc_create (struct hostIPCStruct *theIPCStruct)
{
	theIPCStruct->theQueueAttributes.mq_maxmsg = theIPCStruct->maxMessages;
	theIPCStruct->theQueueAttributes.mq_msgsize = theIPCStruct->theMessageBufferSize;	
	theIPCStruct->theQueueAttributes.mq_flags = O_NONBLOCK;
	theIPCStruct->theMessageQueue = mq_open (theIPCStruct->theMessageQueueName, theIPCStruct->theFileMode, S_IRWXU | S_IRWXG, NULL);
	if (theIPCStruct->theMessageQueue < 0)
	{
		DBGPRINT ("Could not create message queue.\n");
	}

	return theIPCStruct->theMessageQueue;
}

mqd_t
gles2emulator_utils_ipc_close (struct hostIPCStruct *theIPCStruct)
{
	return mq_close (theIPCStruct->theMessageQueue);
}

mqd_t
gles2emulator_utils_ipc_unlink (struct hostIPCStruct *theIPCStruct)
{
	return mq_unlink (theIPCStruct->theMessageQueueName);
}

mqd_t
gles2emulator_utils_ipc_receive_message (struct hostIPCStruct *theIPCStruct)
{
	theIPCStruct->bytes_received = mq_receive (theIPCStruct->theMessageQueue, theIPCStruct->theMessageBuffer, theIPCStruct->theMessageBufferSize, &theIPCStruct->thePriority);
	return theIPCStruct->bytes_received;
}

mqd_t
gles2emulator_utils_ipc_receive_message_with_timeout (struct hostIPCStruct *theIPCStruct, float seconds)
{
struct timespec theWait;


	theWait.tv_sec = (int)floor(seconds);
	theWait.tv_nsec = (int)(1e9 * (seconds - theWait.tv_sec));

	theIPCStruct->bytes_received = mq_timedreceive (theIPCStruct->theMessageQueue, theIPCStruct->theMessageBuffer, theIPCStruct->theMessageBufferSize, &theIPCStruct->thePriority, &theWait);
	return theIPCStruct->bytes_received;
}

mqd_t
gles2emulator_utils_ipc_send_message (struct hostIPCStruct *theIPCStruct)
{
	return mq_send (theIPCStruct->theMessageQueue, theIPCStruct->theMessage, theIPCStruct->theMessageLength, theIPCStruct->thePriority);
}

mqd_t
gles2emulator_utils_ipc_send_message_with_timeout (struct hostIPCStruct *theIPCStruct, float seconds)
{
struct timespec theWait;


	theWait.tv_sec = (int)floor(seconds);
	theWait.tv_nsec = (int)(1e9 * (seconds - theWait.tv_sec));

	return mq_timedsend (theIPCStruct->theMessageQueue, theIPCStruct->theMessage, theIPCStruct->theMessageLength, theIPCStruct->thePriority, &theWait);
}

mqd_t
gles2emulator_utils_ipc_get_attributes (struct hostIPCStruct *theIPCStruct)
{
	return mq_getattr(theIPCStruct->theMessageQueue, &theIPCStruct->theQueueAttributes);
}

mqd_t
gles2emulator_utils_ipc_messages_in_queue (struct hostIPCStruct *theIPCStruct)
{
	return mq_getattr(theIPCStruct->theMessageQueue, &theIPCStruct->theQueueAttributes);
}

mqd_t
gles2emulator_utils_ipc_set_blocking_mode (struct hostIPCStruct *theIPCStruct)
{
mqd_t theReturnVal;


	theReturnVal = mq_getattr(theIPCStruct->theMessageQueue, &theIPCStruct->theQueueAttributes);
	
	if (!theReturnVal)
	{
		DBGPRINT ("Error in obtaining queue attributes!\n");
		return theReturnVal;
	}

//	theIPCStruct->isBlocking ? theIPCStruct->theQueueAttributes.mq_flags = 0 : theIPCStruct->theQueueAttributes.mq_flags = O_NONBLOCK;
	return mq_setattr(theIPCStruct->theMessageQueue, &theIPCStruct->theQueueAttributes, NULL);
}

mqd_t
gles2emulator_utils_ipc_set_notifier_function (struct hostIPCStruct *theIPCStruct, void *theDataPointerToPass)
{
//	theIPCStruct->theSignalThread.sigev_notify = SIGEV_THREAD;
	sigemptyset (&theIPCStruct->theSignalAction.sa_mask);
	theIPCStruct->theSignalAction.sa_flags = SA_SIGINFO;
	theIPCStruct->theSignalAction.sa_sigaction = theIPCStruct->theThreadPointer;
   	sigaction (SIGRTMAX, &theIPCStruct->theSignalAction, NULL);
	theIPCStruct->theSignalEvent.sigev_notify = SIGEV_SIGNAL;
	theIPCStruct->theSignalEvent.sigev_signo = SIGRTMAX;
	theIPCStruct->theSignalEvent.sigev_notify_function = theIPCStruct->theThreadPointer;
//	theIPCStruct->theSignalEvent.sigev_notify_attributes = NULL;
	theIPCStruct->theSignalEvent.sigev_value.sival_ptr = theDataPointerToPass;
	return mq_notify(theIPCStruct->theMessageQueue, &theIPCStruct->theSignalEvent);
}

