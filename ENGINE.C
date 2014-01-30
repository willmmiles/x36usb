/*++

Module Name:

    engine.c


Core engine functions.

Overview:

  This module handles the actual execution of scripts.


--*/

//#define DBG 1

#include <WDM.H>
#include "filter.h"


void LoadConfiguration( struct DEVICE_EXTENSION* DevExt, PCONFIG NewConfig)
{
  PCONFIG Current = NewConfig;
  while (Current != NULL)
  {
  	if (Current->Type == 0)
  	{
    	if (Current->Button < 2*MAX_FROB)
      		DevExt->Buttons[(Current->Button)/2][(Current->Button)%2] = Current->NewCommand;
    } else if (Current->Type == 1)
    {
    	if (Current->Axis < NUM_AXIS)
    	{
    		
    		DevExt->Axes[Current->Axis] = get(&DevExt->Chains,Current->NewChain);
    		// spool to current location
    		DevExt->AxisLoc[Current->Axis] = DevExt->Axes[Current->Axis];
    		
    		while ((DevExt->AxisLoc[Current->Axis] != NULL) && (DevExt->AxisLoc[Current->Axis]->next != NULL) 
    		 		&& (DevExt->LastAxis[Current->Axis] > DevExt->AxisLoc[Current->Axis]->next->position)) {
    		 			DevExt->AxisLoc[Current->Axis] = 	DevExt->AxisLoc[Current->Axis] -> next;
    		 		}
    		 // Fix for sentinel value
    		 if (DevExt->AxisLoc[Current->Axis] == DevExt->Axes[Current->Axis]) // still on sentinel
    		 	DevExt->AxisLoc[Current->Axis] = NULL;
    	}
    }
    		
    Current = Current->Next;
  }
}


VOID ExecuteScript( IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
struct DEVICE_EXTENSION * DevExt = (struct DEVICE_EXTENSION *)DeferredContext;
    LARGE_INTEGER SysTime;
    QENTRY CurrentQ; PSCRIPT Current;
    unsigned char Op;  unsigned short Offset;
    KIRQL Old;
    
//    SCRIPT Temp, Next;

  // Dequeue items.
  KeQuerySystemTime(&SysTime);
  SysTime.QuadPart += 20; // Offset because of Windows' timer inaccuracy.
  
  KeAcquireSpinLock(&DevExt->ScriptLock,&Old);
  
  QueuePeek(&DevExt->ActionQueue, &CurrentQ);
  while ((CurrentQ.Head != NULL) && (SysTime.QuadPart > CurrentQ.Time.QuadPart))
  {
    QueuePop(&DevExt->ActionQueue, &CurrentQ);
    Current = CurrentQ.Current;
    
    if (Current != NULL)
    {
      DbgPrn(("Executing item:   Op: %d   Offset: %d   Time: %x %x\n",Current->Op,Current->Offset,CurrentQ.Time.HighPart,CurrentQ.Time.LowPart));   
      Op = Current->Op;
      Offset = Current->Offset;
      
      switch(Op)
      {
        case 11: // Mouse Y
          *(short*) &(DevExt->MouseStatus[3]) = (short) Offset;
//          DevExt->NewData = DevExt->NewData | 4; // New mouse data
//          KeSetTimer( &DevExt->MouseTimer, CurrentQ.Time, &DevExt->TimerDPC );              
          break;
        
        case 10: // Mouse X
          *(short*) &(DevExt->MouseStatus[1]) = (short) Offset;
//          DevExt->NewData = DevExt->NewData | 4; // New mouse data
//          KeSetTimer( &DevExt->MouseTimer, CurrentQ.Time, &DevExt->TimerDPC );                                    
          break;
          
        
        case 9: // Mouse release
          Offset=(Offset>3)?0:Offset-1;
          DevExt->MouseStatus[0] = DevExt->MouseStatus[0] & ~(1<<Offset);
          DevExt->NewData = DevExt->NewData | 4; // New mouse data                  
          break;
        
        case 8: // Mouse press
          Offset=(Offset>3)?0:Offset-1;
          DevExt->MouseStatus[0] = DevExt->MouseStatus[0] | (1<<Offset);
          DevExt->NewData = DevExt->NewData | 4; // New mouse data
          break;
        
        case 5: // Load new configuration.
          //Offset = Offset&0xFF;
          LoadConfiguration(DevExt,get(&DevExt->Configurations,Offset));
          break;

        case 4: // Windows hat switch command.
          Offset=(Offset>8)?0:Offset;
          DevExt->HatStatus = Offset<<4;
          DevExt->NewData = DevExt->NewData | 1; // New stick data
          break;
          
        case 3: // DirectX release
          --Offset;
          if (Offset > NUM_BUTTONS) Offset = NUM_BUTTONS;                    
          DevExt->DirectXStatus[(Offset)/8]=DevExt->DirectXStatus[(Offset)/8] & ~(1 << (Offset%8));
          DevExt->NewData = DevExt->NewData | 1; // New stick data
          break;
          
        case 2: // DirectX press
          --Offset;
          if (Offset > NUM_BUTTONS) Offset = NUM_BUTTONS;
          DevExt->DirectXStatus[(Offset)/8]=DevExt->DirectXStatus[(Offset)/8] | (1 << (Offset%8));
          DevExt->NewData = DevExt->NewData | 1; // New stick data
          break;

        case 1: // Keyboard release
          DevExt->KeyboardStatus[(Offset)/8]=DevExt->KeyboardStatus[(Offset)/8] & ~(1 << (Offset%8));       
          DevExt->NewData = DevExt->NewData | 2; // New keybd data
          break;
          
        case 0: // Keyboard press
          DevExt->KeyboardStatus[(Offset)/8]=DevExt->KeyboardStatus[(Offset)/8] | (1 << (Offset%8));
          DevExt->NewData = DevExt->NewData | 2; // New keybd data
          break;
      }
      
      if (Current->Next != NULL)
      {
        DbgPrn(("Queuing next item in comand.\n"));
        CurrentQ.Current = Current->Next;
        CurrentQ.Time.QuadPart += CurrentQ.Current->Delay;
        AddScript(&DevExt->ActionQueue, CurrentQ);
      }
    }
    QueuePeek(&DevExt->ActionQueue, &CurrentQ);
  }
  

 
  // Reset timer.
  if (CurrentQ.Head)
    KeSetTimer( &DevExt->Timer, CurrentQ.Time, &DevExt->TimerDPC );              
    

    if (DevExt->NewData)
    {
        KeInsertQueueDpc(&DevExt->ReturnDPC,NULL,NULL);
    }

	  KeReleaseSpinLock(&DevExt->ScriptLock,Old);
}
