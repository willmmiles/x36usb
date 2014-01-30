/*++

Power.C - power management IRP code
Mostly borrowed from Microsoft's NULL filter sample driver
Slightly modified for Saitek X36

--*/
//#define DBG 1

#include <WDM.H>

#include "filter.h"




NTSTATUS VA_Power(struct DEVICE_EXTENSION *devExt, PIRP irp)
/*++

Routine Description:

    Dispatch routine for Power IRPs (MajorFunction == IRP_MJ_Power)


    Note:
        
            This function is left locked down.        

Arguments:

    devExt - device extension for targetted device object
    irp - Io Request Packet

Return Value:

    NT status code

--*/
{
    PIO_STACK_LOCATION irpSp;
    NTSTATUS status;
    KIRQL Old;
    

    irpSp = IoGetCurrentIrpStackLocation(irp);

    DBGOUT(("VA_Power, minorFunc = %d ", (ULONG)irpSp->MinorFunction)); 

    switch (irpSp->MinorFunction){

        case IRP_MN_SET_POWER:
        

            switch (irpSp->Parameters.Power.Type) {

                case SystemPowerState:
                    /*
                     *  For system power states, just pass the IRP down.
                     */
                    /* X36 exception:  If we're shutting down, abort and return the keyboard IRP. */
                    /*
                    if (irpSp->Parameters.Power.State.SystemState!=PowerSystemWorking)
                    {
                    	KeAcquireSpinLock(&devExt->ReadLock, &Old);
						if ( devExt->ReadIrp != NULL )
						{		
							KeCancelTimer(&devExt->Timer);
							KeReleaseSpinLock(&devExt->ReadLock, Old);
							CompleteRead( NULL, devExt, NULL, NULL);				
					
						}
						else
							KeReleaseSpinLock(&devExt->ReadLock, Old);
					}
			KeCancelTimer(&devExt->MouseTimer);					
			KeSetEvent(&devExt->ShuttingDown,0,FALSE);
                    	*/
                    break;

                case DevicePowerState:

                    switch (irpSp->Parameters.Power.State.DeviceState) {

                        case PowerDeviceD0:
                            /*
                             *  Resume from APM Suspend
                             *
                             *  Do nothing here; 
                             *  Send down the read IRPs in the completion
                             *  routine for this (the power) IRP.
                             */
                            break;

                        case PowerDeviceD1:
                        case PowerDeviceD2:
                        case PowerDeviceD3:
                            /*
                             *  Suspend
                             */
                            DbgPrn(("X36F: Suspending."));
                            if (devExt->state == STATE_STARTED){
                                devExt->state = STATE_SUSPENDED;
                            }
                            
	                      	KeAcquireSpinLock(&devExt->ReadLock, &Old);
							if ( devExt->ReadIrp != NULL )
							{		
								KeCancelTimer(&devExt->Timer);
								KeReleaseSpinLock(&devExt->ReadLock, Old);
								CompleteRead( NULL, devExt, NULL, NULL);				
							}
							else
								KeReleaseSpinLock(&devExt->ReadLock,Old);
								
							KeCancelTimer(&devExt->MouseTimer);								
							devExt->ThreadAction = standby;
							KeSetEvent(&devExt->WakeupThread,0,FALSE);

                            break;

                    }
                    break;

            }
            break;

    }


    /*
     *  Send the IRP down the driver stack,
     *  using PoCallDriver (not IoCallDriver, as for non-power irps).
     */
    IncrementPendingActionCount(devExt);
    IoCopyCurrentIrpStackLocationToNext(irp);
    IoSetCompletionRoutine( irp, 
                            VA_PowerComplete, 
                            (PVOID)devExt,  // context
                            TRUE, 
                            TRUE, 
                            TRUE);
    status = PoCallDriver(devExt->topDevObj, irp);

    return status;
}


NTSTATUS VA_PowerComplete(
                            IN PDEVICE_OBJECT devObj, 
                            IN PIRP irp, 
                            IN PVOID context)
/*++

Routine Description:

      Completion routine for Power IRPs (MajorFunction == IRP_MJ_Power)

Arguments:

    devObj - targetted device object
    irp - Io Request Packet
    context - context value passed to IoSetCompletionRoutine by VA_Power

Return Value:

    NT status code

--*/
{
    PIO_STACK_LOCATION irpSp;
    struct DEVICE_EXTENSION *devExt = (struct DEVICE_EXTENSION *)context;
    LARGE_INTEGER li_mouse_tick;

    ASSERT(devExt);
    ASSERT(devExt->signature == DEVICE_EXTENSION_SIGNATURE); 

    /*
     *  If the lower driver returned PENDING, mark our stack location as
     *  pending also.
     */
    if (irp->PendingReturned){
        IoMarkIrpPending(irp);
    }

    irpSp = IoGetCurrentIrpStackLocation(irp);
    ASSERT(irpSp->MajorFunction == IRP_MJ_POWER);

    if (NT_SUCCESS(irp->IoStatus.Status)){
        switch (irpSp->MinorFunction){

            case IRP_MN_SET_POWER:

                switch (irpSp->Parameters.Power.Type){

                    case DevicePowerState:
                        switch (irpSp->Parameters.Power.State.DeviceState){
                            case PowerDeviceD0:
                                if (devExt->state == STATE_SUSPENDED){
                                    devExt->state = STATE_STARTED;
                                }
								DbgPrn(("X36F: Waking up."));
								// wake things up.
								li_mouse_tick.QuadPart = -1* MOUSE_TICK;    
							    KeSetTimer( &devExt->MouseTimer, li_mouse_tick, &devExt->MouseDPC );
								devExt->ThreadAction = save;
								KeSetEvent(&devExt->WakeupThread,0,FALSE);
                                break;                                
                             
                        }
                        break;

                }
                break;
        }

    }
    
    
    /*
     *  Whether we are completing or relaying this power IRP,
     *  we must call PoStartNextPowerIrp.
     */
    PoStartNextPowerIrp(irp);

    /*
     *  Decrement the pendingActionCount, which we incremented in VA_Power.
     */
    DecrementPendingActionCount(devExt);

    return STATUS_SUCCESS;
}




