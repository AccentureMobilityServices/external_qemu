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
#include <signal.h>
#include <fcntl.h>           /* For O_* constants */
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <time.h>
#include <string.h>



struct hostSharedMemoryStruct {
	int fileDescriptor;
	size_t size;
	int sharedMemoryID;
	int sharedMemorySegmentKey;
	char *sharedMemoryObjectName;
	char *sharedMemorySemaphoreName;
	void *requiredAddress;
	void *actualAddress;
};

void gles2emulator_utils_create_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_close_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_map_sharedmemory_file (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_unmap_sharedmemory (struct hostSharedMemoryStruct *theSharedMemoryStruct);
void gles2emulator_utils_create_sharedmemory_segment (struct hostSharedMemoryStruct *theSharedMemoryStruct);
#endif
