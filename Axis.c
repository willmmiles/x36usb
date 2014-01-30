// AXIS.C
//
// Axis translation for X36/45 driver
//


//#define DBG 1

#include <WDM.H>
#include <filter.h>

void ClearAxis(PAXIS_CAL Axis)
{
  unsigned char i;

  for (i=0; i < 3; i++)
  {
    Axis->floating[i] = 1;
    Axis->value[i] = 0xFFFF;
    Axis->deadzone[i] = 0;
  }
}



unsigned short Calculate(PAXIS_CAL Axis, unsigned short raw)
{
	char changed = 0;
    unsigned char calcCenter = 0;
    unsigned long output;


    // check all points if necessary to update.
    // min
    if ((Axis->floating[0]) && ((raw < Axis->value[0]) || (Axis->value[0] == 0xFFFF)))
    {
      Axis->value[0] = raw;
      calcCenter = 1;
      changed = 1;
    }
    // max
    if ((Axis->floating[2]) && ((raw > Axis->value[2]) || (Axis->value[2] == 0xFFFF)))
    {
      Axis->value[2] = raw;
      calcCenter = 1;
      changed = 1;
    }
    // center
    if ((Axis->floating[1]) && ((calcCenter) || (Axis->value[1] == 0xFFFF)))
    {
      Axis->value[1] = ((Axis->value[2] - Axis->value[0])/2) + Axis->value[0];
    }
    
/*    if (changed) {
    	devExt->ThreadAction = save;
		KeSetEvent(&devExt->WakeupThread,0,FALSE);
	}*/

    // sanity test
    if ((Axis->value[0] == Axis->value[1]) || (Axis->value[1] == Axis->value[2]))
      return 0;

    // compute max value
    calcCenter = (raw >= Axis->value[1]); // above center
    
    //DbgPrint("Abv: %d   r: %d  c1: %d c2: %d",calcCenter,raw,(Axis->value[calcCenter] + Axis->deadzone[calcCenter]),(Axis->value[calcCenter+1]-Axis->deadzone[calcCenter+1]));
    // do scaling and deadzone test
    if (raw <= (Axis->value[calcCenter] + Axis->deadzone[calcCenter]))
      output = 0;
    else if (raw >= (Axis->value[calcCenter+1] - Axis->deadzone[calcCenter+1]))
      output = 0x7FFF;
    else
      output = ((((unsigned long)raw - (unsigned long)(Axis->value[calcCenter] + Axis->deadzone[calcCenter])) * 0x7FFF) / ((unsigned long)(Axis->value[calcCenter+1]-Axis->deadzone[calcCenter+1]) - (unsigned long)(Axis->value[calcCenter]+Axis->deadzone[calcCenter])));

	output +=  (calcCenter * 0x8000);
    return (unsigned short) output;
}



void ClearCalibration(struct DEVICE_EXTENSION  *DeviceExtension)
{
  unsigned char i;

  DbgPrn(("X36C: Clearing calibration data.\n"));
  for (i=0; i<NUM_AXIS; i++)
    ClearAxis(&(DeviceExtension->Calibration[i]));
}



void ReadCalibration(struct DEVICE_EXTENSION  *DeviceExtension)
{
	// reads calibration data from the registry.
    KIRQL Irql = KeGetCurrentIrql();
    
    if (Irql == PASSIVE_LEVEL)
    {		
	    NTSTATUS status;
	    HANDLE hRegDevice;
	    
	    DbgPrn(("Reading calibration parameters from registry...\n"));
	    status = IoOpenDeviceRegistryKey(   DeviceExtension->physicalDevObj, 
	                                        PLUGPLAY_REGKEY_DEVICE, 
	                                        KEY_WRITE, 
	                                        &hRegDevice);
	                                        
	
	    if (NT_SUCCESS(status)){	                                        
	    	DbgPrn(("Starting read..."));
	    	status = QueryDeviceKey(hRegDevice,L"Calibration",DeviceExtension->Calibration,sizeof(DeviceExtension->Calibration));
	    	
	    	if (!(NT_SUCCESS(status)))
	    	{
	    		DbgPrn(("... failed!  Status = %d\n",status));
	    		ClearCalibration(DeviceExtension);
	    	}
	    	else
	    	{
	    		DbgPrn(("... success!\n"));
	    	}
	    }
	}
}

void WriteCalibration(struct DEVICE_EXTENSION  *DeviceExtension)
{
    KIRQL Irql = KeGetCurrentIrql();
    
    if (Irql == PASSIVE_LEVEL)
    {		
	    NTSTATUS status;
	    HANDLE hRegDevice;
	    DbgPrn(("Saving calibration parameters from registry..."));
	    status = IoOpenDeviceRegistryKey(   DeviceExtension->physicalDevObj, 
	                                        PLUGPLAY_REGKEY_DEVICE, 
	                                        KEY_WRITE, 
	                                        &hRegDevice);
	                                        
	
	    if (NT_SUCCESS(status)){	                                       
	    	status = SetDeviceKey(hRegDevice,L"Calibration",DeviceExtension->Calibration,sizeof(DeviceExtension->Calibration));
	    	
	    }
	    
	    if (NT_SUCCESS(status))
	    {
	    	DbgPrn(("successful!\n"));
	    }
	    else
	    {
	    	DbgPrn(("failed!  Status = %X\n",status));
	    }
	}
}


// stupid thread that periodically writes to the registry
void WriteThread( void* Context)
{
	struct DEVICE_EXTENSION* devExt = (struct DEVICE_EXTENSION*) Context;
	LARGE_INTEGER Delay;
	LARGE_INTEGER* d = &Delay;
	NTSTATUS status;
	
	Delay.QuadPart = -100000000;
	IncrementPendingActionCount(devExt);
	while(1)
	{
		status = KeWaitForSingleObject(&devExt->WakeupThread, Executive, KernelMode, TRUE, d);
//		if (status != STATUS_TIMEOUT)
		WriteCalibration(devExt);			
		switch (devExt->ThreadAction)
		{		
			case shutdown:				
				DecrementPendingActionCount(devExt);
				PsTerminateSystemThread(STATUS_SUCCESS);

			case save:
				d = &Delay;
				break;
				
			case standby:
				d = NULL;
				break;
		}
		KeResetEvent(&devExt->WakeupThread);
				
	}
}
		



unsigned short AxisQueue(struct DEVICE_EXTENSION *DevExt, PAXIS_POINT Chain, PAXIS_POINT* Current, unsigned short position, LARGE_INTEGER Time)
{
	if ((Chain == NULL) || (Current == NULL))
		return position; // abort for no program.
	else
	{
		unsigned short return_position = position;
//		DbgPrn(("X36F: Axis parsing, position %d\n",return_position));
		
		// test for motion.  Position should be between current.position and current.next.position.
		if (((*Current) != NULL) && (position < (*Current)->position)) // if moved down...
		{
			// do move down
			DbgPrn(("X36F: Moving axis down past position %d (%X)\n",(*Current)->position,(*Current)));
			Queue(DevExt,(*Current)->frobbable,0,Time);
			(*Current) = (*Current)->previous;
			// Check moved off
			if ((*Current) == Chain) *Current = NULL; // Set to null.
			DbgPrn(("X36F: Axis now position: %X",*Current));
		}
		else if (((*Current) == NULL) && (position > (Chain->next)->position)) 
		{
			// do move up to start.
			DbgPrn(("X36F: Moving axis up past first position %d (%X)\n",(Chain->next)->position,Chain->next));
			(*Current) = Chain->next;
			Queue(DevExt,(*Current)->frobbable,1,Time);			
		}
		else if (((*Current) != NULL) && ((*Current)->next != NULL) && (position >= (*Current)->next->position))
		{
			// do move up.
			DbgPrn(("X36F: Moving axis up past position %d (%X) from %X\n",(*Current)->next->position,(*Current)->next,*Current));
			(*Current) = (*Current)->next;
			Queue(DevExt,(*Current)->frobbable,1,Time);
		}
		

#ifdef ENABLE_AXIS_TRANSLATION		
		// now do translation.
		  
		if (position == (*Current)->position)
			return_position = (*Current)->translation;
		else
		{
			// between position and position.next
			unsigned short lastposition, nextposition;
			unsigned short lasttranslation, nexttranslation;
			unsigned long dx, dy;
			unsigned long temp, offset;

			if ((*Current) == NULL) // at sentinel
			{
				lastposition = lasttranslation = 0;
			}
			else
			{
				lastposition = (*Current)->position;
				lasttranslation = (*Current)->translation;				
			}
			
			if ((*Current)->next == NULL) // at end
			{
				nextposition = nexttranslation = 0xFFFF; // full on?
			}
			else
			{
				nextposition = (*Current)->next->position;
				nexttranslation = (*Current)->next->translation;								
			}
			
			dx = nextposition - lastposition;
			dy = nexttranslation - lasttranslation;
			
			temp = (((((unsigned long) position) - ((unsigned long)lastposition)) * dy)/ dx) + ((unsigned long) lasttranslation);
			
			return_position = (unsigned short) temp;			
		}
#endif	// Axis translation		  		
		return return_position;		
	}
}
