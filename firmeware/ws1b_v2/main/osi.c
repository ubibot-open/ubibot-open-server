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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "osi.h"
#include "string.h"

portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

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
OsiReturnVal_e osi_SyncObjCreate(OsiSyncObj_t* pSyncObj)
{
	//Check for NULL
	if(NULL == pSyncObj)
	{
		return OSI_INVALID_PARAMS;
	}
	SemaphoreHandle_t *pl_SyncObj = (SemaphoreHandle_t *)pSyncObj;

	*pl_SyncObj = xSemaphoreCreateBinary();

	if((SemaphoreHandle_t)(*pSyncObj) != NULL)
	{
		return OSI_OK; 
	}
	else
	{
		return OSI_OPERATION_FAILED;
	}
}

/*!
	\brief 	This function deletes a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_SyncObjDelete(OsiSyncObj_t* pSyncObj)
{
	//Check for NULL
	if(NULL == pSyncObj)
	{
		return OSI_INVALID_PARAMS;
	}
	vSemaphoreDelete(*pSyncObj );
	
	return OSI_OK;
}

/*!
	\brief  This function generates a sync signal for the object.

	All suspended threads waiting on this sync object are resumed

	\param  pSyncObj	-	pointer to the sync object control block

	\return  upon successful signaling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function could be called from ISR context
	\warning
*/
OsiReturnVal_e osi_SyncObjSignal(OsiSyncObj_t* pSyncObj)
{
	//Check for NULL
	if(NULL == pSyncObj)
	{
		return OSI_INVALID_PARAMS;
	}

	if(pdTRUE != xSemaphoreGive( *pSyncObj ))
	{
		//In case of Semaphore, you are expected to get this if multiple sem
		// give is called before sem take
		return OSI_OK;
	}
	
	return OSI_OK;
}

/*!
	\brief 		This function generates a sync signal for the object
				from ISR context.

	All suspended threads waiting on this sync object are resumed

	\param		pSyncObj	-	pointer to the sync object control block

	\return 	upon successful signalling the function should return 0
				Otherwise, a negative value indicating the error code shall be returned
	\note		the function is called from ISR context
	\warning
*/
OsiReturnVal_e osi_SyncObjSignalFromISR(OsiSyncObj_t* pSyncObj)
{
	//Check for NULL
	if(NULL == pSyncObj)
	{
		return OSI_INVALID_PARAMS;
	}
	xHigherPriorityTaskWoken = pdFALSE;
	if(pdTRUE == xSemaphoreGiveFromISR( *pSyncObj, &xHigherPriorityTaskWoken ))
	{
		if( xHigherPriorityTaskWoken )
		{
			taskYIELD ();
		}
		return OSI_OK;
	}
	else
	{
		//In case of Semaphore, you are expected to get this if multiple sem
		// give is called before sem take
		return OSI_OK;
	}
}

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
OsiReturnVal_e osi_SyncObjWait(OsiSyncObj_t* pSyncObj , OsiTime_t Timeout)
{
    //Check for NULL
    if(NULL == pSyncObj)
    {
            return OSI_INVALID_PARAMS;
    }
    if(pdTRUE == xSemaphoreTake( (SemaphoreHandle_t)*pSyncObj, ( TickType_t )(Timeout/portTICK_PERIOD_MS) ))
    {
        return OSI_OK;
    }
    else
    {
        return OSI_OPERATION_FAILED;
    }
}

/*!
	\brief 	This function clears a sync object

	\param	pSyncObj	-	pointer to the sync object control block

	\return upon successful clearing the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_SyncObjClear(OsiSyncObj_t* pSyncObj)
{
	//Check for NULL
	if(NULL == pSyncObj)
	{
		return OSI_INVALID_PARAMS;
	}

    if (OSI_OK == osi_SyncObjWait(pSyncObj,0) )
    {
        return OSI_OK;
    }
    else
    {
        return OSI_OPERATION_FAILED;
    }
}

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
OsiReturnVal_e osi_LockObjCreate(OsiLockObj_t* pLockObj)
{
	//Check for NULL
	if(NULL == pLockObj)
	{
		return OSI_INVALID_PARAMS;
	}
	*pLockObj = (OsiLockObj_t)xSemaphoreCreateMutex();
	if(pLockObj != NULL)
	{  
		return OSI_OK;
	}
	else
	{
		return OSI_OPERATION_FAILED;
	}
}

/*!
	\brief 	This function deletes a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful deletion the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_LockObjDelete(OsiLockObj_t* pLockObj)
{
    vSemaphoreDelete((SemaphoreHandle_t)*pLockObj );
    return OSI_OK;
}

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
OsiReturnVal_e osi_LockObjLock(OsiLockObj_t* pLockObj , OsiTime_t Timeout)
{
	//Check for NULL
	if(NULL == pLockObj)
	{
		return OSI_INVALID_PARAMS;
	}
	//Take Semaphore
	if(pdTRUE == xSemaphoreTake( (SemaphoreHandle_t)*pLockObj, ( TickType_t ) (Timeout/portTICK_PERIOD_MS) ))
	{
		return OSI_OK;
	}
	else
	{
		return OSI_OPERATION_FAILED;
	}
}

/*!
	\brief 	This function unlock a locking object.

	\param	pLockObj	-	pointer to the locking object control block

	\return upon successful unlocking the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_LockObjUnlock(OsiLockObj_t* pLockObj)
{
	//Check for NULL
	if(NULL == pLockObj)
	{
		return OSI_INVALID_PARAMS;
	}
	//Release Semaphore
	if(pdTRUE == xSemaphoreGive( (SemaphoreHandle_t)*pLockObj ))
	{
		return OSI_OK;
	}
	else
	{
		return OSI_OPERATION_FAILED;
	}
}

/*!
	\brief 	This function creates a Task.

	Creates a new Task and add it to the last of tasks that are ready to run

	\param	pEntry	-	pointer to the Task Function
	\param	pcName	-	Task Name String
	\param	usStackDepth	-	Stack Size in bytes
	\param	pvParameters	-	pointer to structure to be passed to the Task Function
	\param	uxPriority	-	Task Priority

	\return upon successful creation the function should return 0
			Otherwise, a negative value indicating the error code shall be returned
	\note
	\warning
*/
OsiReturnVal_e osi_TaskCreate(P_OSI_TASK_ENTRY pEntry,const signed char * const pcName,
                              unsigned short usStackDepth, void *pvParameters,
                              unsigned long uxPriority,OsiTaskHandle* pTaskHandle)
{
	if(pdPASS == xTaskCreate( pEntry, (char const*)pcName,
                                (usStackDepth/(sizeof( portSTACK_TYPE ))), 
                                pvParameters,(unsigned portBASE_TYPE)uxPriority,
                                (TaskHandle_t*)pTaskHandle ))
	{
		return OSI_OK;
	}

	return OSI_OPERATION_FAILED;	
}


/*!
	\brief 	This function Deletes a Task.

	Deletes a  Task and remove it from list of running task

	\param	pTaskHandle	-	Task Handle

	\note
	\warning
*/
void osi_TaskDelete(OsiTaskHandle* pTaskHandle)
{
	vTaskDelete((TaskHandle_t)*pTaskHandle);
}

/*!
	\brief 	This function is used to create the MsgQ

	\param	pMsgQ	-	pointer to the message queue
	\param	pMsgQName	-	msg queue name
	\param	MsgSize	-	size of message on the queue
	\param	MaxMsgs	-	max. number of msgs that the queue can hold

	\return - OsiReturnVal_e
	\note
	\warning
*/
OsiReturnVal_e osi_MsgQCreate(OsiMsgQ_t* 		pMsgQ , 
			      char*			pMsgQName,
			      unsigned long 		MsgSize,
			      unsigned long		MaxMsgs)
{
	//Check for NULL
	if(NULL == pMsgQ)
	{
		return OSI_INVALID_PARAMS;
	}

	QueueHandle_t handle =0;

	//Create Queue
	handle = xQueueCreate( MaxMsgs, MsgSize );
	if (handle==0)
	{
		return OSI_OPERATION_FAILED;
	}

	*pMsgQ = (OsiMsgQ_t)handle;
	return OSI_OK;
}

/*!
	\brief 	This function is used to delete the MsgQ

	\param	pMsgQ	-	pointer to the message queue

	\return - OsiReturnVal_e
	\note
	\warning
*/
OsiReturnVal_e osi_MsgQDelete(OsiMsgQ_t* pMsgQ)
{
	//Check for NULL
	if(NULL == pMsgQ)
	{
		return OSI_INVALID_PARAMS;
	}
	vQueueDelete((QueueHandle_t) *pMsgQ );
    return OSI_OK;
}

/*!
	\brief 	This function is used to write data to the MsgQ

	\param	pMsgQ	-	pointer to the message queue
	\param	pMsg	-	pointer to the Msg strut to read into
	\param	Timeout	-	timeout to wait for the Msg to be available

	\return - OsiReturnVal_e
	\note
	\warning
*/

OsiReturnVal_e osi_MsgQWrite(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout)
{
	//Check for NULL
	if(NULL == pMsgQ)
	{
		return OSI_INVALID_PARAMS;
	}

    // if(pdPASS == xQueueSendFromISR((QueueHandle_t) *pMsgQ, pMsg, &xHigherPriorityTaskWoken ))
	if(pdPASS == xQueueSend((QueueHandle_t) *pMsgQ, pMsg, ( TickType_t ) (Timeout/portTICK_PERIOD_MS) ))
    {
		taskYIELD ();
		return OSI_OK;
    }
	else
	{
		return OSI_OPERATION_FAILED;
	}
}
/*!
	\brief 	This function is used to read data from the MsgQ

	\param	pMsgQ	-	pointer to the message queue
	\param	pMsg	-	pointer to the Msg strut to read into
	\param	Timeout	-	timeout to wait for the Msg to be available

	\return - OsiReturnVal_e
	\note
	\warning
*/

OsiReturnVal_e osi_MsgQRead(OsiMsgQ_t* pMsgQ, void* pMsg , OsiTime_t Timeout)
{
	//Check for NULL
	if(NULL == pMsgQ)
	{
		return OSI_INVALID_PARAMS;
	}

	if ( Timeout == OSI_WAIT_FOREVER )
	{
		Timeout = portMAX_DELAY ;
	}

	//Receive Item from Queue
	if( pdTRUE  == xQueueReceive((QueueHandle_t)*pMsgQ,pMsg,( TickType_t ) (Timeout/portTICK_PERIOD_MS)) )
	{
		return OSI_OK;
	}
	else
	{
		return OSI_OPERATION_FAILED;
	}
}

/*!
	\brief 	This function used to disable the tasks
	\param	- void
	\return - Key with the suspended tasks
	\note
	\warning
*/
unsigned long osi_TaskDisable(void)
{
   vTaskSuspendAll();

   return OSI_OK;
}

/*!
	\brief 	This function used to resume all the tasks
	\param	key	-	returned from suspend tasks
	\return - void
	\note
	\warning
*/
void osi_TaskEnable(unsigned long key)
{
   xTaskResumeAll();
}

/*!
	\brief 	This function use to entering into critical section
	\param	void		
	\return - void 
	\note
	\warning
*/
unsigned long osi_EnterCritical(void)
{
     vPortEnterCritical();
    return 0;
}

/*!
	\brief 	This function use to exit critical section
	\param	void		
	\return - void 
	\note
	\warning
*/

void osi_ExitCritical(unsigned long ulKey)
{
     vPortExitCritical();
}

/*!
	\brief 	This function used to start the scheduler
	\param	void
	\return - void
	\note
	\warning
*/
void osi_start(void)
{
	vTaskStartScheduler();
}

/*!
	\brief 	This function used to suspend the task for the specified number of milli secs
	\param	MilliSecs	-	Time in millisecs to suspend the task
	\return - void
	\note
	\warning
*/
void osi_Sleep(unsigned int MilliSecs)
{
	TickType_t xDelay = MilliSecs / portTICK_PERIOD_MS;
	vTaskDelay(xDelay);
}

/*!
	\brief 	This function to call the memory de-allocation function of the FREERTOS

	\param	Size	-	size of memory to alloc in bytes

	\return - void *
	\note
	\warning
*/

void * mem_Malloc(unsigned long Size)
{
	return ( void * ) pvPortMalloc( (size_t)Size );
}

/*!
	\brief 	This function to call the memory de-allocation function of the FREERTOS

	\param	pMem		-	pointer to the memory which needs to be freed
	
	\return - void 
	\note
	\warning
*/
void mem_Free(void *pMem)
{
	vPortFree( pMem );
}

/*!
	\brief 	This function call the memset function
	\param	pBuf	     -	 pointer to the memory to be fill
        \param  Val          -   Value to be fill
        \param  Size         -   Size of the memory which needs to be fill
	\return - void 
	\note
	\warning
*/

void  mem_set(void *pBuf,int Val,size_t Size)
{
	memset( pBuf,Val,Size);
}

/*!
      \brief 	This function call the memcopy function
      \param	pDst	-	pointer to the destination
      \param pSrc     -   pointer to the source
      \param Size     -   Size of the memory which needs to be copy
      
      \return - void 
      \note
      \warning
*/
void  mem_copy(void *pDst, void *pSrc,size_t Size)
{
    memcpy(pDst,pSrc,Size);
}







