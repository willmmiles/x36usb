// QUEUE.C
//
// Handles the timer queue for the X36 scripting engine.
// 
// includes:
//
// queueinit: inits queue.
// scriptadd: adds a whole script to the queue.
// pop: removes the first item from the queue.
// peek: returns the first item in the queue without destroying it
// queueclear: destroys a queue.

//#define DBG 1
#include <WDM.H>
#include "filter.h"


NTSTATUS QueueInit(PXQUEUE MyQueue)
{
  unsigned int i;
  
  if (MyQueue == NULL)
    return STATUS_INVALID_PARAMETER;
  
  MyQueue->Head = NULL;
  MyQueue->Next = 0; MyQueue->Last = 0;
  for (i=0; i<256; i++)
    MyQueue->Free[i]=(unsigned char)i;
  
  
  return STATUS_SUCCESS;
}

NTSTATUS AddScript(PXQUEUE MyQueue, QENTRY NewEntry)
{
  PQENTRY QNew, QCurrent, QPrev;
  KIRQL Old;
  
  
  DbgPrn(("Entering AddScript.\n"));
  if (MyQueue == NULL)
    return STATUS_INVALID_PARAMETER;
  
  if (NewEntry.Current == NULL)
    return STATUS_SUCCESS;
    
  if (MyQueue->Next - MyQueue->Last < 255)
  {
    DbgPrn(("Assigning new space in buffer.."));
    QNew = &MyQueue->Buffer[MyQueue->Free[MyQueue->Next]];
    MyQueue->Next++;
    DbgPrn(("..assigned.  Position: %d     Next: %d  NextPos: %d  Last: %d  LastPos: %d\n",MyQueue->Free[MyQueue->Next-1],MyQueue->Next,MyQueue->Free[MyQueue->Next],MyQueue->Last,MyQueue->Free[MyQueue->Last]));
    RtlCopyMemory(QNew,&NewEntry,sizeof(QENTRY));
    QNew->Next = NULL;
    DbgPrn(("Queuing item:  %x  Head: %x   Current: %x   Time: %x %x\n",QNew,QNew->Head,QNew->Current,QNew->Time.HighPart,QNew->Time.LowPart));   
    
    if (MyQueue->Head == NULL)
      MyQueue->Head = QNew;
    else
    {
      QCurrent=MyQueue->Head;
      QPrev=NULL;     
      
      while((QCurrent != NULL) && (QNew->Time.QuadPart > QCurrent->Time.QuadPart))
      {
        QPrev = QCurrent;
        QCurrent = QCurrent -> Next;
      }
      
      DbgPrn(("Queue position:   Head: %x  Prev: %x   New: %x   Current: %x\n",MyQueue->Head,QPrev,QNew,QCurrent));
      if (QPrev != NULL)
      {
        QNew->Next = QCurrent;
        QPrev->Next = QNew;
      }
      else
      {
        MyQueue->Head = QNew;
        QNew->Next = QCurrent;        
      }
      DbgPrn(("Queue status:   Head: %x   Next: %x\n",MyQueue->Head,(MyQueue->Head==NULL)?NULL:MyQueue->Head->Next));
    }   
  } 
  else
  {
    DbgPrn(("Addscript aborted due to insufficent resources.\n"));
    return STATUS_INSUFFICIENT_RESOURCES;
  }

  
  return STATUS_SUCCESS;
}



void QueuePop(PXQUEUE MyQueue, PQENTRY Output)
{
  PQENTRY QCurrent, QReturn;
  
  if ((MyQueue == NULL) || (Output==NULL))
    return;
  
  QCurrent = MyQueue->Head;
/*  if (QCurrent == QCurrent->Next)
    DbgBreakPoint();*/
  if (QCurrent != NULL)
  {
    RtlCopyMemory(Output,QCurrent,sizeof(QENTRY));  
    MyQueue->Head = QCurrent->Next;
    MyQueue->Free[MyQueue->Last]=(unsigned char) (QCurrent - &MyQueue->Buffer[0]);//sizeof(QENTRY);
    DbgPrn(("Next: %d  NextPos: %d  Last: %d  LastPos: %d\n",MyQueue->Next,MyQueue->Free[MyQueue->Next],MyQueue->Last,MyQueue->Free[MyQueue->Last]));
    MyQueue->Last++;
  }
  else
    Output->Head=NULL;

}

void QueuePeek(PXQUEUE MyQueue, PQENTRY Output)
{
  if ((MyQueue == NULL) || (Output==NULL))
    return;
    
  if (MyQueue->Head != NULL)
    RtlCopyMemory(Output,MyQueue->Head,sizeof(QENTRY));
  else
    Output->Head = NULL;
  
}

void QueueClear(PXQUEUE MyQueue)
{
  PQENTRY QCurrent;
  
  if (MyQueue == NULL)
    return;
    
  while(MyQueue->Head != NULL)
  {
    QCurrent = MyQueue->Head;
    MyQueue->Head = QCurrent->Next;
    MyQueue->Free[MyQueue->Last]=(QCurrent - &MyQueue->Buffer[0])/sizeof(QENTRY);   
    MyQueue->Last++;
  } 
}


void Queue(struct DEVICE_EXTENSION *devExt, unsigned short Frobbable, unsigned char Direction, LARGE_INTEGER Time)
{
    QENTRY QNew;
    unsigned short Command = devExt->Buttons[Frobbable][Direction];
    
    DbgPrn(("Queing command: %d\n",Command));
    //Command = Command%MAX_COMMAND;

  if (get(&devExt->Commands,Command) != NULL)
  {
    QNew.Head = QNew.Current = get(&devExt->Commands,Command);
    QNew.Trigger = Frobbable;
    QNew.Time.QuadPart = Time.QuadPart + QNew.Head->Delay;
    AddScript(&devExt->ActionQueue, QNew);
  }
}
