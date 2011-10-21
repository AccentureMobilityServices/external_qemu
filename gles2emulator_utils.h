/*
 *  gles2emulator_utils.h
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

#ifndef GLES2EMULATOR_UTILS_H
#define GLES2EMULATOR_UTILS_H

#include "qemu-common.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdio.h>
#include <mqueue.h>
#include <time.h>
#include <math.h>
#include <time.h>
#include <string.h>



struct hostSharedMemoryStruct {
	int fileDescriptor;
	size_t size;
	int sharedMemoryID;
	key_t sharedMemorySegmentKey;
	sem_t *semaphore;
	char *sharedMemoryObjectName;
	char *sharedMemorySemaphoreName;
	void *requiredAddress;
	void *actualAddress;
};

struct hostIPCStruct {
	char *theMessageQueueName;
	int maxMessages;
	int  theMessageBufferSize;
	char *theMessageBuffer;
	mqd_t theMessageQueue;
	struct mq_attr theQueueAttributes;
	long bytes_received;
	float waitTime;
	int isBlocking;
	mode_t theFileMode;
	char *theMessage;
	size_t theMessageLength;
	size_t thePriority;
	void *theThreadPointer;
	struct sigaction theSignalAction;
	struct sigevent theSignalEvent;
};

void gles2emulator_utils_create_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_close_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_map_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_unmap_sharedmemory (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_create_sharedmemory_semaphore (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_remove_sharedmemory_semaphore (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_create_sharedmemory_segment (struct hostSharedMemoryStruct *theSharedMemoryStruct);

/* IPC message queue support */
mqd_t gles2emulator_utils_ipc_open (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_close (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_unlink (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_create (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_receive_message (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_receive_message_with_timeout (struct hostIPCStruct *theIPCStruct, float seconds);
mqd_t gles2emulator_utils_ipc_send_message (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_send_message_with_timeout (struct hostIPCStruct *theIPCStruct, float seconds);
mqd_t gles2emulator_utils_ipc_get_attributes (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_messages_in_queue (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_set_blocking_mode (struct hostIPCStruct *theIPCStruct);
mqd_t gles2emulator_utils_ipc_set_notifier_function (struct hostIPCStruct *theIPCStruct, void *theDataPointerToPass);

#endif /* GLES2EMULATOR_UTILS_H */

