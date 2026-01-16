/*
 * sys_arch_static.c - Static Memory Allocation Port for LwIP on FreeRTOS
 *
 * Replaces the default sys_arch.c to comply with "No Dynamic Allocation" policy.
 * Uses pre-allocated pools for Queues (Mailboxes), Semaphores, Mutexes, and Tasks.
 */

/* ------------------------ System architecture includes ----------------------------- */
#include "arch/sys_arch.h"

/* ------------------------ lwIP includes --------------------------------- */
#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/def.h"
#include "lwip/sys.h"
#include "lwip/mem.h"
#include "lwip/stats.h"

/* ------------------------ FreeRTOS includes ----------------------------- */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include <stdlib.h> // for rand()

/* ------------------------ Configuration Constants ----------------------- */
/* Adjust these based on your application needs (Check lwipopts.h values) */
#define SYS_MBOX_POOL_SIZE      (TCPIP_MBOX_SIZE + DEFAULT_UDP_RECVMBOX_SIZE + DEFAULT_TCP_RECVMBOX_SIZE + 4)
#define SYS_SEM_POOL_SIZE       (10)
#define SYS_MUTEX_POOL_SIZE     (10)
#define SYS_THREAD_POOL_SIZE    (4)

/* Max number of items per queue (mailbox). Must be >= largest queue size in lwipopts.h */
#define MAX_QUEUE_ENTRIES       (16) 

/* ------------------------ Static Memory Definitions --------------------- */

/* --- Mailbox (Queue) Pool --- */
typedef struct {
    StaticQueue_t xStaticQueue;                         /* FreeRTOS Static Queue Control Block */
    uint8_t ucQueueStorage[MAX_QUEUE_ENTRIES * sizeof(void*)]; /* Actual Data Storage Area */
    uint8_t is_used;                                    /* Allocation Flag */
} static_mbox_t;

static static_mbox_t mbox_pool[SYS_MBOX_POOL_SIZE];

/* --- Semaphore Pool --- */
typedef struct {
    StaticSemaphore_t xStaticSem;
    uint8_t is_used;
} static_sem_t;

static static_sem_t sem_pool[SYS_SEM_POOL_SIZE];

/* --- Mutex Pool --- */
typedef struct {
    StaticSemaphore_t xStaticMutex;
    uint8_t is_used;
} static_mutex_t;

static static_mutex_t mutex_pool[SYS_MUTEX_POOL_SIZE];

/* --- Thread (Task) Pool --- */
/* Note: Stack size is fixed here for simplicity. 
   If your main thread needs a huge stack, you might need to increase this. */
#define SYS_THREAD_STACK_SIZE   (1024) 

typedef struct {
    StaticTask_t xStaticTask;
    StackType_t xStack[SYS_THREAD_STACK_SIZE];
    uint8_t is_used;
} static_thread_t;

static static_thread_t thread_pool[SYS_THREAD_POOL_SIZE];


/* ------------------------ Helper Functions ------------------------------ */
portBASE_TYPE xInsideISR = pdFALSE;

/* ------------------------ Implementation -------------------------------- */

/*
 * Initialize the system architecture port
 */
void sys_init(void)
{
    int i;
    
    // Initialize Memory Pools
    for(i = 0; i < SYS_MBOX_POOL_SIZE; i++) mbox_pool[i].is_used = 0;
    for(i = 0; i < SYS_SEM_POOL_SIZE; i++)  sem_pool[i].is_used = 0;
    for(i = 0; i < SYS_MUTEX_POOL_SIZE; i++) mutex_pool[i].is_used = 0;
    for(i = 0; i < SYS_THREAD_POOL_SIZE; i++) thread_pool[i].is_used = 0;

    srand(rand());
}

/*
 * Create a new mailbox (queue) using static memory
 */
err_t sys_mbox_new( sys_mbox_t *pxMailBox, int iSize )
{
    int i;
    sys_mbox_t pxTempMbox;

    if (iSize > MAX_QUEUE_ENTRIES) {
        // Requested queue size is larger than our static buffer permits
        return ERR_MEM;
    }

    taskENTER_CRITICAL();
    for (i = 0; i < SYS_MBOX_POOL_SIZE; i++) {
        if (mbox_pool[i].is_used == 0) {
            mbox_pool[i].is_used = 1;
            break;
        }
    }
    taskEXIT_CRITICAL();

    if (i == SYS_MBOX_POOL_SIZE) {
        SYS_STATS_INC(mbox.err);
        return ERR_MEM; // Pool full
    }

    // Create Static Queue
    pxTempMbox.xMbox = xQueueCreateStatic(
        (UBaseType_t) iSize,
        sizeof(void *),
        mbox_pool[i].ucQueueStorage,
        &mbox_pool[i].xStaticQueue
    );

    if (pxTempMbox.xMbox != NULL) {
        pxTempMbox.xTask = NULL;
        *pxMailBox = pxTempMbox;
        SYS_STATS_INC_USED(mbox);
        return ERR_OK;
    } else {
        // Should rarely happen with static alloc unless params are wrong
        mbox_pool[i].is_used = 0; 
        SYS_STATS_INC(mbox.err);
        return ERR_MEM;
    }
}

/*
 * Free a mailbox
 */
void sys_mbox_free( sys_mbox_t *pxMailBox )
{
    int i;
    unsigned long ulMessagesWaiting;
    QueueHandle_t xMboxHandle;

    if ((pxMailBox == NULL) || (pxMailBox->xMbox == NULL)) return;

    xMboxHandle = pxMailBox->xMbox;
    ulMessagesWaiting = uxQueueMessagesWaiting( xMboxHandle );
    configASSERT( ( ulMessagesWaiting == 0 ) );

    #if SYS_STATS
    if( ulMessagesWaiting != 0UL ) {
        SYS_STATS_INC( mbox.err );
    }
    SYS_STATS_DEC( mbox.used );
    #endif

    // Find the pool item corresponding to this handle to mark it free
    taskENTER_CRITICAL();
    
    // Invalidate the handle in the LwIP struct first
    pxMailBox->xMbox = NULL;
    
    // Search pool for this handle
    // Note: The handle returned by xQueueCreateStatic is a pointer to xStaticQueue structure
    for(i=0; i<SYS_MBOX_POOL_SIZE; i++) {
        if ( (QueueHandle_t)&mbox_pool[i].xStaticQueue == xMboxHandle ) {
             // Free the FreeRTOS object
             vQueueDelete(xMboxHandle);
             
             // Mark pool slot as free
             mbox_pool[i].is_used = 0;
             break;
        }
    }
    taskEXIT_CRITICAL();
}

/*
 * Create a new Semaphore using static memory
 */
err_t sys_sem_new( sys_sem_t *pxSemaphore, u8_t ucCount )
{
    int i;
    
    taskENTER_CRITICAL();
    for (i = 0; i < SYS_SEM_POOL_SIZE; i++) {
        if (sem_pool[i].is_used == 0) {
            sem_pool[i].is_used = 1;
            break;
        }
    }
    taskEXIT_CRITICAL();

    if (i == SYS_SEM_POOL_SIZE) {
        SYS_STATS_INC(sem.err);
        return ERR_MEM;
    }

    // Create Static Binary Semaphore
    *pxSemaphore = xSemaphoreCreateBinaryStatic( &sem_pool[i].xStaticSem );

    if (*pxSemaphore != NULL) {
        if( ucCount == 0U ) {
            xSemaphoreTake( *pxSemaphore, 1UL );
        } else {
            // Binary semaphores created using Static are empty by default (taken),
            // so if count is 1, we must give it.
            xSemaphoreGive( *pxSemaphore );
        }
        SYS_STATS_INC_USED(sem);
        return ERR_OK;
    } else {
        sem_pool[i].is_used = 0;
        SYS_STATS_INC(sem.err);
        return ERR_MEM;
    }
}

/*
 * Free a semaphore
 */
void sys_sem_free( sys_sem_t *pxSemaphore )
{
    int i;
    if ((pxSemaphore == NULL) || (*pxSemaphore == NULL)) return;

    SYS_STATS_DEC(sem.used);

    taskENTER_CRITICAL();
    for(i=0; i<SYS_SEM_POOL_SIZE; i++) {
        if( (SemaphoreHandle_t)&sem_pool[i].xStaticSem == *pxSemaphore ) {
            vSemaphoreDelete(*pxSemaphore);
            sem_pool[i].is_used = 0;
            break;
        }
    }
    taskEXIT_CRITICAL();
    *pxSemaphore = NULL;
}

/*
 * Create a new Mutex using static memory
 */
err_t sys_mutex_new( sys_mutex_t *pxMutex )
{
    int i;
    
    taskENTER_CRITICAL();
    for (i = 0; i < SYS_MUTEX_POOL_SIZE; i++) {
        if (mutex_pool[i].is_used == 0) {
            mutex_pool[i].is_used = 1;
            break;
        }
    }
    taskEXIT_CRITICAL();

    if (i == SYS_MUTEX_POOL_SIZE) {
        SYS_STATS_INC(mutex.err);
        return ERR_MEM;
    }

    *pxMutex = xSemaphoreCreateMutexStatic( &mutex_pool[i].xStaticMutex );

    if (*pxMutex != NULL) {
        SYS_STATS_INC_USED(mutex);
        return ERR_OK;
    } else {
        mutex_pool[i].is_used = 0;
        SYS_STATS_INC(mutex.err);
        return ERR_MEM;
    }
}

/*
 * Free a mutex
 */
void sys_mutex_free( sys_mutex_t *pxMutex )
{
    int i;
    if ((pxMutex == NULL) || (*pxMutex == NULL)) return;

    SYS_STATS_DEC(mutex.used);

    taskENTER_CRITICAL();
    for(i=0; i<SYS_MUTEX_POOL_SIZE; i++) {
        if( (SemaphoreHandle_t)&mutex_pool[i].xStaticMutex == *pxMutex ) {
            vSemaphoreDelete(*pxMutex);
            mutex_pool[i].is_used = 0;
            break;
        }
    }
    taskEXIT_CRITICAL();
    *pxMutex = NULL;
}

/*
 * Create a new Thread using static memory
 */
sys_thread_t sys_thread_new( const char *pcName, void( *pxThread )( void *pvParameters ), void *pvArg, int iStackSize, int iPriority )
{
    int i;
    TaskHandle_t xCreatedTask;

    /* Verify stack size fits in our static buffer definition */
    if ((iStackSize/sizeof(StackType_t)) > SYS_THREAD_STACK_SIZE) {
        return NULL;
    }

    taskENTER_CRITICAL();
    for (i = 0; i < SYS_THREAD_POOL_SIZE; i++) {
        if (thread_pool[i].is_used == 0) {
            thread_pool[i].is_used = 1;
            break;
        }
    }
    taskEXIT_CRITICAL();

    if (i == SYS_THREAD_POOL_SIZE) {
        return NULL; // No thread slots available
    }

    xCreatedTask = xTaskCreateStatic(
        pxThread,
        pcName,
        (uint32_t)(iStackSize / sizeof(StackType_t)), /* Convert bytes to words */
        pvArg,
        iPriority,
        thread_pool[i].xStack,
        &thread_pool[i].xStaticTask
    );

    if (xCreatedTask == NULL) {
        thread_pool[i].is_used = 0;
    }

    return xCreatedTask;
}

/*
 * Standard functions that don't depend on allocation scheme
 */

void sys_mbox_post( sys_mbox_t *pxMailBox, void *pxMessageToPost )
{
    while( xQueueSendToBack( pxMailBox->xMbox, &pxMessageToPost, portMAX_DELAY ) != pdTRUE );
}

err_t sys_mbox_trypost( sys_mbox_t *pxMailBox, void *pxMessageToPost )
{
    err_t xReturn;
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    if( xInsideISR != pdFALSE )
    {
        xReturn = xQueueSendFromISR( pxMailBox->xMbox, &pxMessageToPost, &xHigherPriorityTaskWoken );
    }
    else
    {
        xReturn = xQueueSend( pxMailBox->xMbox, &pxMessageToPost, ( TickType_t ) 0 );
    }

    if( xReturn == pdPASS )
    {
        xReturn = ERR_OK;
    }
    else
    {
        xReturn = ERR_MEM;
        SYS_STATS_INC( mbox.err );
    }

    return xReturn;
}

err_t sys_mbox_trypost_fromisr( sys_mbox_t *pxMailBox, void *pxMessageToPost )
{
    err_t xReturn;

    xInsideISR = pdTRUE;
    xReturn = sys_mbox_trypost( pxMailBox, pxMessageToPost );
    xInsideISR = pdFALSE;

    return xReturn;
}

u32_t sys_arch_mbox_fetch( sys_mbox_t *pxMailBox, void **ppvBuffer, u32_t ulTimeOut )
{
    void *pvDummy;
    unsigned long ulReturn = SYS_ARCH_TIMEOUT;
    QueueHandle_t xMbox;
    TaskHandle_t xTask;
    BaseType_t xResult;

    if( pxMailBox == NULL ) goto exit;

    taskENTER_CRITICAL();
    xMbox = pxMailBox->xMbox;
    xTask = xTaskGetCurrentTaskHandle();
    if( ( xMbox != NULL ) && ( xTask != NULL ) && ( pxMailBox->xTask == NULL ) )
    {
        pxMailBox->xTask = xTask;
    }
    else
    {
        xMbox = NULL;
    }
    taskEXIT_CRITICAL();

    if( xMbox == NULL ) goto exit;

    if( NULL == ppvBuffer ) ppvBuffer = &pvDummy;

    if( ulTimeOut != 0UL )
    {
        configASSERT( xInsideISR == ( portBASE_TYPE ) 0 );
        if( pdTRUE == xQueueReceive( xMbox, &( *ppvBuffer ), ulTimeOut/ portTICK_PERIOD_MS ) )
        {
            ulReturn = 1UL;
        }
        else
        {
            *ppvBuffer = NULL;
        }
    }
    else
    {
        for( xResult = pdFALSE; ( xMbox != NULL ) && ( xResult != pdTRUE ); )
        {
            xResult = xQueueReceive( xMbox, &( *ppvBuffer ), portMAX_DELAY );
            xMbox = pxMailBox->xMbox;
        }

        if( xResult == pdTRUE )
        {
            ulReturn = 1UL;
        }
    }

    pxMailBox->xTask = NULL;

exit:
    return ulReturn;
}

u32_t sys_arch_mbox_tryfetch( sys_mbox_t *pxMailBox, void **ppvBuffer )
{
    void *pvDummy;
    unsigned long ulReturn;
    long lResult;
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    if( ppvBuffer== NULL ) ppvBuffer = &pvDummy;

    if( xInsideISR != pdFALSE )
    {
        lResult = xQueueReceiveFromISR( pxMailBox->xMbox, &( *ppvBuffer ), &xHigherPriorityTaskWoken );
    }
    else
    {
        lResult = xQueueReceive( pxMailBox->xMbox, &( *ppvBuffer ), 0UL );
    }

    if( lResult == pdPASS )
    {
        ulReturn = ERR_OK;
    }
    else
    {
        ulReturn = SYS_MBOX_EMPTY;
    }

    return ulReturn;
}

u32_t sys_arch_sem_wait( sys_sem_t *pxSemaphore, u32_t ulTimeout )
{
    TickType_t xStartTime, xEndTime, xElapsed;
    unsigned long ulReturn;

    xStartTime = xTaskGetTickCount();

    if( ulTimeout != 0UL )
    {
        if( xSemaphoreTake( *pxSemaphore, ulTimeout / portTICK_PERIOD_MS ) == pdTRUE )
        {
            xEndTime = xTaskGetTickCount();
            xElapsed = (xEndTime - xStartTime) * portTICK_PERIOD_MS;
            ulReturn = xElapsed;
        }
        else
        {
            ulReturn = SYS_ARCH_TIMEOUT;
        }
    }
    else
    {
        while( xSemaphoreTake( *pxSemaphore, portMAX_DELAY ) != pdTRUE );
        xEndTime = xTaskGetTickCount();
        xElapsed = ( xEndTime - xStartTime ) * portTICK_PERIOD_MS;

        if( xElapsed == 0UL )
        {
            xElapsed = 1UL;
        }

        ulReturn = xElapsed;
    }

    return ulReturn;
}

void sys_mutex_lock( sys_mutex_t *pxMutex )
{
    while( xSemaphoreTake( *pxMutex, portMAX_DELAY ) != pdPASS );
}

void sys_mutex_unlock(sys_mutex_t *pxMutex )
{
    xSemaphoreGive( *pxMutex );
}

void sys_sem_signal( sys_sem_t *pxSemaphore )
{
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

    if( xInsideISR != pdFALSE )
    {
        xSemaphoreGiveFromISR( *pxSemaphore, &xHigherPriorityTaskWoken );
    }
    else
    {
        xSemaphoreGive( *pxSemaphore );
    }
}

u32_t sys_now(void)
{
    return xTaskGetTickCount();
}

sys_prot_t sys_arch_protect( void )
{
    if( xInsideISR == pdFALSE )
    {
        taskENTER_CRITICAL();
    }
    return ( sys_prot_t ) 1;
}

void sys_arch_unprotect( sys_prot_t xValue )
{
    (void) xValue;
    if( xInsideISR == pdFALSE )
    {
        taskEXIT_CRITICAL();
    }
}

void sys_assert( const char *pcMessage )
{
    (void) pcMessage;
    // printf("sys_assert : loop forever\n");
    for (;;)
    {
    }
}