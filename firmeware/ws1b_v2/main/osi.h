/*******************************************************************************
  * @file     
  * @author 
  * @version
  * @date 
  * @brief
  ******************************************************************************
  * @attention
  *
  *
*******************************************************************************/  

/*-------------------------------- Includes ----------------------------------*/
#ifndef __OSI_H__
#define	__OSI_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define OSI_NO_WAIT        			(0)
#define OSI_SAVE_WAIT        		(0)
#define OSI_WAIT_FOREVER   			(0xFFFFFFFF)
	
 
typedef enum
{
  OSI_OK = 0,
  OSI_FAILURE = -1,
  OSI_OPERATION_FAILED = -2,
  OSI_ABORTED = -3,
  OSI_INVALID_PARAMS = -4,
  OSI_MEMORY_ALLOCATION_FAILURE = -5,
  OSI_TIMEOUT = -6,
  OSI_EVENTS_IN_USE = -7,
  OSI_EVENT_OPEARTION_FAILURE = -8
}OsiReturnVal_e;

typedef void* OsiMsgQ_t;

/*!
	\brief 	type definition for a time value

	\note		On each porting or platform the type could be whatever is needed - integer, pointer to structure etc.
*/
//typedef unsigned int OsiTime_t;
typedef unsigned int OsiTime_t;

/*!
	\brief 	type definition for a sync object container

	Sync object is object used to synchronize between two threads or thread and interrupt handler.
	One thread is waiting on the object and the other thread send a signal, which then
	release the waiting thread.
	The signal must be able to be sent from interrupt context.
	This object is generally implemented by binary semaphore or events.

	\note	On each porting or platform the type could be whatever is needed - integer, structure etc.
*/
//typedef unsigned int OsiSyncObj_t;
typedef void * OsiSyncObj_t;

/*!
	\brief 	type definition for a locking object container

	Locking object are used to protect a resource from mutual accesses of two or more threads.
	The locking object should support re-entrant locks by a signal thread.
	This object is generally implemented by mutex semaphore

	\note	On each porting or platform the type could be whatever is needed - integer, structure etc.
*/
//typedef unsigned int OsiLockObj_t;
typedef void * OsiLockObj_t;

/*!
	\brief 	type definition for a spawn entry callback

	the spawn mechanism enable to run a function on different context.
	This mechanism allow to transfer the execution context from interrupt context to thread context
	or changing the context from an unknown user thread to general context.
	The implementation of the spawn mechanism depends on the user's system requirements and could varies
	from implementation of serialized execution using single thread to creating thread per call

	\note	The stack size of the execution thread must be at least of TBD bytes!
*/
typedef short (*P_OSI_SPAWN_ENTRY)(void* pValue);

typedef void (*P_OSI_TASK_ENTRY)(void* pValue);

typedef void* OsiTaskHandle;

/*!
	\brief 	This function creates a sync object

	The sync object is used for synchronization between different thread or ISR and
	a thread.

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_SyncObjCreate(OsiSyncObj_t* pSyncObj);

/*!
	\brief 	This function deletes a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_SyncObjDelete(OsiSyncObj_t* pSyncObj);

/*!
	\brief 		This function generates a sync signal for the object.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signalling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function could be called from ISR context
	\warning
*/
OsiReturnVal_e osi_SyncObjSignal(OsiSyncObj_t* pSyncObj);

/*!
	\brief 		This function generates a sync signal for the object.
				from ISR context.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signalling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function is called from ISR context
	\warning
*/
OsiReturnVal_e osi_SyncObjSignalFromISR(OsiSyncObj_t* pSyncObj);

/*!
	\brief 	This function waits for a sync signal of the specific sync object

	\param	pSyncObj	-	pointer to the sync object control block
	\param	Timeout		-	numeric value specifies the maximum number of mSec to
							stay suspended while waiting for the sync signal
							Currently, the simple link driver uses only two values:
								- OSI_WAIT_FOREVER
								- OSI_NO_WAIT

	\return upon successful reception of the signal within the timeout window return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_SyncObjWait(OsiSyncObj_t* pSyncObj , OsiTime_t Timeout);

/*!
	\brief 	This function clears a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful clearing the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_SyncObjClear(OsiSyncObj_t* pSyncObj);

/*!
	\brief 	This function creates a locking object.

	The locking object is used for protecting a shared resources between different
	threads.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_LockObjCreate(OsiLockObj_t* pLockObj);

/*!
	\brief 	This function deletes a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_LockObjDelete(OsiLockObj_t* pLockObj);

/*!
	\brief 	This function locks a locking object.

	All other threads that call this function before this thread calls
	the osi_LockObjUnlock would be suspended

	\param	pLockObj	-	pointer to the locking object control block
	\param	Timeout		-	numeric value specifies the maximum number of mSec to
							stay suspended while waiting for the locking object
							Currently, the simple link driver uses only two values:
								- OSI_WAIT_FOREVER
								- OSI_NO_WAIT


	\return upon successful reception of the locking object the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_LockObjLock(OsiLockObj_t* pLockObj , OsiTime_t Timeout);

/*!
	\brief 	This function unlock a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful unlocking the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_LockObjUnlock(OsiLockObj_t* pLockObj);

/*!
	\brief 	This function creates a Task.

	Creates a new Task and add it to the last of tasks that are ready to run

	\param	pEntry	-	pointer to the Task Function
	\param	pcName	-	Task Name String
	\param	usStackDepth	-	Stack Size Stack Size in 32-bit long words
	\param	pvParameters	-	pointer to structure to be passed to the Task Function
	\param	uxPriority	-	Task Priority

	\return upon successful unlocking the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_TaskCreate(P_OSI_TASK_ENTRY pEntry,const signed char * const pcName,unsigned short usStackDepth,void *pvParameters,unsigned long uxPriority,OsiTaskHandle *pTaskHandle);

/*!
	\brief 	This function Deletes a Task.

	Deletes a  Task and remove it from list of running task

	\param	pTaskHandle	-	Task Handle

	\note
	\warning
*/
void osi_TaskDelete(OsiTaskHandle* pTaskHandle);

/*******************************************************************************

This function creates a message queue that is typically used for inter thread
communication. 

Parameters:

	pMsgQ		-	pointer to the message queue control block
	pMsgQName	-	pointer to the name of the message queue
	MsgSize		-	the size of the message. 

			NOTICE: THE MESSGAE SIZE MUST BE SMALLER THAN 16

	MaxMsgs		-	maximum number of messages.

Please note that this function allocates the entire memory required 
for the maximum number of messages (MsgSize * MaxMsgs). 

********************************************************************************/
OsiReturnVal_e osi_MsgQCreate(OsiMsgQ_t* 		pMsgQ , 
							  char*				pMsgQName,
							  unsigned long 		MsgSize,
							  unsigned long 		MaxMsgs);

/*******************************************************************************

This function deletes a specific message queue.
All threads suspended waiting for a message from this queue are resumed with
an error return value. 

Parameters:

	pMsgQ		-	pointer to the message queue control block

********************************************************************************/
OsiReturnVal_e osi_MsgQDelete(OsiMsgQ_t* pMsgQ);


/*******************************************************************************

This function writes a message to a specific message queue.

Notice that the message is copied to the queue from the memory area specified 
by pMsg pointer.

--------------------------------------------------------------------------------
THIS FUNCTION COULD BE CALLED FROM ISR AS LONG AS THE TIMEOUT PARAMETER IS 
SET TO "OSI_NO_WAIT"
--------------------------------------------------------------------------------

Parameters:

	pMsgQ		-	pointer to the message queue control block
	pMsg		- 	pointer to the message
	Timeout		-	numeric value specifies the maximum number of mSec to stay 
					suspended while waiting for available space for the message

********************************************************************************/
OsiReturnVal_e osi_MsgQWrite(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout);


/*******************************************************************************

This function retrieves a message from the specified message queue. The
retrieved message is copied from the queue into the memory area specified by 
the pMsg pointer 

Parameters:

	pMsgQ		-	pointer to the message queue control block
	pMsg		- 	pointer that specify the location where to copy the message
	Timeout		-	numeric value specifies the maximum number of mSec to stay 
					suspended while waiting for a message to be available

********************************************************************************/
OsiReturnVal_e osi_MsgQRead(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout);

/*!
	\brief 	This function used to disable the tasks
	\param	- void
	\return - Key with the suspended tasks
	\note
	\warning
*/
unsigned long osi_TaskDisable(void);

/*!
	\brief 	This function used to enable all tasks
	\param	unsigned long
	\return - void
	\note
	\warning
*/
void osi_TaskEnable(unsigned long);

/*!
    \brief			Enter Critical Section
    \sa
    \note
    \warning        
*/
unsigned long osi_EnterCritical(void);

/*!
    \brief			Exit Critical Section
    \sa
    \note
    \warning        
*/
void osi_ExitCritical(unsigned long ulKey);

/*!
	\brief 	This function starts the OS Scheduler
	\param	- void
	\return - void
	\note
	\warning
*/
void osi_start(void);

/*!
	\brief 	This function used to suspend the task for the specified number of milli secs
	\param	MilliSecs	-	Time in millisecs to suspend the task
	\return void
	\note
	\warning
*/
void osi_Sleep(unsigned int MilliSecs);

/*!
    \brief			  		Allocates Memory on Heap
	\param	Size		- 	Size of the Buffer to be allocated
    \sa
    \note
    \warning
*/
void * mem_Malloc(unsigned long Size);


/*!
    \brief				Deallocates Memory
	\param	pMem		-	Pointer to the Buffer to be freed
	\return void
    \sa
    \note
    \warning
*/
void mem_Free(void *pMem);


/*!
    \brief				Set Memory
	\param	pBuf		-	Pointer to the Buffer
	\param	Val			- 	Value to be set
	\param	Size 		- 	Size of the memory to be set
    \sa
    \note
    \warning        
*/
void  mem_set(void *pBuf,int Val,size_t Size);

/*!
    \brief				Copy Memory
	\param	pDst		-	Pointer to the Destination Buffer
	\param	pSrc 		- 	Pointer to the Source Buffer
	\param	Size 		- 	Size of the memory to be copied
	\return void
    \note
    \warning        
*/
void  mem_copy(void *pDst, void *pSrc,size_t Size);

#ifdef  __cplusplus
}
#endif // __cplusplus

#endif



