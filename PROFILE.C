// PROFILE.C
//
// Profile management for X36 driver.
//


//#define DBG 1

#include <WDM.H>
#include <filter.h>


void ClearStatus(struct DEVICE_EXTENSION* devExt)
{
	int i;
	
    RtlFillMemory(devExt->KeyboardStatus,29,0x00);  
    RtlFillMemory(devExt->DirectXStatus,sizeof(devExt->DirectXStatus),0x00);
    RtlFillMemory(devExt->MouseStatus,sizeof(devExt->MouseStatus),0x00);
    RtlFillMemory(devExt->LastPosition,0x07,0xFF);  
    //RtlFillMemory(devExt->LastAxis,sizeof(devExt->LastAxis),0xFF);      
    //RtlFillMemory(devExt->LastAxisRaw,sizeof(devExt->LastAxisRaw),0xFF);      
    RtlFillMemory(devExt->LastAxisTrans,sizeof(devExt->LastAxisTrans),0xFF);      
    RtlFillMemory(&devExt->LastPosition[7],0x4,0x00); 
        
    RtlZeroMemory(devExt->FrobStatus, MAX_FROB);    

	// fix axis chains
	for (i=0;i<NUM_AXIS;i++)
	{
		//devExt->AxisLoc[i] = devExt->Axes[i]; // Active axis location.
		
		// spool to current location
		devExt->AxisLoc[i] = devExt->Axes[i];
		
		while ((devExt->AxisLoc[i] != NULL) && (devExt->AxisLoc[i]->next != NULL) 
		 		&& (devExt->LastAxis[i] > devExt->AxisLoc[i]->next->position)) {
		 			devExt->AxisLoc[i] = 	devExt->AxisLoc[i] -> next;
		 		}
		 // Fix for sentinel value
		 if (devExt->AxisLoc[i] == devExt->Axes[i]) // still on sentinel
		 	devExt->AxisLoc[i] = NULL;		
	}
}
  


unsigned char LoadProfile(struct DEVICE_EXTENSION* DevExt, unsigned char* Buffer, unsigned long Length)
{
	PCONFIG StartButton, CurrentButton;
	PSCRIPT StartScript, CurrentScript;
	PAXIS_POINT StartPoint, CurrentPoint;
	void** CurrentPtr;
	unsigned int NumEntries, NumConfigs, NumAxis, CurrentConfig, CurrentAxis;
	unsigned int NumCommands,CurrentCommand,i,j;
	unsigned long Pos = (sizeof(unsigned char)+sizeof(unsigned long));

	DbgPrn(("X36C: Profile load beginning.  Recieved bytes: %d\n",Length));
	// BEGIN SANITY CHECKS ON BUFFER.
	// min length, first pass.
	if (Length < Pos)
		return 2;
	
	// Print header information
	DbgPrn(("X36C: Profile version: %d   Profile length: %d\n",Buffer[0],*(unsigned long*)&Buffer[1]));

	// Check profile version.
	if ((Buffer[0] < 1)||(Buffer[0] > 4))
		return 3;
	// Check length field
	if (*(unsigned long*)&Buffer[1]!= Length)
		return 4;


	if (Buffer[0] < 3)
	{
		// Check length
		if (Length < (Pos + (64*2*sizeof(unsigned short))))
			return 2;
		
		DbgPrn(("X36C: Profile loading default frobbables.\n"));
		// Load initial frobbables.
		RtlCopyMemory(DevExt->Buttons,&Buffer[sizeof(unsigned char)+sizeof(unsigned long)],(64*2*sizeof(unsigned short))); // old 64-entry frob table.
		Pos += (64*2*sizeof(unsigned short));
	}
	
	// Begin loading commands.
	
	// Check length
	if (Length < (Pos + (sizeof(unsigned short))))
		return 2;
	
	
	if ((NumCommands = *(unsigned short*)&Buffer[Pos]) > MAX_COMMAND)
	{
		return 5;
	}
	Pos += sizeof(unsigned short);
	
	// Load commands.
	DbgPrn(("X36C: Profile loading commands.  Num: %d   StartOffset: %d\n",NumCommands,Pos));
	for (i=0; i < NumCommands; i++)
	{
		// Sanity check on buffer size.
		if ((Pos+sizeof(unsigned short)+sizeof(unsigned char)) > Length)
		{
			return 6;
		}

		//Get current command.
		CurrentCommand = *(unsigned short*)&Buffer[Pos];  Pos += sizeof(unsigned short);
		CurrentPtr = getloc2(&DevExt->Commands, CurrentCommand);

		// Get number of script entries.
		if (Buffer[0] < 3)    {
			NumEntries = Buffer[Pos]; Pos += sizeof(unsigned char);
		} else {
			NumEntries = *(unsigned short*) (&Buffer[Pos]);
			Pos += sizeof(unsigned short);
		}


		DbgPrn(("X36C: Loading command: %d   Script Entries: %d\n",CurrentCommand,NumEntries));
		// Sanity check number of commands and the command entry position.
		if (CurrentCommand > MAX_COMMAND)
		{
			return 12;
		}
		if (CurrentPtr == NULL) // couldn't allocate memory.
		{
			return 20;
		}



		// Sanity check on buffer size including number of script entries.
		if ((Pos + (NumEntries * (sizeof(unsigned char)+sizeof(unsigned short)+sizeof(unsigned long)))) > Length)
		{
			return 7;
		}



		if (NumEntries > 0)
		{
			// Load script entries.
			StartScript = CurrentScript = (PSCRIPT) ExAllocatePool(NonPagedPool, NumEntries*sizeof(SCRIPT));
			if (StartScript == NULL)
			{
				return 8;
			}

			for (j=0; j<NumEntries; j++)
			{
				CurrentScript->Op = Buffer[Pos];  Pos+=sizeof(unsigned char);
				CurrentScript->Offset = *(unsigned short*)&Buffer[Pos];  Pos+=sizeof(unsigned short);
				CurrentScript->Delay = (*(unsigned long*)&Buffer[Pos])* 10000;  Pos+=sizeof(unsigned long);
				CurrentScript->Next = CurrentScript + 1;
				DbgPrn(("X36C: Loaded command %d entry %d.   Op: %d  Offset: %d  Delay: %d \n",CurrentCommand,j,CurrentScript->Op,CurrentScript->Offset,CurrentScript->Delay));
				CurrentScript=CurrentScript->Next;
			}
			CurrentScript-=1;
			CurrentScript->Next = NULL;
		}
		else
		StartScript = NULL;

		// Clear command if there's an overlap.
		if (*CurrentPtr != NULL)
		ExFreePool(*CurrentPtr);

		*CurrentPtr=StartScript;
		DbgPrn(("X36C: Command %d loaded.\n",CurrentCommand));
	}

	// If we're a new version profile, start loading configurations.
	if (Buffer[0] >= 2)
	{

		if (Buffer[0] == 2)
		{
			NumConfigs = Buffer[Pos++];
		} else {
			NumConfigs = *(unsigned short*) (&Buffer[Pos]);
			Pos += sizeof(unsigned short);
		}

		// Load commands.
		DbgPrn(("X36C: Profile loading configurations.  Num: %d   StartOffset: %d\n",NumConfigs,Pos));
		for (i=0; i < NumConfigs; i++)
		{
			// Sanity check on buffer size.
			if ((Pos+(Buffer[0]==2)?sizeof(unsigned char):sizeof(unsigned short)+sizeof(unsigned char)) > Length)
			{
				return 14;
			}
			if (CurrentPtr == NULL) // couldn't allocate memory.
			{
				return 21;
			}



			//Get current command.
			if (Buffer[0] == 2) {
				CurrentConfig = Buffer[Pos++];
			}
			else
			{
				CurrentConfig = *(unsigned short*)&Buffer[Pos];
				Pos += sizeof(unsigned short);
			}

			CurrentPtr = getloc2(&DevExt->Configurations, CurrentConfig);
			// Get number of config entries.
			if (Buffer[0] < 3)    {
				NumEntries = Buffer[Pos]; Pos += sizeof(unsigned char);
			} else {
				NumEntries = *(unsigned short*) (&Buffer[Pos]);
				Pos += sizeof(unsigned short);
			}

			DbgPrn(("X36C: Loading config: %d   Script Entries: %d\n",CurrentConfig,NumEntries));

			// Sanity check on buffer size including number of config entries.
			if ((Pos + (NumEntries * (sizeof(unsigned char)+sizeof(unsigned short)))) > Length)
			{
				return 15;
			}



			if (NumEntries > 0)
			{
				// Load config entries.
				StartButton = CurrentButton = (PCONFIG) ExAllocatePool(NonPagedPool, NumEntries*sizeof(CONFIG));
				if (StartButton == NULL) return 16;
				for (j=0; j<NumEntries; j++)
				{
					if (Buffer[0]==2) {
						CurrentButton->Type = 0;
						CurrentButton->Button = Buffer[Pos];  Pos+=sizeof(unsigned char);
					} else if (Buffer[0] == 3) {
						CurrentButton->Type = 0;
						CurrentButton->Button = *(unsigned short*) (&Buffer[Pos]);
						Pos += sizeof(unsigned short);
					}
					else
					{
						CurrentButton->Type = Buffer[Pos];  Pos+=sizeof(unsigned char);
						CurrentButton->Button = *(unsigned short*) (&Buffer[Pos]);
						Pos += sizeof(unsigned short);
					}

					CurrentButton->NewCommand = *(unsigned short*)&Buffer[Pos];  Pos+=sizeof(unsigned short);
					CurrentButton->Next = CurrentButton + 1;
					DbgPrn(("X36C: Loaded config %d entry %d.   Button: %d  NewCommand: %d\n",CurrentConfig,j,CurrentButton->Button,CurrentButton->NewCommand));
					CurrentButton=CurrentButton->Next;
				}
				CurrentButton-=1;
				CurrentButton->Next = NULL;
			}
			else
			StartButton = NULL;

			// Clear config if there's an overlap.
			if (*CurrentPtr != NULL)
			ExFreePool(*CurrentPtr);

			*CurrentPtr=StartButton;
			DbgPrn(("X36C: Config %d loaded.\n",CurrentConfig));
		}


		if (Buffer[0] == 4) // type 4 profile loads axis chains.
		{
			NumAxis = *(unsigned short*) (&Buffer[Pos]);
			Pos += sizeof(unsigned short);

			// Load commands.
			DbgPrn(("X36C: Profile loading axes.  Num: %d   StartOffset: %d\n",NumAxis,Pos));
			for (i=0; i < NumAxis; i++)
			{
				// Sanity check on buffer size.
				if ((Pos+sizeof(unsigned short)+sizeof(unsigned char)) > Length)
				{
					return 26;
				}


				//Get current chain.
				CurrentAxis = *(unsigned short*)&(Buffer[Pos]);  Pos += sizeof(unsigned short);

				CurrentPtr = getloc2(&DevExt->Chains, CurrentAxis);
				
				// Get number of chain entries.
				NumEntries = *(unsigned char*) (&Buffer[Pos]);
				Pos += sizeof(unsigned char);


				DbgPrn(("X36C: Loading chain: %d   Point Entries: %d\n",CurrentAxis,NumEntries));

				// Sanity check on buffer size including number of script entries.
				if ((Pos + (NumEntries * (sizeof(unsigned short)+sizeof(unsigned short)+sizeof(unsigned short)))) > Length)
				{
					return 28;
				}

				if (NumEntries > 0)
				{
					// Load script entries.
					StartPoint = (PAXIS_POINT) ExAllocatePool(NonPagedPool, (NumEntries+1)*sizeof(AXIS_POINT));
					if (StartPoint == NULL)
					{
						return 29;
					}
					CurrentPoint = StartPoint+1;
					StartPoint->next = CurrentPoint;
					StartPoint->previous = StartPoint;

					// drop points in load order
					for (j=0; j<NumEntries; j++)
					{
						CurrentPoint->position = *(unsigned short*)&Buffer[Pos];  Pos+=sizeof(unsigned short);
						CurrentPoint->frobbable = *(unsigned short*)&Buffer[Pos];  Pos+=sizeof(unsigned short);
						CurrentPoint->translation = *(unsigned short*)&Buffer[Pos];  Pos+=sizeof(unsigned short);
						CurrentPoint->previous = CurrentPoint -1;
						CurrentPoint->next = CurrentPoint + 1;
						DbgPrn(("X36C: Loaded point entry %d.   Pos: %d  Frob: %d  Xlat: %d \n",j,CurrentPoint->position,CurrentPoint->frobbable,CurrentPoint->translation));
						CurrentPoint=CurrentPoint->next;
					}
					CurrentPoint-=1;
					CurrentPoint->next = NULL;
					StartPoint->previous = NULL;
					j = (NumEntries > 1) ? 1 : 0;
					// sort points using a bogosort.
					while (j > 0)
					{
						CurrentPoint = StartPoint->next->next; // two entries needed.
						j = 0;
						while (CurrentPoint != NULL)
						{
							if ((CurrentPoint->previous != StartPoint) && (CurrentPoint->position < CurrentPoint->previous->position))
							{
								PAXIS_POINT Temp = CurrentPoint->previous;
								DbgPrn(("X36C: Sort swapping two entries, pos %d and %d\n",CurrentPoint->position,CurrentPoint->previous->position));
								CurrentPoint->previous = Temp->previous;
								Temp->next = CurrentPoint->next;
								CurrentPoint->next = Temp;
								Temp->previous = CurrentPoint;

								// and 2nd-level links
								if (CurrentPoint->previous != NULL) CurrentPoint->previous->next = CurrentPoint;
								if (Temp->next != NULL) Temp->next->previous = Temp;
								j++;
							}
							else
							{
								CurrentPoint = CurrentPoint->next;
							}
						}
						// refind lowest point
						//while (StartPoint->previous != NULL) StartPoint = StartPoint->previous;
					}
				}
				else
					StartPoint = NULL;
					
				// Clear command if there's an overlap.
				if (*CurrentPtr != NULL)
					ExFreePool(*CurrentPtr);
					

				*CurrentPtr = StartPoint;

				DbgPrn(("X36C: Axis %d loaded, pos %d.\n",CurrentAxis,Pos));
			}

		}
	}

	// Finally, check the footer.
	if ((Pos+sizeof(unsigned char)) != Length )
	{
		return 10;
	}


	if (Buffer[Pos] != 0x00)
	{
		return 11;
	}

	if (Buffer[0] >= 3)
	{
		// Load configuration 0.
		LoadConfiguration(DevExt,get(&DevExt->Configurations,0));
	}

	// Finally, clear the status buffers.
	ClearStatus(DevExt);



	DbgPrn(("X36C: Profile load successful.\n",CurrentCommand));
	return 0;
}



NTSTATUS ClearProfile(struct DEVICE_EXTENSION  *DeviceExtension) // Clears a profile and destroys all memory structures.
{
  unsigned int i,j,k,l,m;

  // Clear scripts and configs.
  flush2(&DeviceExtension->Commands);
  flush2(&DeviceExtension->Configurations);
  flush2(&DeviceExtension->Chains);

  // Clear the active queue.
  QueueClear(&DeviceExtension->ActionQueue);

  RtlZeroMemory(&DeviceExtension->Commands,sizeof(DeviceExtension->Commands));
  RtlZeroMemory(&DeviceExtension->Configurations, sizeof(DeviceExtension->Configurations));
  RtlZeroMemory(&DeviceExtension->Axes,sizeof(DeviceExtension->Axes));
  RtlZeroMemory(DeviceExtension->Buttons, sizeof(unsigned short)*2*MAX_FROB);

  ClearStatus(DeviceExtension);

  return STATUS_SUCCESS;
}







/* -------------------------------------- */
// Begin new profile management section.
// Functions under here deal with the program control object. 
/* -------------------------------------- */

// Defines for the control device name.
#define SYMBOLIC_NAME_STRING    L"\\DosDevices\\X36Program"
#define NTDEVICE_NAME_STRING    L"\\Device\\X36Program"



NTSTATUS InitControl(struct DEVICE_EXTENSION  *fDeviceExtension, PDRIVER_OBJECT DriverObject)
{
  NTSTATUS            status = STATUS_SUCCESS;
    UNICODE_STRING      NtDeviceName;
    UNICODE_STRING      SymbolicLinkName;
    struct CDEVICE_EXTENSION* CDevExt;
    


    // 
    // Initialize the Unicode strings.
    // 
    RtlInitUnicodeString(&NtDeviceName, NTDEVICE_NAME_STRING);
    RtlInitUnicodeString(&SymbolicLinkName, SYMBOLIC_NAME_STRING);

  
    // Create a named deviceobject so that applications or drivers
    // can directly talk to us without going through the entire stack.
    // This call could fail if there are not enough resources, or
    // another deviceobject of same name exists (name collision).
    // 
//  DbgPrint("Creating control device object.\n");    
    status = IoCreateDevice(DriverObject,
                            sizeof(struct CDEVICE_EXTENSION),
                            &NtDeviceName,
                            FILE_DEVICE_UNKNOWN,
                            0,
                            FALSE, 
                            &fDeviceExtension->ControlDevObj);

    if (NT_SUCCESS( status )) {
//    DbgPrint("Control device object created.\n");

        CDevExt = (struct CDEVICE_EXTENSION *)fDeviceExtension->ControlDevObj->DeviceExtension;
        CDevExt->signature = CDEVICE_EXTENSION_SIGNATURE;
        CDevExt->filterDevExt = fDeviceExtension;
        CDevExt->Status=0;
        CDevExt->ReturnVal=0;
        
        status = IoCreateSymbolicLink( &SymbolicLinkName, &NtDeviceName );

        if ( !NT_SUCCESS( status )) {
          DbgPrint("Error! Unable to set up symbolic link.  Destroying control device object.\n");
      IoDeleteDevice(fDeviceExtension->ControlDevObj);
      return status;
        }
        fDeviceExtension->ControlDevObj->Flags |= DO_BUFFERED_IO;
      fDeviceExtension->ControlDevObj->Flags &= ~DO_DEVICE_INITIALIZING;
    } 

  return status;
}


NTSTATUS ProfileWrite(struct CDEVICE_EXTENSION* CDevExt, struct DEVICE_EXTENSION* DevExt, unsigned long Len, PVOID DataIn)
{
    unsigned char* Buffer;
    unsigned char returnval;
    KIRQL Old;
    
    // Sanity check.
    if (Len == 0)
    {
        return STATUS_INVALID_PARAMETER;
    }
    if (CDevExt->Status & 0x80)
    {
        return STATUS_DEVICE_BUSY;
    }
    
    // Copy buffer locally so we have easy access.
    Buffer = (unsigned char*) ExAllocatePool(NonPagedPool, Len);
    RtlCopyMemory(Buffer, DataIn, Len);
    
    // Comm protocol block.
    switch (CDevExt->Status)
    {
        case 0: // New command.
            if (Len > 1)
            {
                CDevExt->Status &= 0x80;
                CDevExt->ReturnVal = 0xFF;
            }
            else
                switch (*Buffer)
                {
                    case 0: // ping.
                        DbgPrn(("X36C: OP: Ping \n"));
                        CDevExt->Status &= 0x80;
                        CDevExt->ReturnVal = 0;
                        break;
                    case 1: // Request to send new profile.
                        DbgPrn(("X36C: OP: New Profile \n"));
                        CDevExt->Status = 0x81;
                        CDevExt->ReturnVal = 0;
                        break;
                    case 2: // Request to send profile.
                        DbgPrn(("X36C: OP: Send Existing Profile \n"));
                            // NOT YET IMPLEMENTED.                        
                    case 3: // Clear existing profile.
                        DbgPrn(("X36C: OP: Clear \n"));
                        KeAcquireSpinLock(&DevExt->ScriptLock,&Old);            
                        ClearProfile(DevExt);
                        KeReleaseSpinLock(&DevExt->ScriptLock,Old);
                        CDevExt->Status = 0x80;
                        CDevExt->ReturnVal = 0;
                        break;
                    case 4: // Clear all calibration data
                        DbgPrn(("X36C: OP: ClearCalib \n"));
                        ClearCalibration(DevExt);
                        CDevExt->Status = 0x80;
                        CDevExt->ReturnVal = 0;
                        break;
                    case 5: // Interpreted calibration command
                      DbgPrn(("X36C: OP: InterpCalib \n"));                    
                        CDevExt->Status = 0x85;
                        CDevExt->ReturnVal = 0;
                        break;
                      

                    default:
                        DbgPrn(("X36C: OP: Unsupported \n"));
                        CDevExt->Status = 0x80;
                        CDevExt->ReturnVal = 0xFF;
                }
            break;

        case 1: // Incoming profile block.
            DbgPrn(("X36C: Receiving new profile \n"));
			KeAcquireSpinLock(&DevExt->ScriptLock,&Old);            
            ClearProfile(DevExt);
            returnval = LoadProfile(DevExt, Buffer, Len);
            if (returnval != 0)
            	ClearProfile(DevExt);
			KeReleaseSpinLock(&DevExt->ScriptLock,Old);                        	
            DbgPrn(("X36C: New profile return value: %d \n",returnval));
            CDevExt->Status = 0x80;
            CDevExt->ReturnVal = returnval;
            break;

        case 5: // Interpreted calibration command
            DbgPrn(("X36C: Calibration command!\n"));
            CDevExt->Status = 0x80;
            if ((Len != 3) || (Buffer[0] >= NUM_AXIS ) || (Buffer[1] >= 3))
            {
              CDevExt->ReturnVal = 0xFF;
              break;
            }
            if (Buffer[2] & 0x80)
            {
              // set dead zone
               DbgPrn(("X36C: Axis %d Deadzone %d set to %d\n",Buffer[0],Buffer[1],Buffer[2]&0x7F));                    
               DevExt->Calibration[Buffer[0]].deadzone[Buffer[1]] = Buffer[2]&0x7F;
               CDevExt->ReturnVal = 0x0;
            }
            else
            {
              switch(Buffer[2])
              {
                  case 0: // Disable floating
                    DbgPrn(("X36C: Axis %d value %d floating off.\n",Buffer[0],Buffer[1]));     
                    DevExt->Calibration[Buffer[0]].floating[Buffer[1]] = 0;
                    CDevExt->ReturnVal = 0x0;
                    break;

                  case 1: // Enable floating
                    DbgPrn(("X36C: Axis %d value %d floating on.\n",Buffer[0],Buffer[1]));     
                    DevExt->Calibration[Buffer[0]].floating[Buffer[1]] = 1;
                    CDevExt->ReturnVal = 0x0;
                    break;

                  case 2: // Load value
                    DbgPrn(("X36C: Axis %d value %d loading LastAxis.\n",Buffer[0],Buffer[1]));     
                    DevExt->Calibration[Buffer[0]].value[Buffer[1]] = (unsigned short)DevExt->LastAxisRaw[Buffer[0]];
                    
                    if (((Buffer[1] == 0) || (Buffer[1] == 2)) && (DevExt->Calibration[Buffer[0]].floating[1]))
					{
						DevExt->Calibration[Buffer[0]].value[1] = ((DevExt->Calibration[Buffer[0]].value[2] - DevExt->Calibration[Buffer[0]].value[0])/2) + DevExt->Calibration[Buffer[0]].value[0];
					}
                    
                    CDevExt->ReturnVal = 0x0;
                    break;

                  default: // your guess is as good as mine
                    CDevExt->ReturnVal = 0x0F;
              }
            }
            break;
            

        default: // Anything else
        DbgPrn(("X36C:  Unknown status\n"));
        CDevExt->Status &= 0x80;
        CDevExt->ReturnVal = 0xFF;
    }

    ExFreePool(Buffer);    
    return STATUS_SUCCESS;
}



// Handle control device IRPs.
NTSTATUS ControlHandleIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
  PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    struct CDEVICE_EXTENSION* CDevExt = DeviceObject->DeviceExtension;
    struct DEVICE_EXTENSION* DevExt = CDevExt->filterDevExt;
    
    status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    irpStack = IoGetCurrentIrpStackLocation (Irp);

    //DbgPrint("Control IRP: majorFunc=%d, minorFunc=%d\n", 
    //            (ULONG)irpStack->MajorFunction, (ULONG)irpStack->MinorFunction); 

    switch (irpStack->MajorFunction) {
        case IRP_MJ_CREATE:
            DbgPrn(("X36C: Create \n"));
            if (CDevExt->Status)
                status = STATUS_DEVICE_BUSY;
            else
                CDevExt->Status=0;
            break;
        case IRP_MJ_CLOSE:
            DbgPrn(("X36C: Close \n"));            
            break;
        case IRP_MJ_CLEANUP:
            DbgPrn(("X36C: Cleanup \n"));
            CDevExt->Status = 0;
        break;
        case IRP_MJ_READ:
            DbgPrn(("X36C: Read \n"));
            if (irpStack->Parameters.Read.Length > 0)
            {
                RtlCopyMemory( Irp->AssociatedIrp.SystemBuffer, &CDevExt->ReturnVal, 1);
                Irp->IoStatus.Information = 1;
                status = STATUS_SUCCESS;
                if (CDevExt->Status & 0x80)
                {
                    CDevExt->Status -= 0x80;
                }
            }
            break;
        case IRP_MJ_WRITE:
            status = ProfileWrite(CDevExt, DevExt, irpStack->Parameters.Write.Length, Irp->AssociatedIrp.SystemBuffer);
            Irp->IoStatus.Information = irpStack->Parameters.Write.Length;
            break;
        default:
            status = STATUS_NOT_SUPPORTED;
    }

    Irp->IoStatus.Status = status;
    IoCompleteRequest (Irp, IO_NO_INCREMENT);
    return status;
}



// Remove the control device from the system.
void RemoveControl(struct DEVICE_EXTENSION  *fDeviceExtension)
{
    UNICODE_STRING      SymbolicLinkName;

    RtlInitUnicodeString(&SymbolicLinkName, SYMBOLIC_NAME_STRING);
//  DbgPrint("Removing control device object.\n");
    IoDeleteSymbolicLink(&SymbolicLinkName);
    IoDeleteDevice(fDeviceExtension->ControlDevObj);
  //fDeviceExtension->ControlDevObj=NULL;
}
