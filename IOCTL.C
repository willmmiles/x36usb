/*++

Module Name:

    ioctl.c


X36 IO handler.

Overview:

  This module is responsible for handling all the device control calls that
pass through this filter.  It performs the following functions:
  -> Tweaking the USB descriptors as they come up through the stack to add
     the parameters necessary to create a virtual keyboard.
  -> Capturing an I/O request to reply with keyboard data on my own timers
  -> Replying with keyboard data
  -> Filter X36 joystick data

--*/

//#define DBG 1

#include <WDM.H>
#include "usbioctl.h"
#include "usbdi.h"
#include "usbdlib.h"
#include "filter.h"
#include "fakehid.h"




#ifdef ALLOC_PRAGMA
        #pragma alloc_text(PAGE, VA_Ioctl)
#endif


NTSTATUS VA_Ioctl(struct DEVICE_EXTENSION *  devExt, PIRP Irp)
{
    NTSTATUS            ntStatus = STATUS_SUCCESS;    
    PIO_STACK_LOCATION  IrpStack = IoGetCurrentIrpStackLocation(Irp);    
    ULONG dwControlCode = IrpStack->Parameters.DeviceIoControl.IoControlCode;
  PURB pUrb = (PURB)IrpStack->Parameters.Others.Argument1;
  KIRQL Old;

          
    if (IOCTL_INTERNAL_USB_SUBMIT_URB == dwControlCode)
  {

    switch (pUrb->UrbHeader.Function)
    {
        case URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:
        KeAcquireSpinLock(&devExt->ReadLock,&Old);
        if ((devExt->ReadIrp == NULL)&&(KeGetCurrentIrql()==DISPATCH_LEVEL)) // Don't capture if we're not really running.
		{
			DbgPrn(("Capturing keyboard read IRP.\n"));          
			devExt->ReadIrp = Irp;
			KeReleaseSpinLock(&devExt->ReadLock,Old);
			
			IncrementPendingActionCount(devExt);
			
			IoMarkIrpPending(Irp);
			ntStatus = STATUS_PENDING;
			
			if (devExt->NewData)
			{
				DbgPrn(("Filling request right away.  New data: %d\n",devExt->NewData));
				KeInsertQueueDpc(&devExt->ReturnDPC,NULL,NULL);
			}
		}
		else
		{
			KeReleaseSpinLock(&devExt->ReadLock,Old);
			IoCopyCurrentIrpStackLocationToNext(Irp);
			IoSetCompletionRoutine(Irp, &ReportCompletion, NULL, TRUE, TRUE, TRUE);    
			ntStatus = IoCallDriver(devExt->topDevObj, Irp);          
		}
        break;
    
        case URB_FUNCTION_RESET_PIPE:
          DbgPrn(("Reset pipe.\n"));
        if ( devExt->ReadIrp != NULL )
        {
          KeCancelTimer(&devExt->Timer);
          CompleteRead( NULL, devExt, NULL, NULL);        
        }
        break ;
        
      case URB_FUNCTION_ABORT_PIPE:
        DbgPrn(("Abort pipe.\n"));
        KeCancelTimer(&devExt->Timer);
        KeAcquireSpinLock(&devExt->ReadLock,&Old);
        if (devExt->ReadIrp != NULL)        
        {
            devExt->ReadIrp->IoStatus.Status = STATUS_CANCELLED;
            IoCompleteRequest(devExt->ReadIrp, IO_NO_INCREMENT);  
          devExt->ReadIrp = NULL;
          DecrementPendingActionCount( devExt );
        }
        KeReleaseSpinLock(&devExt->ReadLock,Old);       
        break;
    
        case URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE:
        IoCopyCurrentIrpStackLocationToNext(Irp) ;
        IoSetCompletionRoutine(Irp,(PIO_COMPLETION_ROUTINE)ConfigDescriptorCompletion, NULL, TRUE, TRUE,TRUE) ;
        ntStatus = IoCallDriver(devExt->topDevObj, Irp) ;
        break;
    
         case URB_FUNCTION_GET_DESCRIPTOR_FROM_INTERFACE:
        IoCopyCurrentIrpStackLocationToNext(Irp) ;
        IoSetCompletionRoutine(Irp,(PIO_COMPLETION_ROUTINE)ReportDescriptorCompletion, NULL, TRUE,TRUE, TRUE) ;
        ntStatus = IoCallDriver(devExt->topDevObj, Irp) ;
        break;
        
      default:
        IoSkipCurrentIrpStackLocation(Irp);
        ntStatus = IoCallDriver(devExt->topDevObj, Irp);
      }
      
  }
  else
  {
    IoSkipCurrentIrpStackLocation(Irp);
    ntStatus = IoCallDriver(devExt->topDevObj, Irp);
  }

    return ntStatus;    
}


NTSTATUS ConfigDescriptorCompletion( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context )
{
  /* Should print the data in the system. */
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    struct DEVICE_EXTENSION *  DeviceExtension = DeviceObject->DeviceExtension;
    PURB pUrb = (PURB)IoGetCurrentIrpStackLocation(Irp)->Parameters.Others.Argument1;
    struct _URB_CONTROL_TRANSFER   *pControlTransfer = (struct _URB_CONTROL_TRANSFER *) pUrb;
    PUCHAR BufPtr=NULL;
    PUCHAR Temp;
    unsigned long b,i,pos, tmp;
    
    
    if (Irp->PendingReturned) {
        IoMarkIrpPending( Irp );
    }
    

  // OK,we need to (a) snapture the report id; then (b) snapture the interrupts themselves.
  // To get the report ID we need to know which one to watch for.
  // So, first we have to capture the HID descriptor and parse that.  It comes between the configuration and interface descriptors.


  if (pUrb != NULL)
  {
      // Figure out what packet this was.
      
    DbgPrn(("  SetupPacket          :"));
    for(b=0; b<sizeof(pControlTransfer->SetupPacket); b++)
      DbgPrn((" %02x", pControlTransfer->SetupPacket[b]));
    DbgPrn(("\n"));

    BufPtr=NULL;
    if ((PUCHAR)pControlTransfer->TransferBuffer)
    {
      BufPtr=(PUCHAR)pControlTransfer->TransferBuffer;
    }
    else if(pControlTransfer->TransferBufferMDL)
    {
      BufPtr = (PUCHAR)MmGetSystemAddressForMdlSafe(pControlTransfer->TransferBufferMDL,HighPagePriority);
    }
    if (BufPtr==NULL)
    { // Confused.  Abort.
      DbgPrn(("Can't find buffer, aborting.\n"));
      return STATUS_SUCCESS;
    }
                
    if ((pControlTransfer->SetupPacket[3]==0x02)&&((pControlTransfer->SetupPacket[6]>0x09)||(pControlTransfer->SetupPacket[7]>0)))
    {
      DbgPrn(("Complete descriptor transferred.  Locating HID descriptor.\n"));
      
      for(pos=0;((BufPtr[pos+1]!=0x21)&&(pos<pControlTransfer->TransferBufferLength));pos=pos+BufPtr[pos]);
      
      if (pos>=pControlTransfer->TransferBufferLength)
      {
        DbgPrn(("Can't find HID descriptor, aborting.\n"));
        return STATUS_SUCCESS;
      }
      else
      {
        DbgPrn(("HID descriptor located at position %x.\n",pos));
      }
        
      DeviceExtension->ReportDescType = BufPtr[pos+6];
      DeviceExtension->ReportBufSize = BufPtr[pos+7];
      BufPtr[pos+7]=sizeof(MyReportDescriptor);
                    
                    
      
      if (Temp = (PUCHAR)MmGetSystemAddressForMdlSafe(pControlTransfer->TransferBufferMDL,HighPagePriority))
        RtlCopyMemory(Temp,BufPtr,pControlTransfer->TransferBufferLength);
    }   
  }
  return STATUS_SUCCESS;
}



NTSTATUS ReportDescriptorCompletion( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context )
{
  /* Should print the data in the system. */
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    struct DEVICE_EXTENSION *  DeviceExtension = DeviceObject->DeviceExtension;
    PURB pUrb = (PURB)IoGetCurrentIrpStackLocation(Irp)->Parameters.Others.Argument1;
    struct _URB_CONTROL_TRANSFER   *pControlTransfer = (struct _URB_CONTROL_TRANSFER *) pUrb;
    PUCHAR BufPtr=NULL;
    PUCHAR Temp;
    unsigned long b,i,pos, tmp;
    LARGE_INTEGER li_mouse_tick;
    
    
    if (Irp->PendingReturned) {
        IoMarkIrpPending( Irp );
    }
    

  // OK,we need to (a) snapture the report id; then (b) snapture the interrupts themselves.
  // To get the report ID we need to know which one to watch for.
  // So, first we have to capture the HID descriptor and parse that.  It comes between the configuration and interface descriptors.


  if (pUrb != NULL)
  {
      // Figure out what packet this was.
      
    DbgPrn(("  SetupPacket          :"));
    for(b=0; b<sizeof(pControlTransfer->SetupPacket); b++)
      DbgPrn((" %02x", pControlTransfer->SetupPacket[b]));
    DbgPrn(("\n"));

    BufPtr=NULL;
    if ((PUCHAR)pControlTransfer->TransferBuffer)
    {
      BufPtr=(PUCHAR)pControlTransfer->TransferBuffer;
    }
    else if(pControlTransfer->TransferBufferMDL)
    {
      BufPtr = (PUCHAR)MmGetSystemAddressForMdlSafe(pControlTransfer->TransferBufferMDL,HighPagePriority);
    }
    if (BufPtr==NULL)
    { // Confused.  Abort.
      DbgPrn(("Can't find buffer, aborting.\n"));
      return STATUS_SUCCESS;
    }
                


    if (pControlTransfer->SetupPacket[3]==DeviceExtension->ReportDescType)
    {
      DbgPrn(("Report Descriptor.\n"));
        
      // copy over modified report descriptor
      RtlCopyMemory(BufPtr,MyReportDescriptor, sizeof(MyReportDescriptor));
      pControlTransfer->TransferBufferLength=sizeof(MyReportDescriptor);
      
      // finally, turn the mouse ticker on
      li_mouse_tick.QuadPart = -1* MOUSE_TICK;
      KeSetTimer( &DeviceExtension->MouseTimer, li_mouse_tick, &DeviceExtension->MouseDPC );
    }
  }
  return STATUS_SUCCESS;
}



NTSTATUS ReportCompletion( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context )
{
  /* Should print the data in the system. */
    NTSTATUS            ntStatus = STATUS_SUCCESS;
    struct DEVICE_EXTENSION *  DeviceExtension = DeviceObject->DeviceExtension;
    PURB pUrb = (PURB)IoGetCurrentIrpStackLocation(Irp)->Parameters.Others.Argument1;
  struct _URB_BULK_OR_INTERRUPT_TRANSFER *pBulkOrInterruptTransfer = (struct _URB_BULK_OR_INTERRUPT_TRANSFER *) pUrb;
    PUCHAR BufPtr=NULL;
    PUCHAR Temp;
    LARGE_INTEGER LocalTime;
    QENTRY Current;
    unsigned char Delta[0x0a];
    unsigned char switch1,switch2,shift;
    unsigned short i;
    unsigned long b,pos, tmp;
    unsigned short axisval,outaxisval;
    KIRQL Old;

    
    if (Irp->PendingReturned) {
        IoMarkIrpPending( Irp );
    }

    
    // Lag avoidance routine.
  // KeQuerySystemTime(&LocalTime);
  

  // OK,we need to (a) snapture the report id; then (b) snapture the interrupts themselves.
  // To get the report ID we need to know which one to watch for.
  // So, first we have to capture the HID descriptor and parse that.  It comes between the configuration and interface descriptors.
  // DbgPrn(("Joystick sending data packet.\n"));

  if ((pUrb != NULL)&&(pBulkOrInterruptTransfer->TransferBufferLength == 0x0b))
  {
    // This is the stick reporting its packet.
    // DbgPrn(("Stick reporting packet.\n"));
    
    BufPtr = NULL;
  
    if ((PUCHAR)pBulkOrInterruptTransfer->TransferBuffer)
    {
      BufPtr=(PUCHAR)pBulkOrInterruptTransfer->TransferBuffer;
    }
    else if(pBulkOrInterruptTransfer->TransferBufferMDL)
    {
      BufPtr = (PUCHAR)MmGetSystemAddressForMdlSafe(pBulkOrInterruptTransfer->TransferBufferMDL,HighPagePriority);
    }
    if (BufPtr==NULL)
    { // Confused.  Abort.
      return STATUS_SUCCESS;
    }
    
    
    // Compute data structures.
    // Store system time.
    KeQuerySystemTime(&LocalTime);             
    
    // Store raw values, just in case we need them
	KeAcquireSpinLock(&DeviceExtension->ScriptLock,&Old);
    
    // Process axes.
    // x and y are special cases; do them first
    i=0;
    axisval = (unsigned short)BufPtr[0] + (((unsigned short)BufPtr[1]&0x0F) << 8);
    DeviceExtension->LastAxisRaw[i]=axisval;
    outaxisval = Calculate(&(DeviceExtension->Calibration[i]),axisval);
    DeviceExtension->LastAxis[i]=outaxisval;
    outaxisval = AxisQueue(DeviceExtension,DeviceExtension->Axes[i],&(DeviceExtension->AxisLoc[i]),outaxisval,LocalTime);    
   	DeviceExtension->LastAxisTrans[i]= (unsigned long) outaxisval;//(outaxisval - (unsigned short)0x8000); // convert to signed.
    //DbgPrint("X axis: %d %d %d %d %d %d\n",axisval,DeviceExtension->LastAxis[i],outaxisval,DeviceExtension->Calibration[i].value[0],DeviceExtension->Calibration[i].value[1],DeviceExtension->Calibration[i].value[2]);

    //y
    i++;
    axisval = (((unsigned short)BufPtr[1]&0xF0) >> 4) + ((unsigned short)BufPtr[2] << 4);
    DeviceExtension->LastAxisRaw[i]=axisval;
    outaxisval = Calculate(&(DeviceExtension->Calibration[i]),axisval);
    DeviceExtension->LastAxis[i]=outaxisval;
	outaxisval = AxisQueue(DeviceExtension,DeviceExtension->Axes[i],&(DeviceExtension->AxisLoc[i]),outaxisval,LocalTime);    
	DeviceExtension->LastAxisTrans[i]= (unsigned long) outaxisval;//(outaxisval - (unsigned short)0x8000); // convert to signed.
	//DbgPrint("Y axis: %d %d %d %d %d %d\n",axisval,DeviceExtension->LastAxis[i],outaxisval,DeviceExtension->Calibration[i].value[0],DeviceExtension->Calibration[i].value[1],DeviceExtension->Calibration[i].value[2]);



    // everything else
    for (i++;i<NUM_AXIS; i++)
    {
      axisval = (unsigned short) BufPtr[i+1];
      DeviceExtension->LastAxisRaw[i]=axisval;
      outaxisval = Calculate(&(DeviceExtension->Calibration[i]),axisval);
      DeviceExtension->LastAxis[i]=outaxisval;
      outaxisval = AxisQueue(DeviceExtension,DeviceExtension->Axes[i],&(DeviceExtension->AxisLoc[i]),outaxisval,LocalTime);    
	  DeviceExtension->LastAxisTrans[i]= (unsigned long) outaxisval;//(outaxisval - (unsigned short)0x8000); // convert to signed.

      //DbgPrint("Axis %d: %d %d %d %d %d %d\n",i,axisval,DeviceExtension->LastAxis[i],outaxisval,DeviceExtension->Calibration[i].value[0],DeviceExtension->Calibration[i].value[1],DeviceExtension->Calibration[i].value[2]);     
    }
      
      
//    DbgPrn(("Axis processing complete.\n"));                        
    

        // Right-shift hat switches 2 bits, for ease of computation.
        b=*(unsigned long*)&BufPtr[7];
        b=(b&0xF0003FFF) | ((b&0x03FFC000)<<2);
        
    // Convert hat switches to 8-way.  Compute hat switch deltas.
    for(i=0;i<3;i++)
    {
        shift = (unsigned char) ((b >> ((i*4)+16))&0x0F);
        shift = hat4to8(shift);
        pos=(0xFFFFFFFF ^ (0x0F << ((i*4)+16)));
        b = (b&pos)+ (shift <<((i*4)+16));
      }
    *(unsigned long*)&BufPtr[7] = b;


    // Compute deltas.
    for(i=0;i<0x09;i++)
    {
      Delta[i]=DeviceExtension->LastPosition[i] ^ BufPtr[i];
      DeviceExtension->LastPosition[i] = BufPtr[i];
    }
    for(;i<0x0b;i++)
    {
      Delta[i]=DeviceExtension->LastPosition[i];
      DeviceExtension->LastPosition[i] = BufPtr[i];
    }
               
        
    // Data structures computed.  Begin engine.
/*
    pos=*(unsigned long*)&BufPtr[7];
    DbgPrint("Buttons: %X ",pos);
    pos=*(unsigned long*)&Delta[7];
    DbgPrint("Delta: %X\n",pos);
*/    

    // Script queueing engine.
    // Buttons
    for(i=0;i<8;i++)
    {
      if ((Delta[7+(i/8)]>>(i%8))&0x01) // Button toggled
        Queue(DeviceExtension,i,(unsigned char)((BufPtr[7+(i/8)]>>(i%8))&0x01),LocalTime);                
    }
    
    // 3-ways - special handling to enforce ordered operation
    // release must be handled before press, so queue presses before releases - queue is lifo
    for(i=8;i<14;i++)
    {
      if (((Delta[7+(i/8)]>>(i%8))&0x01)&&(((BufPtr[7+(i/8)]>>(i%8))&0x01)==1)) // Button toggled
        Queue(DeviceExtension,i,(unsigned char)((BufPtr[7+(i/8)]>>(i%8))&0x01),LocalTime);                
    }
    for(i=8;i<14;i++)
    {
      if (((Delta[7+(i/8)]>>(i%8))&0x01)&&(((BufPtr[7+(i/8)]>>(i%8))&0x01)==0)) // Button toggled
        Queue(DeviceExtension,i,(unsigned char)((BufPtr[7+(i/8)]>>(i%8))&0x01),LocalTime);                
    }

    


    //DbgPrn(("Begin hat switch processing.\n"));
    
    b=*(unsigned long*)&BufPtr[7];
    tmp=*(unsigned long*)&Delta[7];
    for(i=0;i<4;i++) // Hat switches.
    {
      unsigned char value, prev;
                        
      // Grab hat values
      value = (unsigned char) ((b >> ((i*4)+16))&0x0F);
      prev = (unsigned char) ((tmp >> ((i*4)+16))&0x0F);
      //DbgPrint("Hat %d:  b %x   prev %x.\n",i,value,prev);
      
      if (prev != value )
      {
        // Execute release before press, so queue press first.
        Queue(DeviceExtension,(unsigned short)(14+(9*i)+value),1,LocalTime);
        Queue(DeviceExtension,(unsigned short)(14+(9*i)+prev),0,LocalTime);
        
      }

    }         


    // Construct actual outgoing packet
    BufPtr[0]=0x01;
    pBulkOrInterruptTransfer->TransferBufferLength=1;
    RtlCopyMemory(&BufPtr[pBulkOrInterruptTransfer->TransferBufferLength],DeviceExtension->LastAxisTrans,sizeof(DeviceExtension->LastAxisTrans));
    pBulkOrInterruptTransfer->TransferBufferLength+=sizeof(DeviceExtension->LastAxisTrans);
    RtlCopyMemory(&BufPtr[pBulkOrInterruptTransfer->TransferBufferLength],DeviceExtension->DirectXStatus,sizeof(DeviceExtension->DirectXStatus));
    pBulkOrInterruptTransfer->TransferBufferLength+=sizeof(DeviceExtension->DirectXStatus);
    RtlCopyMemory(&BufPtr[pBulkOrInterruptTransfer->TransferBufferLength],&DeviceExtension->HatStatus,sizeof(DeviceExtension->HatStatus));
    pBulkOrInterruptTransfer->TransferBufferLength+=sizeof(DeviceExtension->HatStatus);
    DeviceExtension->NewData&=0xFE;
    
    
    // old joystick packet patch
    //RtlMoveMemory(&BufPtr[1],BufPtr,0x0b/*pBulkOrInterruptTransfer->TransferBufferLength*/);
    //BufPtr[0]=0x01;   
    //pBulkOrInterruptTransfer->TransferBufferLength/*+=1*/ = 0x0c;

    // Patch on internal DirectX buffer.
    //RtlCopyMemory(&BufPtr[8],&DeviceExtension->KeyboardStatus[21],4);
    
    
    //
    // Respawn timer if we have any events.
    //
    QueuePeek(&DeviceExtension->ActionQueue, &Current);
    if (Current.Head)
    {
          KeSetTimer( &DeviceExtension->Timer, Current.Time, &DeviceExtension->TimerDPC );              
    }

	KeReleaseSpinLock(&DeviceExtension->ScriptLock,Old);      
  }
  //DbgPrn(("JoyPendingActionCount: %d\n",DeviceExtension->pendingActionCount));
  return STATUS_SUCCESS;
}


void CompleteRead( IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
  struct DEVICE_EXTENSION * DeviceExtension = (struct DEVICE_EXTENSION *)DeferredContext;
  PIRP Irp = NULL;
    PIO_STACK_LOCATION IrpStack;
    ULONG bytesRead;
    UCHAR buffer[30];
    unsigned char switch1,switch2,shift,b;
    unsigned long i,pos;
    KIRQL OldIrqL;
    LARGE_INTEGER li_large_mouse_tick;

  if (DeviceExtension)
  {
    
    
    if (Dpc == &(DeviceExtension->MouseDPC))
    {
      li_large_mouse_tick.QuadPart = MOUSE_TICK * -10000;
      KeSetTimer( &DeviceExtension->MouseTimer, li_large_mouse_tick, &DeviceExtension->MouseDPC );        
      if (!(*(short*)&DeviceExtension->MouseStatus[1] || *(short*)&DeviceExtension->MouseStatus[3]))
      {
//      	DbgPrn(("Not filling empty mouse irp.\n"));
      	return; // we don't need to fill this one
      }
    }
    
    
        
    KeAcquireSpinLock(&DeviceExtension->ReadLock,&OldIrqL);
    Irp = DeviceExtension->ReadIrp;
    DeviceExtension->ReadIrp = NULL;
    KeReleaseSpinLock(&DeviceExtension->ReadLock,OldIrqL);
    

  if (Irp)
  {
    IrpStack = IoGetCurrentIrpStackLocation(Irp);

    if (Dpc == &(DeviceExtension->MouseDPC))// && )
    {
      	// Mouse movement packet
      	DbgPrn(("Completing mouse movement read..\n"));
      	DbgPrn(("Mouse data:  Buttons: %X  X: %d  Y:  %d\n",DeviceExtension->MouseStatus[0],*(short*)&DeviceExtension->MouseStatus[1],*(short*)&DeviceExtension->MouseStatus[3]));
      	buffer[0]=0x03;
      	RtlCopyMemory(&buffer[1],DeviceExtension->MouseStatus,sizeof(DeviceExtension->MouseStatus));
      	bytesRead=sizeof(DeviceExtension->MouseStatus)+1;
      	DeviceExtension->NewData&= ~(0x04);
    }
    else if (DeviceExtension->NewData & 0x04) 
    {
      DbgPrn(("Completing mouse read..\n"));
      DbgPrn(("Mouse data:  Buttons: %X  X: %d  Y:  %d\n",DeviceExtension->MouseStatus[0],*(short*)&DeviceExtension->MouseStatus[1],*(short*)&DeviceExtension->MouseStatus[3]));
      buffer[0]=0x03;
      RtlCopyMemory(&buffer[1],DeviceExtension->MouseStatus,1);
      RtlFillMemory(&buffer[2],sizeof(DeviceExtension->MouseStatus)-1,0x00);
      bytesRead=sizeof(DeviceExtension->MouseStatus)+1;
//      if ((*(short*)&DeviceExtension->MouseStatus[1] == 0) && (*(short*)&DeviceExtension->MouseStatus[3]==0))
      DeviceExtension->NewData&= ~(0x04);
    }   
    else if (DeviceExtension->NewData & 0x02)
    { 
      DbgPrn(("Completing keyboard read..\n"));
        
      RtlCopyMemory(&buffer[1],DeviceExtension->KeyboardStatus,29);
      buffer[0]=0x02;
    
      //RtlFillMemory(&buffer[20],4,0x00);  
  
      // Complete our IO report.
          bytesRead = 30;     
          DeviceExtension->NewData&=0xFD;
    }
    else
    {     
      DbgPrn(("Completing joystick read..\n"));


	    buffer[0]=0x01;
	    bytesRead=1;
	    RtlCopyMemory(&buffer[bytesRead],DeviceExtension->LastAxisTrans,sizeof(DeviceExtension->LastAxisTrans));
	    bytesRead+=sizeof(DeviceExtension->LastAxisTrans);
	    RtlCopyMemory(&buffer[bytesRead],DeviceExtension->DirectXStatus,sizeof(DeviceExtension->DirectXStatus));
	    bytesRead+=sizeof(DeviceExtension->DirectXStatus);
	    RtlCopyMemory(&buffer[bytesRead],&DeviceExtension->HatStatus,sizeof(DeviceExtension->HatStatus));
	    bytesRead+=sizeof(DeviceExtension->HatStatus);
	    DeviceExtension->NewData&=0xFE;
    }
          
    if (bytesRead > IrpStack->Parameters.DeviceIoControl.OutputBufferLength)
      {
          bytesRead = IrpStack->Parameters.DeviceIoControl.OutputBufferLength;
      }

      RtlCopyMemory((PUCHAR) Irp->UserBuffer, buffer, bytesRead);
      Irp->IoStatus.Information = bytesRead;
      Irp->IoStatus.Status = STATUS_SUCCESS;
    
      IoCompleteRequest(Irp, IO_NO_INCREMENT);
      DecrementPendingActionCount(DeviceExtension);
    }
  }
}

// Stub function for Iotimer event to send periodic keyboard reports regardless of stick status.
void KeyboardTimer(IN PDEVICE_OBJECT DeviceObject, IN PVOID Context)
{
    CompleteRead( NULL, Context, NULL, NULL); 
}



/* Utilitiy functions to convert hat direction.
   Compiled inline to save performance. */
unsigned char hat4to8(unsigned char four)
{
  switch(four)
  {
    case 0: return 0;
    case 1: return 1;
    case 2: return 3;
    case 3: return 2;
    case 4: return 5;
    case 6: return 4;
    case 8: return 7;
    case 9: return 8;
    case 12: return 6;
  }
  return 0;
}


unsigned char hat8to4(unsigned char eight)
{
  switch(eight)
  {
    case 0: return 0;
    case 1: return 1;
    case 2: return 3;
    case 3: return 2;
    case 4: return 6;
    case 5: return 4;
    case 6: return 12;
    case 7: return 8;
    case 8: return 9;
  }
  return 0;
}
