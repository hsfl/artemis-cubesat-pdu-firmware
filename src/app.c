/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the active PDU application loop.

  Description:
    This file applies startup defaults, sets the status LED, and polls UART
    bytes for the framed PDU protocol.
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

#include <stdint.h>

#include "app.h"
#include "definitions.h"
#include "pdu_packet.h"

#define APP_UART_RX_BYTES_PER_TICK 32U

static void USART_READ(void);

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void)
{
    disableAllGPIOs();
    LED_Set();
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void)
{
    USART_READ();
    pdu_protocol_service_timers();
}

static void USART_READ(void)
{
    uint8_t rxByte = 0U;
    uint8_t bytesRead = 0U;

    /*
     * Drain a bounded number of bytes per task tick so normal UART bursts do
     * not fall behind the 1 ms app-task cadence.
     */
    while ((bytesRead < APP_UART_RX_BYTES_PER_TICK) &&
           (SERCOM3_USART_ReceiverIsReady() == true))
    {
        if (SERCOM3_USART_ErrorGet() == USART_ERROR_NONE)
        {
            if (SERCOM3_USART_Read(&rxByte, 1U))
            {
                pdu_protocol_process_byte(rxByte);
            }
            else
            {
                pdu_protocol_reset_parser();
                return;
            }
        }
        else
        {
            /*
             * SERCOM3_USART_ErrorGet() clears the hardware error flags in the
             * generated PLIB. Reset parser state too so a corrupted byte cannot
             * poison the next valid frame.
             */
            pdu_protocol_reset_parser();
            return;
        }

        bytesRead++;
    }
}

/*******************************************************************************
 End of File
 */
