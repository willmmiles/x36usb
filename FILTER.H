/*++

Module Name:

    filter.h

Abstract: NULL filter driver -- boilerplate code

Author:

    ervinp

Environment:

    Kernel mode

Revision History:

  Modified for X36 filter driver by Dhauzimmer.

--*/

enum deviceState {
        STATE_INITIALIZED,
        STATE_STARTING,
        STATE_STARTED,
        STATE_START_FAILED,
        STATE_STOPPED,  // implies device was previously started successfully
        STATE_SUSPENDED,
        STATE_REMOVING,
        STATE_REMOVED
};


/*
 *  Memory tag for memory blocks allocated by this driver
 *  (used in ExAllocatePoolWithTag() call).
 *  This DWORD appears as "Filt" in a little-endian memory byte dump.
 *
 *  NOTE:  PLEASE change this value to be unique for your driver!  Otherwise,
 *  your allocations will show up with every other driver that uses 'tliF' as
 *  an allocation tag.
 *
 */
#define FILTER_TAG (ULONG)'F63X'

#undef ExAllocatePool
#define ExAllocatePool(type, size) \
            ExAllocatePoolWithTag (type, size, FILTER_TAG)


// X36 configuration data structures.

typedef struct _SCRIPT {
  unsigned char Op;   // Operation to execute.
  unsigned short  Offset; // Keyboard pos/joystick button, or script pointer. ;)
  unsigned long Delay;  // Absolute delay in ms.
  struct _SCRIPT *Next;   // Next entry.
} SCRIPT, *PSCRIPT;


typedef struct _CONFIG {
  unsigned char  Type; // 0 = button, 1 = axis
  union {
  	unsigned short  Button; // Button to reprogram
  	unsigned short  Axis;
  };
  union {
  	unsigned short  NewCommand; // New script to assign.
  	unsigned short  NewChain;
  };
  struct _CONFIG *Next;   // Next entry.
} CONFIG, *PCONFIG;


// X36 queue data structures.
typedef struct _QENTRY {
  PSCRIPT     Head, Current;  // Script pointers to the first and current locations in this command.
  unsigned short   Trigger;      // Frobbable that caused the event.
  LARGE_INTEGER Time;     // System time it goes off.
  struct _QENTRY *Next;     // Next queue entry.
} QENTRY, *PQENTRY;
  

typedef struct _XQUEUE {
  PQENTRY Head;
  QENTRY Buffer[256];
  unsigned char Next,Last;
  unsigned char Free[256];

} XQUEUE, *PXQUEUE;



// axis calibration structure.
typedef struct _AXIS_CAL {
  // 0 = min, 1 = center, 2 = max
  unsigned char floating[3];
  unsigned short value[3];
  unsigned short deadzone[3];

} AXIS_CAL, *PAXIS_CAL;


// axis programming data structure
// doubly-linked list entry
typedef struct _AXIS_POINT {
	unsigned short position;
	unsigned short translation;	  // output for variable axis response, if < 0 == ignore
	unsigned short frobbable;	  // Frobbable to trigger on exit to this position.
	struct _AXIS_POINT* previous; // previous->position < this->position
	struct _AXIS_POINT* next;	  // this->position > next->position
} AXIS_POINT, *PAXIS_POINT;


// memory management structure for profiling
typedef struct __ptr_block {
  void* ptr[256];
} ptr_block;


// # of axes on an X36/45
#define NUM_AXIS 6

// # of exported buttons
#define NUM_BUTTONS 31


// Maximum number of commands.
#define MAX_COMMAND 256*256
#define MAX_CONFIG  256*256
// Maximum number of frobbables.
#define MAX_FROB    256

// Mouse movement tick - ms
#define MOUSE_TICK 10

// #define ENABLE_AXIS_TRANSLATION

// Driver extension.

#define DEVICE_EXTENSION_SIGNATURE 'F63X'

typedef struct DEVICE_EXTENSION {

    /*
     *  Memory signature of a device extension, for debugging.
     */
    ULONG signature;

    /*
     *  Plug-and-play state of this device object.
     */
    enum deviceState state;

    /*
     *  The device object that this filter driver created.
     */
    PDEVICE_OBJECT filterDevObj;

    /*
     *  The device object created by the next lower driver.
     */
    PDEVICE_OBJECT physicalDevObj;

    /*
     *  The device object at the top of the stack that we attached to.
     *  This is often (but not always) the same as physicalDevObj.
     */
    PDEVICE_OBJECT topDevObj;

    /*
     *  deviceCapabilities includes a
     *  table mapping system power states to device power states.
     */
    DEVICE_CAPABILITIES deviceCapabilities;

    /*
     *  pendingActionCount is used to keep track of outstanding actions.
     *  removeEvent is used to wait until all pending actions are
     *  completed before complete the REMOVE_DEVICE IRP and let the
     *  driver get unloaded.
     */
    LONG pendingActionCount;
    KEVENT removeEvent;
    
    // Programming device.
  PDEVICE_OBJECT ControlDevObj;


  // State data to fix report descriptor so Windows represents us as a keyboard too.
  UCHAR ReportDescType;
  UCHAR ReportBufSize;


  // Packet reporting data
  PIRP ReadIrp;  // Read request IRP, held for keyboard replies.
  KSPIN_LOCK            ReadLock; // Spinlock on event buffer.
  KDPC              TimerDPC; // For timer.
  KDPC              MouseDPC; // For mouse events, 100ms cycle.
  KDPC              ReturnDPC; // To fill out and return packets.
  
  KTIMER              MouseTimer; // For mouse events.
  
  unsigned char         NewData; // Presence of data to return to user.
  LARGE_INTEGER         StartTime; // Check packet delay.
  
  // Engine stuff  
  KTIMER              Timer; // For scripting.
  XQUEUE ActionQueue; // Timer queue with actions to perform.
  
  // Profile data
  KSPIN_LOCK ScriptLock;      // spinlock on config data
  unsigned short Buttons[MAX_FROB][2];    // Active script matrix, representing each frobbable and position.
  PAXIS_POINT	 Axes[NUM_AXIS]; // Active chain matrix, one to an axis.
  ptr_block      Commands; // Array of scripts that may be executed.
  ptr_block      Configurations; // Array of configuration deltas that may be called.
  ptr_block		 Chains; // Array of axis chains.


  // X36 state data.
  unsigned short LastAxisRaw[NUM_AXIS];  // Previous axis positions, raw form.
  unsigned short LastAxis[NUM_AXIS];  // Previous axis positions, calibrated form.
  unsigned long LastAxisTrans[NUM_AXIS]; // Previous axis positions, translated form.
  unsigned char LastPosition[0x0b];  // Previous joystick packet, axes calibrated then translated.
  unsigned char KeyboardStatus[29]; // Keyboard report packet
  unsigned char DirectXStatus[(NUM_BUTTONS/8) + (((NUM_BUTTONS%8)>0)?1:0)]; // DirectX button status
  unsigned char HatStatus; // Same.
  unsigned char MouseStatus[5]; // Mouse report packet.
  unsigned char FrobStatus[MAX_FROB]; // Array for each frobbable, storing status.
  PAXIS_POINT	AxisLoc[NUM_AXIS]; // Active axis location.

  // calibration data
  AXIS_CAL Calibration[NUM_AXIS]; // axis calibration data
  
  // Thread data to save reg data every so often.
  HANDLE SaveThread;
  KEVENT WakeupThread;
  enum {save,standby,shutdown} ThreadAction;
};

#define CDEVICE_EXTENSION_SIGNATURE 'C63X'

typedef struct CDEVICE_EXTENSION {

   /*
     *  Memory signature of a device extension, for debugging.
     */
    ULONG signature;

  struct DEVICE_EXTENSION* filterDevExt;

  unsigned char Status;
  unsigned char ReturnVal;
};


#if DBG
    #define DBGOUT(params_in_parentheses)   \
        {                                               \
            DbgPrint("'FILTER> "); \
            DbgPrint params_in_parentheses; \
            DbgPrint("\n"); \
        }
    #define TRAP(msg)  \
        {   \
            DBGOUT(("TRAP at file %s, line %d: '%s'.", __FILE__, __LINE__, msg)); \
            DbgBreakPoint(); \
        }
  #define DbgPrn(params_in_parentheses) DbgPrint params_in_parentheses ;
#else
    #define DBGOUT(params_in_parentheses)
    #define TRAP(msg)
    #define DbgPrn(params_in_parentheses)
#endif


// Number of devices we have, used for keeping track of control device names.
unsigned char NumDevices;


/*
 *  Function externs
 */
NTSTATUS    DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath);
NTSTATUS    VA_AddDevice(IN PDRIVER_OBJECT driverObj, IN PDEVICE_OBJECT pdo);
VOID        VA_DriverUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS    VA_Dispatch(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS    VA_PnP(struct DEVICE_EXTENSION *devExt, PIRP irp);
NTSTATUS    VA_Power(struct DEVICE_EXTENSION *devExt, PIRP irp);
NTSTATUS    VA_PowerComplete(IN PDEVICE_OBJECT devObj, IN PIRP irp, IN PVOID context);
NTSTATUS    GetDeviceCapabilities(struct DEVICE_EXTENSION *devExt);
NTSTATUS    CallNextDriverSync(struct DEVICE_EXTENSION *devExt, PIRP irp);
NTSTATUS    CallDriverSync(PDEVICE_OBJECT devObj, PIRP irp);
NTSTATUS    CallDriverSyncCompletion(IN PDEVICE_OBJECT devObj, IN PIRP irp, IN PVOID Context);
VOID        IncrementPendingActionCount(struct DEVICE_EXTENSION *devExt);
VOID        DecrementPendingActionCount(struct DEVICE_EXTENSION *devExt);
NTSTATUS    QueryDeviceKey (IN  HANDLE  Handle,IN  PWCHAR  ValueNameString,OUT PVOID   Data,IN  ULONG   DataLength);
NTSTATUS    SetDeviceKey (IN  HANDLE  Handle,IN  PWCHAR  ValueNameString,IN PVOID   Data,IN  ULONG   DataLength);


// IOCTL.C
NTSTATUS VA_Ioctl(struct DEVICE_EXTENSION *devExt, PIRP Irp);
NTSTATUS ConfigDescriptorCompletion( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context );
NTSTATUS ReportDescriptorCompletion( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context );
NTSTATUS ReportCompletion( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context );
void CompleteRead( IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2);
void KeyboardTimer(IN PDEVICE_OBJECT DeviceObject, IN PVOID Context);
__inline unsigned char hat8to4(unsigned char eight);
__inline unsigned char hat4to8(unsigned char four);

// Engine.c
void LoadConfiguration( struct DEVICE_EXTENSION* DevExt, PCONFIG NewConfig);
void ExecuteScript( IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2);

// profile.C
void ClearStatus(struct DEVICE_EXTENSION* devExt);
NTSTATUS ClearProfile(struct DEVICE_EXTENSION  *DeviceExtension);
// New interface functions
NTSTATUS InitControl(struct DEVICE_EXTENSION  *fDeviceExtension, PDRIVER_OBJECT DriverObject);
NTSTATUS ControlHandleIrp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
void RemoveControl(struct DEVICE_EXTENSION  *fDeviceExtension);


// Queue.c
NTSTATUS QueueInit(PXQUEUE MyQueue);
NTSTATUS AddScript(PXQUEUE MyQueue, QENTRY NewEntry);
void QueuePop(PXQUEUE MyQueue, PQENTRY Output);
void QueuePeek(PXQUEUE MyQueue, PQENTRY Output);
void QueueClear(PXQUEUE MyQueue);
void Queue(struct DEVICE_EXTENSION *devExt, unsigned short Frobbable, unsigned char Direction, LARGE_INTEGER Time);


// Axis.c
void ReadCalibration(struct DEVICE_EXTENSION  *DeviceExtension);
void WriteCalibration(struct DEVICE_EXTENSION  *DeviceExtension);
void ClearCalibration(struct DEVICE_EXTENSION  *DeviceExtension);
void ClearAxis(PAXIS_CAL Axis);
unsigned short Calculate(PAXIS_CAL Axis, unsigned short raw);
unsigned short AxisQueue(struct DEVICE_EXTENSION *DevExt, PAXIS_POINT Chain, PAXIS_POINT* Current,unsigned short position, LARGE_INTEGER Time);
void WriteThread( void* Context);


// Memory.c
void* get(ptr_block* top_level, unsigned int loc);
void** getloc2(ptr_block* top_level, unsigned int loc);
void flush(ptr_block* p);
void flush2(ptr_block* top_level);
