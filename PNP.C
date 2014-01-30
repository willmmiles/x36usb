/*++

Copyright (c) 1996  Microsoft Corporation

Module Name:

    pnp.c

Abstract: NULL filter driver -- boilerplate code

Author:

    ervinp

Environment:

    Kernel mode

Revision History:


	Modified for X36 by Dhauzimmer, 02/2001
	 -> Abort held IRP during device stop and device removal conditions.
	 -> HANDLE_DEVICE_USAGE blocks removed because we're not using them.

--*/

//#define DBG 1
#include <WDM.H>

#include "filter.h"


#ifdef ALLOC_PRAGMA
        #pragma alloc_text(PAGE, VA_PnP)
        #pragma alloc_text(PAGE, GetDeviceCapabilities)
#endif


/* TEMP DEBUG */
NTSTATUS 
    D3C_CatchQuery
    (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
{
    NTSTATUS ntStatus = STATUS_SUCCESS;

    DbgPrint("D3C_CatchQuery(DeviceObject=0x%x,Irp=0x%x,Context=0x%x)",DeviceObject, Irp, Context);
                    
	// Read the data in the IRP for logging purposes.
	DbgPrint("Status: %d   Buffer: '%x'",Irp->IoStatus.Status,Irp->IoStatus.Information);
	if (Irp->IoStatus.Information != 0)
	{
		DbgPrint("Value: '%ws'",Irp->IoStatus.Information);
	};

    UNREFERENCED_PARAMETER (DeviceObject);
    UNREFERENCED_PARAMETER (Context);

    return ntStatus;
}




            
NTSTATUS VA_PnP(struct DEVICE_EXTENSION *devExt, PIRP irp)
/*++

Routine Description:

    Dispatch routine for PnP IRPs (MajorFunction == IRP_MJ_PNP)

Arguments:

    devExt - device extension for the targetted device object
    irp - IO Request Packet

Return Value:

    NT status code

--*/
{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status = STATUS_SUCCESS;
    BOOLEAN completeIrpHere = FALSE;
    BOOLEAN justReturnStatus = FALSE;
    PIRP MyIrp = NULL;
    KIRQL OldIrql;
    int temp;
    LARGE_INTEGER li_mouse_tick;

    PAGED_CODE();

    irpSp = IoGetCurrentIrpStackLocation(irp);

    DBGOUT(("VA_PnP, minorFunc = %x ", (ULONG)irpSp->MinorFunction)); 
   	
    switch (irpSp->MinorFunction){

    case IRP_MN_START_DEVICE:
        DBGOUT(("START_DEVICE")); 

        devExt->state = STATE_STARTING;

        /*
         *  First, send the START_DEVICE irp down the stack
         *  synchronously to start the lower stack.
         *  We cannot do anything with our device object
         *  before propagating the START_DEVICE this way.
         */
        IoCopyCurrentIrpStackLocationToNext(irp);
        status = CallNextDriverSync(devExt, irp);

        if (NT_SUCCESS(status)){
            /*
             *  Now that the lower stack is started,
             *  do any initialization required by this device object.
             */
            status = GetDeviceCapabilities(devExt);
            if (NT_SUCCESS(status)){                
                devExt->state = STATE_STARTED;
                // reset the mouse ticker and calibration writing thread.  Can't possibly hurt.
                li_mouse_tick.QuadPart = -1* MOUSE_TICK;
                KeSetTimer( &devExt->MouseTimer, li_mouse_tick, &devExt->MouseDPC );
                devExt->ThreadAction = save;
				KeSetEvent(&devExt->WakeupThread,0,FALSE);
            }
            else {
                devExt->state = STATE_START_FAILED;
            }
        }
        else {
            devExt->state = STATE_START_FAILED;
        }        
        completeIrpHere = TRUE;
        break;

    case IRP_MN_QUERY_STOP_DEVICE:
        /*
         *  We will pass this IRP down the driver stack.
         *  However, we need to change the default status
         *  from STATUS_NOT_SUPPORTED to STATUS_SUCCESS.
         */
        irp->IoStatus.Status = STATUS_SUCCESS;
        break;

    case IRP_MN_STOP_DEVICE:
        if (devExt->state == STATE_SUSPENDED){
            status = STATUS_DEVICE_POWER_FAILURE;
            completeIrpHere = TRUE;
        }
        else {
            /*
             *  Only set state to STOPPED if the device was
             *  previously started successfully.
             */
            if (devExt->state == STATE_STARTED){
            	KeAcquireSpinLock(&devExt->ReadLock, &OldIrql);
            		MyIrp = devExt->ReadIrp;
            		devExt->ReadIrp = NULL;
            	KeReleaseSpinLock(&devExt->ReadLock, OldIrql);
            	
				if ( MyIrp )
				{
					KeCancelTimer(&devExt->Timer);
					MyIrp->IoStatus.Status = STATUS_CANCELLED;
    				IoCompleteRequest(MyIrp, IO_NO_INCREMENT);	
					DecrementPendingActionCount( devExt );  	    			
				}
				KeCancelTimer(&devExt->MouseTimer);
				devExt->ThreadAction = standby;
				KeSetEvent(&devExt->WakeupThread,0,FALSE);

                devExt->state = STATE_STOPPED;
            }
        }
        break;
  
    case IRP_MN_QUERY_REMOVE_DEVICE:
    case IRP_MN_SURPRISE_REMOVAL:
        DBGOUT(("SURPRISE_REMOVAL")); 

        /*
         *  We will pass this IRP down the driver stack.
         *  However, we need to change the default status
         *  from STATUS_NOT_SUPPORTED to STATUS_SUCCESS.
         */
        irp->IoStatus.Status = STATUS_SUCCESS;


			DbgPrn(("Acquiring Spinlock.\n"));
           	KeAcquireSpinLock(&devExt->ReadLock, &OldIrql);
           		DbgPrn(("Grabbing read IRP.\n"));
           		MyIrp = devExt->ReadIrp;
           		devExt->ReadIrp = NULL;
           	KeReleaseSpinLock(&devExt->ReadLock, OldIrql);
			DbgPrn(("Releasing spinlock.\n"));

			if ( MyIrp != NULL )
			{
				DbgPrn(("Clearing IRP.\n"));
				KeCancelTimer(&devExt->Timer);
				MyIrp->IoStatus.Status = STATUS_DELETE_PENDING;
				DbgPrn(("Completing IRP.\n"));
    			IoCompleteRequest(MyIrp, IO_NO_INCREMENT);	
				DecrementPendingActionCount( devExt );  	
				DbgPrn(("IRP clear completed.\n"));
			}
			KeCancelTimer(&devExt->MouseTimer);
			devExt->ThreadAction = shutdown;
			KeSetEvent(&devExt->WakeupThread,0,FALSE);

        /*
         *  For now just set the STATE_REMOVING state so that
         *  we don't do any more IO.  We are guaranteed to get
         *  IRP_MN_REMOVE_DEVICE soon; we'll do the rest of
         *  the remove processing there.
         */
        devExt->state = STATE_REMOVING;

        break;

    case IRP_MN_REMOVE_DEVICE:
        /*
         *  Check the current state to guard against multiple
         *  REMOVE_DEVICE IRPs.
         */
        DBGOUT(("REMOVE_DEVICE")); 
        if (devExt->state != STATE_REMOVED){


            devExt->state = STATE_REMOVED;

			DbgPrn(("Acquiring Spinlock.\n"));
           		KeAcquireSpinLock(&devExt->ReadLock, &OldIrql);
           		DbgPrn(("Grabbing read IRP.\n"));
           		MyIrp = devExt->ReadIrp;
           		devExt->ReadIrp = NULL;
           		KeReleaseSpinLock(&devExt->ReadLock, OldIrql);
			DbgPrn(("Releasing spinlock.\n"));

			if ( MyIrp != NULL )
			{
				DbgPrn(("Clearing IRP.\n"));
				KeCancelTimer(&devExt->Timer);
				MyIrp->IoStatus.Status = STATUS_DELETE_PENDING;
				DbgPrn(("Completing IRP.\n"));
    			IoCompleteRequest(MyIrp, IO_NO_INCREMENT);	
				DecrementPendingActionCount( devExt );  	
				DbgPrn(("IRP clear completed.\n"));
			}

			DbgPrn(("Clearing profile.\n"));
			ClearProfile(devExt);
			DbgPrn(("Clearing any pending events.\n"));
			QueueClear(&devExt->ActionQueue);
			KeCancelTimer(&devExt->MouseTimer);
			devExt->ThreadAction = shutdown;
			KeSetEvent(&devExt->WakeupThread,0,FALSE);
			DbgPrn(("X36 clear complete.\n"));
            /*
             *  Send the REMOVE IRP down the stack asynchronously.
             *  Do not synchronize sending down the REMOVE_DEVICE
             *  IRP, because the REMOVE_DEVICE IRP must be sent
             *  down and completed all the way back up to the sender
             *  before we continue.
             */
            IoCopyCurrentIrpStackLocationToNext(irp);
            status = IoCallDriver(devExt->topDevObj, irp);
            justReturnStatus = TRUE;
            
            

            DBGOUT(("REMOVE_DEVICE - waiting for %d irps to complete...",
                    devExt->pendingActionCount));  


            /*
             *  We must for all outstanding IO to complete before
             *  completing the REMOVE_DEVICE IRP.
             *
             *  First do an extra decrement on the pendingActionCount.
             *  This will cause pendingActionCount to eventually
             *  go to -1 once all asynchronous actions on this
             *  device object are complete.
             *  Then wait on the event that gets set when the
             *  pendingActionCount actually reaches -1.
             */
            DecrementPendingActionCount(devExt);


	    DbgPrn(("PendingActionCount: %d",devExt->pendingActionCount));

            KeWaitForSingleObject(  &devExt->removeEvent,
                                    Executive,      // wait reason
                                    KernelMode,
                                    FALSE,          // not alertable
                                    NULL );         // no timeout


			

            DBGOUT(("REMOVE_DEVICE - ... DONE waiting. ")); 
			
            /*
             *  Detach our device object from the lower 
             *  device object stack.
             */
            IoDetachDevice(devExt->topDevObj);
            

	   

			// Remove the control device.
			RemoveControl(devExt);

            /*
             *  Delete our device object.
             *  This will also delete the associated device extension.
             */
            IoDeleteDevice(devExt->filterDevObj);
            
            // Update global device count variable
            NumDevices--;
	           	
            
        }
        break;


    case IRP_MN_QUERY_DEVICE_RELATIONS:
			DbgPrn(("Acquiring Spinlock.\n"));
           		KeAcquireSpinLock(&devExt->ReadLock, &OldIrql);
           		DbgPrn(("Grabbing read IRP.\n"));
           		MyIrp = devExt->ReadIrp;
           		KeReleaseSpinLock(&devExt->ReadLock, OldIrql);
			DbgPrn(("Releasing spinlock.\n"));

			if ( MyIrp != NULL )
			{
				DbgPrn(("Clearing IRP.\n"));
				CompleteRead(NULL,devExt,NULL,NULL);
				DbgPrn(("IRP clear completed.\n"));
			}
			break;


	case IRP_MN_QUERY_ID:
		DbgPrint("Query ID: %d",irpSp->Parameters.QueryId.IdType );
        IoCopyCurrentIrpStackLocationToNext (irp);
        IoSetCompletionRoutine (irp, D3C_CatchQuery, NULL, TRUE, TRUE, TRUE);
        status = IoCallDriver (devExt->topDevObj, irp);
        justReturnStatus=TRUE;
        break;

    
    
    default:
        break;


    }

    if (justReturnStatus){
        /*
         *  We've already sent this IRP down the stack.
         */
    }
    else if (completeIrpHere){
        irp->IoStatus.Status = status;
        IoCompleteRequest(irp, IO_NO_INCREMENT);
    }
    else {
        IoCopyCurrentIrpStackLocationToNext(irp);
        status = IoCallDriver(devExt->topDevObj, irp);
    }

    return status;
}



NTSTATUS GetDeviceCapabilities(struct DEVICE_EXTENSION *devExt)
/*++

Routine Description:

    Function retrieves the DEVICE_CAPABILITIES descriptor from the device

Arguments:

    devExt - device extension for targetted device object

Return Value:

    NT status code

--*/
{
    NTSTATUS status;
    PIRP irp;

    PAGED_CODE();

    irp = IoAllocateIrp(devExt->topDevObj->StackSize, FALSE);
    if (irp){
        PIO_STACK_LOCATION nextSp = IoGetNextIrpStackLocation(irp);

        // must initialize DeviceCapabilities before sending...
        RtlZeroMemory(  &devExt->deviceCapabilities, 
                        sizeof(DEVICE_CAPABILITIES));
        devExt->deviceCapabilities.Size = sizeof(DEVICE_CAPABILITIES);
        devExt->deviceCapabilities.Version = 1;
        devExt->deviceCapabilities.Address = -1;
        devExt->deviceCapabilities.UINumber = -1;

        // setup irp stack location...
        nextSp->MajorFunction = IRP_MJ_PNP;
        nextSp->MinorFunction = IRP_MN_QUERY_CAPABILITIES;
        nextSp->Parameters.DeviceCapabilities.Capabilities = 
                        &devExt->deviceCapabilities;

        /*
         *  For any IRP you create, you must set the default status
         *  to STATUS_NOT_SUPPORTED before sending it.
         */
        irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

        status = CallNextDriverSync(devExt, irp);

        IoFreeIrp(irp);
    }
    else {
        status = STATUS_INSUFFICIENT_RESOURCES;
    }

    ASSERT(NT_SUCCESS(status));
    return status;
}

