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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>        /* For mode constants */
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
 

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

	theSharedMemoryStruct->fileDescriptor = shm_open (theSharedMemoryStruct->sharedMemoryObjectName, (O_CREAT | O_RDWR), (ALLPERMS));
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

