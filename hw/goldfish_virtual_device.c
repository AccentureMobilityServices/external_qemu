/*
 *  goldfish_virtual_device.c
 *
 *  Android virtual Qemu device.
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

#include "qemu_file.h"
#include "goldfish_device.h"
#include "qemu_debug.h"
#include "android/globals.h"
#include "gles2emulator_utils.h"

#include <signal.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include "gles2_emulator_constants.h"

//#define DEBUG 1

#if DEBUG
#  define  DBGPRINT(...) dprintn(__VA_ARGS__)
#else
#  define  DBGPRINT(...) ((void)0)
#endif

extern void  dprintn (const char*  fmt, ...);


/* Per-buffer structure. */
struct goldfish_virtualDevice_buff {
	uint32_t	bufferTag;
	uint32_t	bufferSize;
	uint32_t	bufferAvailable;
	uint32_t	transferSize;
    uint32_t	bufferOffset;
    uint32_t	guestDataAddress;
    uint8*		hostDataAddress;
};

/* The device parameters - IRQ status, buffers, etc. */
struct goldfish_virtualDevice_device_parameters {
	int signalType, signalValue;
	pthread_mutex_t	parametersMutex;
    struct goldfish_device dev;
    uint32_t	int_status;
    uint32_t	int_enable;
	uint32_t	numberOfBuffers;
	uint8		*hostBaseStructureAddress;
	uint32_t	eachBufferSize;
    uint32_t	totalBuffersLength;
    uint32_t	hostBufferLength;
    int input_buffer_1_available_count;
    int input_buffer_2_available_count;
    int current_input_buffer;
    int current_output_buffer;
	struct goldfish_virtualDevice_buff output_buffer_1[1];
	struct goldfish_virtualDevice_buff output_buffer_2[1];
	struct goldfish_virtualDevice_buff input_buffer_1[1];
	struct goldfish_virtualDevice_buff input_buffer_2[1];
	struct hostSharedMemoryStruct theSharedMemoryStruct;
	struct hostSharedMemoryStruct theResetSemaphoreStruct;
	struct hostIPCStruct theInputIPCStruct;
	struct hostIPCStruct theOutputIPCStruct;

    uint8*		hostDataBufferAddress;
    uint32_t	hostDataBufferOffset;
    uint32_t	hostDataBufferSize;
    uint32_t	hostDataBufferFreeBytes;
	struct 		hostSharedMemoryStruct theHostDataBuffer;
};

/* Used to reference the device for testing. */
struct goldfish_virtualDevice_device_parameters *deviceParametersGlobal;

static void
goldfish_virtualDevice_output_filler (void *opaque);


/* Initialise the buffer. */
static void
goldfish_virtualDevice_buff_init (struct goldfish_virtualDevice_buff *theBuffer, uint32_t bufferNumber, uint32_t theBufferOffset, uint32_t theBufferSize, uint8 *theBufferAddress)
{
	theBuffer->bufferTag = 'BUFx';
	*(char *)(&theBuffer->bufferTag) = '0' + bufferNumber;		/* Test tag to identify it in memory. */
    theBuffer->bufferSize = theBufferSize;
    theBuffer->bufferOffset = theBufferOffset;
	theBuffer->hostDataAddress = theBufferAddress;
	theBuffer->transferSize = 0;
	theBuffer->guestDataAddress = 0;
}


/* Reset the buffer's offset. */
static void
goldfish_virtualDevice_buff_reset (struct goldfish_virtualDevice_buff *theBuffer)
{
    theBuffer->bufferOffset = 0;
    theBuffer->transferSize = 0;
}


/* Get buffer's length. */
static uint32_t
goldfish_virtualDevice_buff_length (struct goldfish_virtualDevice_buff *theBuffer)
{
    return theBuffer->transferSize;
}


/* Dynamically allocate and resize the host buffer as needed. */
static void
goldfish_virtualDevice_buff_check_and_reallocate(struct goldfish_virtualDevice_buff *theBuffer, uint32_t theSize)
{
    if (theBuffer->bufferSize < theSize) {
       theBuffer->hostDataAddress = qemu_realloc(theBuffer->hostDataAddress, theSize);
       theBuffer->bufferSize = theSize;
    }
}


/* Get how many bytes are availbale in the buffer. */
static int
goldfish_virtualDevice_buff_available (struct goldfish_virtualDevice_buff *theBuffer)
{
    return theBuffer->transferSize - theBuffer->bufferOffset;
}


/* Set the address to guest's - this is the emulated device's address from within the device. */
static void
goldfish_virtualDevice_buff_set_guest_address (struct goldfish_virtualDevice_buff *theBuffer, uint32_t theAddress)
{
    theBuffer->guestDataAddress = theAddress;
}

/* Set the address to the host's - this is the host-side/Qemu's address. */
static void
goldfish_virtualDevice_buff_set_host_address (struct goldfish_virtualDevice_buff *theBuffer, uint8 *theAddress)
{
    theBuffer->hostDataAddress = theAddress;
}


/* TODO: Currently hardcoded to BUFFER_SIZE - can be made dynamic. */
/* Set length of the buffer. */
static void
goldfish_virtualDevice_buff_set_length (struct goldfish_virtualDevice_buff *theBuffer, uint32_t theLength)
{
    theBuffer->transferSize = theLength;
    theBuffer->bufferOffset = 0;
    goldfish_virtualDevice_buff_check_and_reallocate (theBuffer, theLength);		/* Dummy function template for the TODO: above. */
}


/* Read data from guest. */
static void
goldfish_virtualDevice_buff_read (struct goldfish_virtualDevice_buff *theBuffer)
{
    cpu_physical_memory_read (theBuffer->guestDataAddress, theBuffer->hostDataAddress, theBuffer->transferSize);
}


/* Write data to guest. */
static void
goldfish_virtualDevice_buff_write (struct goldfish_virtualDevice_buff *theBuffer)
{
    cpu_physical_memory_write (theBuffer->guestDataAddress, theBuffer->hostDataAddress, theBuffer->transferSize);
}


/* Test to write data to host. */
static int
write_data_to_host_test (char *whereToPutIt, int bytes_available_to_write)
{
	static char *currentMessagePointer;
	char *theMessage = "[HOST:  This is some data that should appear in your host buffer!] ";
	int bytes_done;


	currentMessagePointer = theMessage;
	bytes_done = 0;

	while (bytes_available_to_write && *currentMessagePointer) {
		*(whereToPutIt++) = *(currentMessagePointer++);
		bytes_available_to_write--;
		bytes_done++;
	}
	return bytes_done;
}


/* Test to write data to host. */
static int
copy_data_to_host_buffer (char *sourceData, char *whereToPutIt, int bytes_available_to_write, struct goldfish_virtualDevice_device_parameters *theDevice)
{
	int bytes_done;


	bytes_done = 0;

	while (bytes_available_to_write) {
		whereToPutIt[theDevice->hostDataBufferOffset++] = *(sourceData++);
		if (theDevice->hostDataBufferOffset >= theDevice->hostDataBufferSize)
		{
			theDevice->hostDataBufferOffset = 0;
		}
		bytes_available_to_write--;
		bytes_done++;
	}
	return bytes_done;
}


/* Send data from the guest buffer to the host - when guest module does a 'write' operation.  The data is already present in the buffer which we can access directly.
*  There is no need to send it anywhere, but the placeholder is here in case the data needs to be relayed elsewhere - the 'write_data_to_host_test' example highlights
*  what can be done with the buffer.
*  Basically, this is the data 'consumer'.
*/
static int
goldfish_virtualDevice_buff_send_to_host (struct goldfish_virtualDevice_buff *theBuffer, int bytes_free, struct goldfish_virtualDevice_device_parameters *theDevice)
{
    int  bytes_processed, bytes_to_write = theBuffer->transferSize;


    DBGPRINT ( "[INFO (%s)] : Size = %d\n", __FUNCTION__, bytes_to_write );

    if (bytes_to_write > bytes_free)
        bytes_to_write = bytes_free;

    bytes_processed = copy_data_to_host_buffer (theBuffer->hostDataAddress + theBuffer->bufferOffset, theDevice->hostDataBufferAddress, bytes_to_write, theDevice);

    theBuffer->bufferOffset += bytes_processed;
    theBuffer->transferSize -= bytes_processed;
    return bytes_processed;
}


/* Test to read data from host (which gets sent to the guest). */
static int
get_data_from_host_test (char *whereToPutIt, int bytes_available_to_write)
{
	static char *currentMessagePointer;
	char *theMessage = "[GUEST:  This is data that you should be seeing in your guest!] ";
	int bytes_done;


	currentMessagePointer = theMessage;
	bytes_done = 0;

	while (bytes_available_to_write && *currentMessagePointer) {
		*(whereToPutIt++) = *(currentMessagePointer++);
		bytes_available_to_write--;
		bytes_done++;
	}
	return bytes_done;
}


/* Forward any host data to the guest - when guest module does a 'read' operation.  The function 'get_data_from_host_test' serves as a sample data provider.
*  Basically, this is the data 'provider'.
*/
static int
goldfish_virtualDevice_buff_receive_from_host (struct goldfish_virtualDevice_buff *theBuffer, int available_bytes, struct goldfish_virtualDevice_device_parameters *theDevice)
{
    int     bytes_left = theBuffer->bufferSize - theBuffer->bufferOffset;
    int     bytes_available = (available_bytes > bytes_left) ? bytes_left : available_bytes;
    int     bytes_done;

    bytes_done = get_data_from_host_test (theBuffer->hostDataAddress + theBuffer->bufferOffset, bytes_available);

    if (bytes_done == 0)
        return 0;

	/* Bung the provided data into the guest. */
    cpu_physical_memory_write (theBuffer->guestDataAddress + theBuffer->bufferOffset, theBuffer->hostDataAddress, bytes_done);
    theBuffer->bufferOffset += bytes_done;

    return bytes_done;
}


/*
save and retrieve:
To support qemu hibernation if needed
*/
/* Store Qemu save state. */
static void
goldfish_virtualDevice_store_state (struct goldfish_virtualDevice_buff *theBuffer, QEMUFile *fp)
{
//    qemu_put_be32 (fp, theBuffer->hostBaseBufferAddress);
//    qemu_put_be32 (fp, theBuffer->bufferSize);
//    qemu_put_be32 (fp, theBuffer->bufferOffset);
//    qemu_put_buffer (fp, theBuffer->hostDataAddress, theBuffer->length);
}


/* Retrieve Qemu saved state. */
static void
goldfish_virtualDevice_retrieve_state (struct goldfish_virtualDevice_buff *theBuffer, QEMUFile *fp)
{
//    theBuffer->hostBaseBufferAddress = qemu_get_be32 (fp);
//    theBuffer->bufferSize  = qemu_get_be32 (fp);
//    theBuffer->bufferOffset  = qemu_get_be32 (fp);
//    goldfish_virtualDevice_buff_set_length (theBuffer, theBuffer->bufferSize);
//    qemu_get_buffer (fp, theBuffer->hostDataAddress, theBuffer->bufferSize);
}

#define  VIRTUALDEVICE_STATE_SAVE_VERSION  2

#define  QFIELD_STRUCT   struct goldfish_virtualDevice_device_parameters
QFIELD_BEGIN(virtualDevice_state_fields)
    QFIELD_INT32(int_status),
    QFIELD_INT32(int_enable),
//    QFIELD_INT32(input_buffer_1_available_count),
//    QFIELD_INT32(input_buffer_2_available_count),
//    QFIELD_INT32(current_input_buffer),
//    QFIELD_INT32(current_output_buffer),
QFIELD_END


/* Save state of this device for Qemu. */
static void
virtualDevice_state_save (QEMUFile *fp, void *opaque)
{
    struct goldfish_virtualDevice_device_parameters *theDevice = opaque;


    qemu_put_struct (fp, virtualDevice_state_fields, theDevice);

//    goldfish_virtualDevice_store_state (theDevice->input_buffer_1, fp);
//    goldfish_virtualDevice_store_state (theDevice->input_buffer_2, fp);
//    goldfish_virtualDevice_store_state (theDevice->output_buffer_1, fp);
//    goldfish_virtualDevice_store_state (theDevice->output_buffer_2, fp);
}


/* Load state of this device from Qemu. */
static int
virtualDevice_state_load (QEMUFile *fp, void *opaque, int version_id)
{
    struct goldfish_virtualDevice_device_parameters *theDevice = opaque;
    int ret;


    if (version_id != VIRTUALDEVICE_STATE_SAVE_VERSION)
        return -1;

    ret = qemu_get_struct (fp, virtualDevice_state_fields, theDevice);
    if (!ret) {
//		goldfish_virtualDevice_retrieve_state (theDevice->input_buffer_1, fp);
//		goldfish_virtualDevice_retrieve_state (theDevice->input_buffer_2, fp);
//		goldfish_virtualDevice_retrieve_state (theDevice->output_buffer_1, fp);
//		goldfish_virtualDevice_retrieve_state (theDevice->output_buffer_2, fp);
    }
    return -1;
}


/* Enable the device - basically reset the buffers :P */
static void
enable_virtualDevice (struct goldfish_virtualDevice_device_parameters *theDevice, int enable)
{
        goldfish_virtualDevice_buff_reset (theDevice->output_buffer_1);
        goldfish_virtualDevice_buff_reset (theDevice->output_buffer_2);
        goldfish_virtualDevice_buff_reset (theDevice->input_buffer_1);
        goldfish_virtualDevice_buff_reset (theDevice->input_buffer_2);
		theDevice->current_output_buffer = 0;
}


/* Prepare to recive data from guest. This is when the guest requests to send us data (write). */
static void
start_write_request (struct goldfish_virtualDevice_device_parameters *s, uint32_t count)
{
    DBGPRINT ( "[INFO (%s)] : Count = %d\n", __FUNCTION__, count );
    goldfish_virtualDevice_buff_set_length (s->input_buffer_1, count);
    goldfish_virtualDevice_buff_set_length (s->input_buffer_2, count);
    s->input_buffer_1_available_count = count;
    s->input_buffer_2_available_count = count;
}


/* Process command request to read a parameter from our device. */
static uint32_t
goldfish_virtualDevice_readcommand_requested (void *opaque, target_phys_addr_t offset)
{
    uint32_t ret;
    struct goldfish_virtualDevice_device_parameters *theDevice = opaque;
 
	DBGPRINT ("[INFO (%s)] : Command %d.\n", __FUNCTION__, offset);

    switch (offset) {
		case VIRTUALDEVICE_INT_STATUS:
		    // Return current buffer status flags.
			DBGPRINT ("    (more) : VIRTUALDEVICE_INT_STATUS\n");
		    ret = theDevice->int_status & theDevice->int_enable;
		    if (ret) {
		        goldfish_device_set_irq (&theDevice->dev, 0, 0);
		    }
	    	return ret;

		case VIRTUALDEVICE_INPUT_BUFFER_1_AVAILABLE:
			DBGPRINT ("    (more) : Command = VIRTUALDEVICE_INPUT_BUFFER_1_AVAILABLE : 0x%x\n", offset);
			DBGPRINT ("    (more) : Input buffer 1 ready with offset: %d.\n", offset);
			goldfish_virtualDevice_buff_write (theDevice->input_buffer_1);
			return theDevice->input_buffer_1_available_count;

		case VIRTUALDEVICE_INPUT_BUFFER_2_AVAILABLE:
			DBGPRINT ("    (more) : Command = VIRTUALDEVICE_INPUT_BUFFER_2_AVAILABLE : 0x%x\n", offset);
			DBGPRINT ("    (more) : Input buffer 2 ready with offset: %d.\n", offset);
			goldfish_virtualDevice_buff_write (theDevice->input_buffer_2);
			return theDevice->input_buffer_2_available_count;

		default:
//			cpu_abort (cpu_single_env, "%s: Bad command: %x\n", __FUNCTION__, offset);     //will shut down qemu if gets a bad command if enabled
			DBGPRINT ("    (more) : Bad command: %x\n", offset);
			return 0;
    }
}


/* Process a command requested to write a parameter to our device. */
static void
goldfish_virtualDevice_writecommand_requested (void *opaque, target_phys_addr_t offset, uint32_t val)
{
    struct goldfish_virtualDevice_device_parameters *theDevice = opaque;
	target_phys_addr_t length;
	void *theMemory;
	int semaphore_value;

	DBGPRINT ("[INFO (%s)] : Pointer: 0x%x - Command val: 0x%x - data: 0x%x.\n", __FUNCTION__, opaque, offset, val);

	switch (offset) {
		case VIRTUALDEVICE_INITIALISE:
			DBGPRINT ("    (more) : Command = VIRTUALDEVICE_INITIALISE : 0x%x)\n", offset);
            enable_virtualDevice (theDevice, val);
            theDevice->int_enable = val;
            theDevice->int_status = (VIRTUALDEVICE_INT_OUTPUT_BUFFER_1_EMPTY | VIRTUALDEVICE_INT_OUTPUT_BUFFER_2_EMPTY);
            goldfish_device_set_irq (&theDevice->dev, 0, (theDevice->int_status & theDevice->int_enable));
            break;

		case SET_INPUT_BUFFER_1_ADDRESS:
				DBGPRINT ("    (more) : Command = SET_INPUT_BUFFER_1_ADDRESS : 0x%x\n", offset);
				DBGPRINT ("    (more) : Host address : 0x%x\n", qemu_get_ram_ptr (val));
				goldfish_virtualDevice_buff_set_guest_address (theDevice->input_buffer_1, val);
				break;
		case SET_INPUT_BUFFER_2_ADDRESS:
				DBGPRINT ("    (more) : Command = SET_INPUT_BUFFER_2_ADDRESS : 0x%x\n", offset);
				DBGPRINT ("    (more) : Host address : 0x%x\n", qemu_get_ram_ptr (val));
				goldfish_virtualDevice_buff_set_guest_address (theDevice->input_buffer_2, val);
				break;
		case SET_OUTPUT_BUFFER_1_ADDRESS:
				DBGPRINT ("    (more) : Command = SET_OUTPUT_BUFFER_1_ADDRESS : 0x%x\n", offset);
				DBGPRINT ("    (more) : Host address : 0x%x\n", qemu_get_ram_ptr (val));
				goldfish_virtualDevice_buff_set_guest_address (theDevice->output_buffer_1, val);
				break;
		case SET_OUTPUT_BUFFER_2_ADDRESS:
				DBGPRINT ("    (more) : Command = SET_OUTPUT_BUFFER_2_ADDRESS : 0x%x\n", offset);
				DBGPRINT ("    (more) : Host address : 0x%x\n", qemu_get_ram_ptr (val));
				goldfish_virtualDevice_buff_set_guest_address (theDevice->output_buffer_2, val);
				break;

		case VIRTUALDEVICE_IOCTL_GRALLOC_ALLOCATED_REGION_INFO:
				/* Guest cpu can return physical offset or we can use qemu_ram_addr_from_host . */
				DBGPRINT ("    (more) : Command = VIRTUALDEVICE_IOCTL_GRALLOC_ALLOCATED_REGION_INFO : 0x%x\n", offset);
				DBGPRINT ("    (more) : Gralloc region address : 0x%x\n", val);
				break;

		case VIRTUALDEVICE_IOCTL_SYSTEM_RESET:
				DBGPRINT ("    (more) : Command = VIRTUALDEVICE_IOCTL_SYSTEM_RESET : 0x%x\n", offset);
				goldfish_virtualDevice_output_filler (theDevice);		/* Send this buffer over to the host buffer */
				sem_post (theDevice->theResetSemaphoreStruct.semaphore);  /* Signal to the host that a system reset has been called. */
				/* Wait until host clears the semaphore */				
				enable_virtualDevice (theDevice, 0);					/* Reset the buffers */
				theDevice->hostDataBufferOffset = 0;
				break;

		case VIRTUALDEVICE_IOCTL_SIGNAL_BUFFER_SYNC:
				/* Guest cpu can return physical offset or we can use qemu_ram_addr_from_host . */
//				DBGPRINT ("    (more) : Command = VIRTUALDEVICE_IOCTL_SIGNAL_SYNC : 0x%x\n", offset);
//				DBGPRINT ("    (more) : Sync value : 0x%x\n", val);
				pthread_mutex_lock (&theDevice->parametersMutex);
				theDevice->signalType = VIRTUALDEVICE_IOCTL_SIGNAL_BUFFER_SYNC;
				theDevice->signalValue = val;
				pthread_mutex_unlock (&theDevice->parametersMutex);
				goldfish_virtualDevice_output_filler (theDevice);		/* Send this buffer over to the host buffer */
				sem_post (theDevice->theSharedMemoryStruct.semaphore);
				break;

		/* Signalled when guest module has an output buffer available so we can write to it. */
        case VIRTUALDEVICE_OUTPUT_BUFFER_1_AVAILABLE:
				DBGPRINT ("    (more) : Command = VIRTUALDEVICE_OUTPUT_BUFFER_1_AVAILABLE : 0x%x  Len: %d\n", offset, val);
	            if (theDevice->current_output_buffer == 0) theDevice->current_output_buffer = 1;
				goldfish_virtualDevice_buff_set_length (theDevice->output_buffer_1, val);
				goldfish_virtualDevice_buff_read (theDevice->output_buffer_1);
				theDevice->int_status &= ~VIRTUALDEVICE_INT_OUTPUT_BUFFER_1_EMPTY;
				goldfish_virtualDevice_output_filler (theDevice);		/* Send this buffer over to the host buffer */
				/* Signal to host this buffer is available with data, theDevice->signalValue holds number of bytes available. */
				pthread_mutex_lock (&theDevice->parametersMutex);
				theDevice->signalType = VIRTUALDEVICE_OUTPUT_BUFFER_1_AVAILABLE;
				theDevice->signalValue = val;
				pthread_mutex_unlock (&theDevice->parametersMutex);
//				sem_post (theDevice->theSharedMemoryStruct.semaphore);
            	break;

        case VIRTUALDEVICE_OUTPUT_BUFFER_2_AVAILABLE:
 				DBGPRINT ("    (more) : Command = VIRTUALDEVICE_OUTPUT_BUFFER_2_AVAILABLE : 0x%x  Len: %d\n", offset, val);
         	  	if (theDevice->current_output_buffer == 0) theDevice->current_output_buffer = 2;
				goldfish_virtualDevice_buff_set_length (theDevice->output_buffer_2, val);
				goldfish_virtualDevice_buff_read (theDevice->output_buffer_2);
				theDevice->int_status &= ~VIRTUALDEVICE_INT_OUTPUT_BUFFER_2_EMPTY;
				goldfish_virtualDevice_output_filler (theDevice);		/* Send this buffer over to the host buffer */
				/* Signal to host this buffer is available with data, theDevice->signalValue holds number of bytes available. */
				pthread_mutex_lock (&theDevice->parametersMutex);
				theDevice->signalType = VIRTUALDEVICE_OUTPUT_BUFFER_2_AVAILABLE;
				theDevice->signalValue = val;
				pthread_mutex_unlock (&theDevice->parametersMutex);
//				sem_post (theDevice->theSharedMemoryStruct.semaphore);
          		 break;

		/* Signalled when guest module wants to start writing to this device. */
        case VIRTUALDEVICE_START_INPUT:
  				DBGPRINT ("    (more) : Command = VIRTUALDEVICE_START_INPUT : 0x%x\n", offset);
         	    if (theDevice->current_input_buffer == 0) theDevice->current_input_buffer = 1;
         	    start_write_request (theDevice, val);			/* Start with first buffer. */
          	    theDevice->int_status &= ~VIRTUALDEVICE_INT_INPUT_BUFFER_1_FULL;
         	    goldfish_device_set_irq (&theDevice->dev, 0, (theDevice->int_status & theDevice->int_enable));
           		break;

		default:
//			cpu_abort (cpu_single_env, "%s: Bad command: %x\n", __FUNCTION__, offset);	//will shut down qemu if gets a bad command if enabled
				DBGPRINT ("    (more) : Bad command: %x\n", offset);
	}
}


/*  Output buffer handler. */
static void
goldfish_virtualDevice_output_filler (void *opaque)
{
    struct goldfish_virtualDevice_device_parameters *theDevice = opaque;
    int new_status = 0;
    uint32_t buffer_exhausted_signal = 1;
    long resultCode;


    /* Wait/loop until we have something to write and either buffer has something.*/
    while (theDevice->hostDataBufferFreeBytes && theDevice->current_output_buffer) {

        /* Write data in buffer 1 if it is the current one. */
        while (free && theDevice->current_output_buffer == 1) {
            int  written = goldfish_virtualDevice_buff_send_to_host (theDevice->output_buffer_1, theDevice->hostDataBufferFreeBytes, theDevice);
            if (written) {
				DBGPRINT ("[INFO (%s)] : Sent %d bytes to host buffer_1\n", __FUNCTION__, written);
                theDevice->hostDataBufferFreeBytes -= written;

				/* Swap buffer if we are done with it. */
                if (goldfish_virtualDevice_buff_length (theDevice->output_buffer_1) == 0) {
                    new_status |= VIRTUALDEVICE_INT_OUTPUT_BUFFER_1_EMPTY;
                    theDevice->current_output_buffer = (goldfish_virtualDevice_buff_length (theDevice->output_buffer_2) ? 2 : 0);
                }
            } else {
                break;
            }
        }

        /* Write data in buffer 2 if current. */
        while (theDevice->hostDataBufferFreeBytes && theDevice->current_output_buffer == 2) {
            int  written = goldfish_virtualDevice_buff_send_to_host (theDevice->output_buffer_2, theDevice->hostDataBufferFreeBytes, theDevice);
            if (written) {
				DBGPRINT ("[INFO (%s)] : Sent %d bytes to host buffer_2\n", __FUNCTION__, written);
                theDevice->hostDataBufferFreeBytes -= written;

                if (goldfish_virtualDevice_buff_length (theDevice->output_buffer_2) == 0) {
                    new_status |= VIRTUALDEVICE_INT_OUTPUT_BUFFER_2_EMPTY;
                    theDevice->current_output_buffer = (goldfish_virtualDevice_buff_length (theDevice->output_buffer_1) ? 1 : 0);
                }
            } else {
                break;
            }
        }
    }

	/* Check if we have exhausted host buffer. */
	if (theDevice->hostDataBufferFreeBytes <= 0)
	{
//		theDevice->theOutputIPCStruct.theMessage = (char *)&buffer_exhausted_signal;
//		theDevice->theOutputIPCStruct.theMessageLength = 4;
//		theDevice->theOutputIPCStruct.thePriority = 0;

		theDevice->theOutputIPCStruct.theMessage = "Buffeh!";
		theDevice->theOutputIPCStruct.theMessageLength = strlen(theDevice->theOutputIPCStruct.theMessage) + 1;
		theDevice->theOutputIPCStruct.thePriority = 0;
		gles2emulator_utils_ipc_send_message (&theDevice->theOutputIPCStruct);

//		gles2emulator_utils_ipc_send_message (&theDevice->theOutputIPCStruct);
//		if (resultCode = -1) DBGPRINT ("[INFO (%s)] : ------------------------------ BLOM!.\n", __FUNCTION__);
//		resultCode = gles2emulator_utils_ipc_receive_message_with_timeout (&theDevice->theInputIPCStruct, 2.5);
//		if (resultCode = -1) DBGPRINT ("[INFO (%s)] : ------------------------------ Timeout on waiting for buffer exhausted confirmation.\n", __FUNCTION__);

		theDevice->hostDataBufferFreeBytes = theDevice->hostDataBufferSize;		// At the moment not doing host buffer locking, so resetting this.
	}

    if (new_status && new_status != theDevice->int_status) {
        theDevice->int_status |= new_status;
        goldfish_device_set_irq (&theDevice->dev, 0, (theDevice->int_status & theDevice->int_enable));
    }
}


/* Input buffer handler. */
static void
goldfish_virtualDevice_input_filler (void *opaque, int bytes_available)
{
    struct goldfish_virtualDevice_device_parameters *theDevice = opaque;
    int new_status = 0;


    if (goldfish_virtualDevice_buff_available (theDevice->input_buffer_1) != 0 )
	{
		while ((bytes_available > 0)  && (theDevice->current_input_buffer == 1)) {
			int  read = goldfish_virtualDevice_buff_receive_from_host (theDevice->input_buffer_1, bytes_available, theDevice);
			if (read == 0)
				break;
			bytes_available -= read;

			if (goldfish_virtualDevice_buff_available (theDevice->input_buffer_1) == 0) {
				DBGPRINT ("[INFO (%s)] : Input buffer 1 is full and available with %d bytes ready.\n", __FUNCTION__, goldfish_virtualDevice_buff_length (theDevice->input_buffer_1));
			   	new_status |= VIRTUALDEVICE_INT_INPUT_BUFFER_1_FULL;
				theDevice->current_output_buffer = (goldfish_virtualDevice_buff_length (theDevice->input_buffer_2 ) ? 2 : 0);
				break;
			}
		}

    } else if (goldfish_virtualDevice_buff_available (theDevice->input_buffer_2) != 0 ) {

		while ((bytes_available > 0)  && (theDevice->current_input_buffer == 2)) {
		    int  read = goldfish_virtualDevice_buff_receive_from_host (theDevice->input_buffer_2, bytes_available, theDevice);
		    if (read == 0)
		        break;
		    bytes_available -= read;

		    if (goldfish_virtualDevice_buff_available (theDevice->input_buffer_2) == 0) {
				DBGPRINT ("[INFO (%s)] : Input buffer 2 is full and available with %d bytes ready.\n", __FUNCTION__, goldfish_virtualDevice_buff_length (theDevice->input_buffer_2));
		     	new_status |= VIRTUALDEVICE_INT_INPUT_BUFFER_2_FULL;
				theDevice->current_output_buffer = (goldfish_virtualDevice_buff_length (theDevice->input_buffer_1 ) ? 1 : 0);
		        break;
        	}
		}
    } else {
		return;
	}

    if (new_status && new_status != theDevice->int_status) {
        theDevice->int_status |= new_status;
        goldfish_device_set_irq (&theDevice->dev, 0, (theDevice->int_status & theDevice->int_enable));
    }
}


/* Registered fucntions to read/write from memory. */
static CPUReadMemoryFunc
	*goldfish_virtualDevice_readfn[] = {
	goldfish_virtualDevice_readcommand_requested,
	goldfish_virtualDevice_readcommand_requested,
	goldfish_virtualDevice_readcommand_requested
};


static CPUWriteMemoryFunc
	*goldfish_virtualDevice_writefn[] = {
	goldfish_virtualDevice_writecommand_requested,
	goldfish_virtualDevice_writecommand_requested,
	goldfish_virtualDevice_writecommand_requested
};


static void
goldfish_virtualDevice_signal_function (int signo, siginfo_t *theSigVal, void *ignored)
{
struct mq_attr theQueueAttributes;
uint32_t theCommand;
int theSize;
int numMessages, i;
struct goldfish_virtualDevice_device_parameters *theDevice = (struct goldfish_virtualDevice_device_parameters *) theSigVal->si_value.sival_ptr;



	if (gles2emulator_utils_ipc_get_attributes (&theDevice->theInputIPCStruct) == -1) DBGPRINT("    (more) : Cannot get queue attributes.\n");

//	if (mq_getattr(theDevice->theInputIPCStruct.theMessageQueue, &theQueueAttributes) == -1) DBGPRINT("    (more) : Cannot get queue attributes.\n");

	numMessages = theDevice->theInputIPCStruct.theQueueAttributes.mq_curmsgs;
	DBGPRINT ("[INFO (%s)] : <<<<<<<<<!!!!<<<<<<<<<  Received signal from message queue, amount: %d.  >>>>>>>>>>>>>>>>>>>>>>\n\n\n", __FUNCTION__, numMessages);
	
	for (i = 0; i < numMessages; i++)
	{
		theSize = gles2emulator_utils_ipc_receive_message (&theDevice->theInputIPCStruct);
//		theSize = mq_receive(theDevice->theInputIPCStruct.theMessageQueue, theDevice->theInputIPCStruct.theMessageBuffer, theDevice->theInputIPCStruct.theMessageBufferSize, NULL);
//	if (theSize == -1) DBGPRINT("    (more) : Error in mq_receive.\n");

		DBGPRINT("    (more) : Read %ld bytes from Message queue: %s\n", (long) theSize, theDevice->theInputIPCStruct.theMessageBuffer);


	theCommand = *(uint32_t *)theDevice->theInputIPCStruct.theMessageBuffer;
	if (theCommand == 0x4703F322)
	{

		theCommand = *(uint32_t *)(theDevice->theInputIPCStruct.theMessageBuffer + 16);
		switch (theCommand)
		{
			// Cheapo test before adding in command structure.
			case 8:
				DBGPRINT("    (more) : Host buffer pointer reset requested!\n");
				deviceParametersGlobal->hostDataBufferOffset = 0;
		}
	}

	}

	/* Re-register signal request. */	
	mq_notify(theDevice->theInputIPCStruct.theMessageQueue, &theDevice->theInputIPCStruct.theSignalEvent);

//	gles2emulator_utils_ipc_set_notifier_function (&theDevice->theInputIPCStruct, theDevice);
}


/* Initialise the device. Called here when the board is booted. */
void
goldfish_virtualDevice_init (uint32_t base, int id)
{
	uint32_t i, j;
    	struct goldfish_virtualDevice_device_parameters *theDevice;
	struct hostSharedMemoryStruct theSharedMemoryStruct;
	struct hostSharedMemoryStruct theHostDataBuffer;


	DBGPRINT ("[INFO (%s)] : Initialising virtual device...\n", __FUNCTION__);

	theSharedMemoryStruct.sharedMemoryObjectName = "qemu_vd1_params";
	theSharedMemoryStruct.size = sizeof (struct goldfish_virtualDevice_device_parameters);
	gles2emulator_utils_create_sharedmemory_file (&theSharedMemoryStruct);
	gles2emulator_utils_map_sharedmemory_file (&theSharedMemoryStruct);

	if (theSharedMemoryStruct.actualAddress != 0) {

		memset(theSharedMemoryStruct.actualAddress, 0, theSharedMemoryStruct.size);
		theDevice = (struct goldfish_virtualDevice_device_parameters *)theSharedMemoryStruct.actualAddress;
		theDevice->dev.name = "virtual-device";			/* NOTE:  Limited to <>32 chars, this is the name the kernel module picks up to attach to. */
		theDevice->dev.id = id;
		theDevice->dev.base = base;
		theDevice->dev.size = 0x1000;
		theDevice->dev.irq_count = 1;
		theDevice->eachBufferSize = CommandBufferSize;					
		theDevice->numberOfBuffers = NumberCommandBuffers * 2;	// *2 required because there are both Input and Output buffers		

		theDevice->totalBuffersLength = theDevice->eachBufferSize * theDevice->numberOfBuffers;

		theDevice->theHostDataBuffer.sharedMemoryObjectName = "qemu_vd1_inputBuffer";
		theDevice->theHostDataBuffer.sharedMemorySegmentKey = 0x1339;							/* For shared segments, would allow others to loas shared memory using key */
		theDevice->theHostDataBuffer.size = HostBufferSize;
		theDevice->theHostDataBuffer.requiredAddress = NULL;								/* NULL = find me some memory, don't try to match */
		gles2emulator_utils_create_sharedmemory_file (&theDevice->theHostDataBuffer);
		gles2emulator_utils_map_sharedmemory_file (&theDevice->theHostDataBuffer);

		theDevice->hostDataBufferAddress = theDevice->theHostDataBuffer.actualAddress;
		theDevice->hostDataBufferOffset = 0;
		theDevice->hostDataBufferSize = theDevice->theHostDataBuffer.size;
		theDevice->hostDataBufferFreeBytes = theDevice->hostDataBufferSize;

		goldfish_virtualDevice_buff_init (theDevice->output_buffer_1, 1, 0, 0, 0);
		goldfish_virtualDevice_buff_init (theDevice->output_buffer_2, 1, 0, 0, 0);
		goldfish_virtualDevice_buff_init (theDevice->input_buffer_1, 1, 0, 0, 0);
		goldfish_virtualDevice_buff_init (theDevice->input_buffer_2, 1, 0, 0, 0);

		DBGPRINT ("[INFO (%s)] : Host buffers addr: 0x%x\n", __FUNCTION__, theDevice->output_buffer_1->hostDataAddress);

		theDevice->theSharedMemoryStruct.sharedMemorySemaphoreName = "qemu_vd1_semaphore";
		gles2emulator_utils_create_sharedmemory_semaphore (&theDevice->theSharedMemoryStruct);
		theDevice->theResetSemaphoreStruct.sharedMemorySemaphoreName = "qemu_vd1_systemReset_sem";
		gles2emulator_utils_create_sharedmemory_semaphore (&theDevice->theResetSemaphoreStruct);


		theDevice->theInputIPCStruct.theMessageBufferSize = 8192;
		theDevice->theInputIPCStruct.maxMessages = 32;
		theDevice->theOutputIPCStruct.theMessageBufferSize = 8192;
		theDevice->theOutputIPCStruct.maxMessages = 32;
		
		DBGPRINT ("    (more) : Creating input IPC message pipe...  ");
		theDevice->theInputIPCStruct.theMessageBuffer = malloc(theDevice->theInputIPCStruct.theMessageBufferSize);
		theDevice->theInputIPCStruct.theMessageQueueName = "/gles2emulator_msgQInput";
		theDevice->theInputIPCStruct.theFileMode = O_CREAT | O_RDWR;
		gles2emulator_utils_ipc_create (&theDevice->theInputIPCStruct);
		DBGPRINT ("OK\n");

//		DBGPRINT ("    (more) : Creating output IPC message pipe...  ");
//		theDevice->theOutputIPCStruct.theMessageBuffer = malloc(theDevice->thePutputIPCStruct.theMessageBufferSize);
//		theDevice->theOutputIPCStruct.theMessageQueueName = "/gles2emulator_msgQOutput";
//		theDevice->theOutputIPCStruct.theFileMode = (O_CREAT|O_WRONLY);
//		gles2emulator_utils_ipc_create (&theDevice->theOutputIPCStruct, 32, 1024);
//		DBGPRINT ("OK\n");

		if (!gles2emulator_utils_ipc_get_attributes (&theDevice->theInputIPCStruct))
		{
			i = theDevice->theInputIPCStruct.theQueueAttributes.mq_curmsgs;
			for (j = 0; j < i; j++)
			{
				gles2emulator_utils_ipc_receive_message (&theDevice->theInputIPCStruct);
				DBGPRINT("Clearing message cobwebs: %s\n", theDevice->theInputIPCStruct.theMessageBuffer);
			}
		}

		theDevice->theInputIPCStruct.theThreadPointer = goldfish_virtualDevice_signal_function;
		gles2emulator_utils_ipc_set_notifier_function (&theDevice->theInputIPCStruct, theDevice);

//		theDevice->theOutputIPCStruct.theMessage = "Test!";
//		theDevice->theOutputIPCStruct.theMessageLength = strlen(theDevice->theOutputIPCStruct.theMessage) + 1;
//		theDevice->theOutputIPCStruct.thePriority = 0;
//		gles2emulator_utils_ipc_send_message (&theDevice->theOutputIPCStruct);

		if (pthread_mutex_init (&theDevice->parametersMutex, NULL)) {
			DBGPRINT ("    (WARN) : Could not initialise mutex!  Device not installed.\n");
			gles2emulator_utils_remove_sharedmemory_semaphore (&theDevice->theSharedMemoryStruct);
			gles2emulator_utils_unmap_sharedmemory (&theDevice->theSharedMemoryStruct);
			gles2emulator_utils_close_sharedmemory_file (&theDevice->theSharedMemoryStruct);
			return;
		}

		goldfish_device_add (&theDevice->dev, goldfish_virtualDevice_readfn, goldfish_virtualDevice_writefn, theDevice);

		/* Register machine to save the state from Qemu. */
		register_savevm ("virtualdevice1_state", 0, VIRTUALDEVICE_STATE_SAVE_VERSION, virtualDevice_state_save, virtualDevice_state_load, theDevice);
		deviceParametersGlobal = theDevice;
		DBGPRINT ("    (more) : Done, all ok!\n");
	} else {
		DBGPRINT ("    (WARN) : Couldn't allocate memory!  Device not installed.\n");
		return;
	}
}

