/*******************************************************************************
 System Tasks File

  File Name:
    tasks.c

  Summary:
    This file contains source code necessary to maintain system's polled tasks.

  Description:
    This file contains source code necessary to maintain system's polled tasks.
    It implements the "SYS_Tasks" function that calls the individual "Tasks"
    functions for all polled MPLAB Harmony modules in the system.

  Remarks:
    This file requires access to the systemObjects global data structure that
    contains the object handles to all MPLAB Harmony module objects executing
    polled in the system.  These handles are passed into the individual module
    "Tasks" functions to identify the instance of the module to maintain.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
 *******************************************************************************/
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "configuration.h"
#include "definitions.h"
#include "sys_tasks.h"


// *****************************************************************************
// *****************************************************************************
// Section: RTOS "Tasks" Routine
// *****************************************************************************
// *****************************************************************************

/* -------- constants -------------------------------------------------- */
#define WATCHDOG_PERIOD_MS   500     // 0.5-s edges (well inside 10-s MAX16998 window)

static void lWatchdogTask(void *pvParameters)
{
    bool wdiLevel = false;

    while(true)
    {
        /* 1. clear internal SAME51 watchdog */
        WDT_Clear();

        /* 2. toggle external MAX16998 WDI pin */
        if (wdiLevel)
            WDT_WDI_Clear();
        else
            WDT_WDI_Set();
        wdiLevel = !wdiLevel;

        /* 3. sleep exactly WATCHDOG_PERIOD_MS */
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_PERIOD_MS));
    }
}

static void lDRV_SDSPI_0_Tasks(  void *pvParameters  )
{
    while(true)
    {
        DRV_SDSPI_Tasks(sysObj.drvSDSPI0);
        vTaskDelay(10U / portTICK_PERIOD_MS);
    }
}


static void lSYS_FS_Tasks(  void *pvParameters  )
{
    while(true)
    {
        SYS_FS_Tasks();
        vTaskDelay(10U / portTICK_PERIOD_MS);
    }
}



/* Handle for the APP_Tasks. */
TaskHandle_t xAPP_Tasks;



static void lAPP_Tasks(  void *pvParameters  )
{   
    while(true)
    {
        APP_Tasks();
    }
}




// *****************************************************************************
// *****************************************************************************
// Section: System "Tasks" Routine
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void SYS_Tasks ( void )

  Remarks:
    See prototype in system/common/sys_module.h.
*/
void SYS_Tasks ( void )
{
    /* Maintain system services */
    
    (void) xTaskCreate( lSYS_FS_Tasks,
        "SYS_FS_TASKS",
        SYS_FS_STACK_SIZE,
        (void*)NULL,
        SYS_FS_PRIORITY ,
        (TaskHandle_t*)NULL
    );

    /* Create watchdog task
     * This task maintains two watchdogs:
     * 1. Internal SAME51 watchdog (16s timeout)
     * 
     * The task runs every 500ms to:
     * - Clear the internal watchdog timer
     * - Toggle the WDI pin for the external watchdog
     * 
     * If this task fails to run:
     * - Internal watchdog will reset the MCU after ~16s
     * 
     * Priority is set just above idle to ensure it runs even
     * when the system is idle, but doesn't interfere with
     * higher priority tasks.
     */
    (void) xTaskCreate( lWatchdogTask,
        "WDT",
        configMINIMAL_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 1,
        NULL
    );

    /* Maintain Device Drivers */
    (void) xTaskCreate( lDRV_SDSPI_0_Tasks,
        "DRV_SD_0_TASKS",
        DRV_SDSPI_STACK_SIZE_IDX0,
        (void*)NULL,
        DRV_SDSPI_PRIORITY_IDX0 ,
        (TaskHandle_t*)NULL
    );

    /* Maintain Middleware & Other Libraries */
    

    /* Maintain the application's state machine. */
    
    /* Create OS Thread for APP_Tasks. */
    (void) xTaskCreate(
           (TaskFunction_t) lAPP_Tasks,
           "APP_Tasks",
           1024,
           NULL,
           1U ,
           &xAPP_Tasks);

    /* Start RTOS Scheduler. */
    
     /**********************************************************************
     * Create all Threads for APP Tasks before starting FreeRTOS Scheduler *
     ***********************************************************************/
    vTaskStartScheduler(); /* This function never returns. */
}

/*******************************************************************************
 End of File
 */

