/*
 *  sharedmemory_allocator.h
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

#ifndef SHAREDMEMORY_ALLOCATOR_H
#define SHAREDMEMORY_ALLOCATOR_H

#include "qemu-common.h"
#include <stdlib.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#endif /* SHAREDMEMORY_ALLOCATOR_H */


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

void sharedMemory_allocator_create_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void sharedMemory_allocator_close_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void sharedMemory_allocator_map_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void sharedMemory_allocator_unmap_sharedmemory (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void sharedMemory_allocator_create_sharedmemory_semaphore (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void sharedMemory_allocator_remove_sharedmemory_semaphore (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void sharedMemory_allocator_create_sharedmemory_segment (struct hostSharedMemoryStruct *theSharedMemoryStruct);

