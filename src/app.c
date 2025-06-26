/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
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

#include "app.h"
#include "definitions.h"
#include "ff.h"
#include "pdu_packet.h"
#include <stdio.h>

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

#define SDCARD_MOUNT_NAME "/mnt/myDrive1"
#define SDCARD_DEV_NAME "/dev/mmcblka1"
#define SDCARD_FILE_NAME "hello.txt"
#define SDCARD_DIR_NAME "Dir1"

#define APP_DATA_LEN 512

char APP_INIT_MSG[] = "\r\nAPP INITIALIZED\r\n";
char SD_MOUNT_FAIL[] = "SD MOUNT FAIL\r\n";
char SD_SUCCESS_MSG[] = "SD SUCCESS\r\n";
char SD_FAILED_MSG[] = "SD FAIL\r\n";

// USART Definitions
// *****************************************************************************
#define RX_BUFFER_SIZE 512  // Increased buffer size for better reliability
#define TX_BUFFER_SIZE 256

// Circular buffer structure for USART
typedef struct {
    uint8_t buffer[RX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
    volatile bool overflow;
} usart_rx_buffer_t;

typedef struct {
    uint8_t buffer[TX_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} usart_tx_buffer_t;

char newline[] = "\r\n";
char errorMessage[] = "\r\n**** USART error has occurred ****\r\n";
char overflowMessage[] = "\r\n**** USART buffer overflow ****\r\n";
char bufferResetMessage[] = "\r\n**** USART buffer reset ****\r\n";

// Global buffer instances
static usart_rx_buffer_t rxBuffer = {0};
static usart_tx_buffer_t txBuffer = {0};

// Command processing buffer
char commandBuffer[256];
uint16_t commandLength = 0;
// *****************************************************************************

void read_CMD(char *cmd);
void delay_ms(int delay);
void USART_READ(void);
void I2C_READ(void);
void enableGPIOs(void);
void disableGPIOs(void);
void FATFS_APP(void);

// USART Buffer Management Functions
void usart_rx_buffer_init(void);
bool usart_rx_buffer_put(uint8_t data);
bool usart_rx_buffer_get(uint8_t *data);
uint16_t usart_rx_buffer_available(void);
void usart_rx_buffer_reset(void);
void usart_tx_buffer_init(void);
bool usart_tx_buffer_put(uint8_t data);
bool usart_tx_buffer_get(uint8_t *data);
uint16_t usart_tx_buffer_available(void);
void usart_process_command(void);
void usart_handle_error(USART_ERROR error);
void usart_safe_write(const char* message, size_t length);
void usart_print_buffer_status(void);

FATFS FatFs; /* FatFs work area needed for each volume */
FIL Fil;     /* File object needed for each open file */

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void)
{
    //    SERCOM3_USART_Write(&APP_INIT_MSG[0], sizeof(APP_INIT_MSG));
    disableGPIOs();
    RTC_Initialize();
    RTC_Timer32Start();
    SERCOM4_I2C_Initialize();
    // SERCOM2_SPI_Initialize();
    
    // Initialize USART buffers
    usart_rx_buffer_init();
    usart_tx_buffer_init();
    
    // set the LED solid color once initialized
    LED_Set();
    // set the LED solid color once initialized
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
    //    I2C_READ();
    // FATFS_APP();
}

UINT bw;
void FATFS_APP(void)
{
    if (f_mount(&FatFs, "", 1) == FR_OK)
    { /* Mount SD */

        if (f_open(&Fil, "hello.txt", FA_OPEN_ALWAYS | FA_READ | FA_WRITE) == FR_OK)
        { /* Open or create a file */

            if ((Fil.fptr != 0) && (f_lseek(&Fil, Fil.fptr) != FR_OK))
                goto endSD; /* Jump to the end of the file */

            SERCOM3_USART_Write(&SD_SUCCESS_MSG[0], sizeof(SD_SUCCESS_MSG));
            f_write(&Fil, "Hello world!\r\n", 14, &bw); /* Write data to the file */

        endSD:
            f_close(&Fil); /* Close the file */
        }
        else
        {
            SERCOM3_USART_Write(&SD_FAILED_MSG[0], sizeof(SD_FAILED_MSG));
        }
    }
    else
    {
        SERCOM3_USART_Write(&SD_MOUNT_FAIL[0], sizeof(SD_MOUNT_FAIL));
    }
}

void USART_READ(void)
{
    uint8_t data;
    USART_ERROR error;
    
    /* Check if there is a received character */
    if (SERCOM3_USART_ReceiverIsReady() == true)
    {
        /* Check for USART errors first */
        error = SERCOM3_USART_ErrorGet();
        if (error != USART_ERROR_NONE)
        {
            usart_handle_error(error);
            return;
        }
        
        /* Read the data */
        SERCOM3_USART_Read(&data, 1);
        
        /* Add to circular buffer */
        if (!usart_rx_buffer_put(data))
        {
            /* Buffer overflow occurred */
            usart_safe_write(overflowMessage, sizeof(overflowMessage) - 1);
            usart_rx_buffer_reset();
        }
    }
    
    /* Process any complete commands in the buffer */
    usart_process_command();
}

void I2C_READ(void)
{
    uint8_t i2c_char = SERCOM4_I2C_ReadByte();
    SERCOM3_USART_WriteByte(i2c_char);
    // SERCOM4_I2C_WriteByte(i2c_char);
}

void enableGPIOs(void)
{
    BURN_5V_Set();
    BURN2_EN_Set();
    SW_12V_EN1_Set();
    SW_3V3_EN1_Set();
    SW_3V3_EN2_Set();
    SW_5V_EN1_Set();
    SW_5V_EN2_Set();
    SW_5V_EN3_Set();
    SW_5V_EN4_Set();
    SW_VBATT_EN_Set();
    //    WDT_WDI_Set();
    BURN1_EN_Set();
    IN1_Set();
    IN2_Set();
    IN3_Set();
    IN4_Set();
    IN5_Set();
    IN6_Set();
    IN7_Set();
    IN8_Set();
    TRQ1_Set();
    TRQ2_Set();
    FAULT1_Set();
    FAULT2_Set();
    SLEEP1_Set();
    SLEEP2_Set();
}

void disableGPIOs(void)
{
    BURN_5V_Clear();
    BURN2_EN_Clear();
    SW_12V_EN1_Clear();
    SW_3V3_EN2_Clear();
    SW_3V3_EN1_Clear();
    SW_5V_EN2_Clear();
    SW_5V_EN1_Clear();
    SW_5V_EN3_Clear();
    SW_5V_EN4_Clear();
    SW_VBATT_EN_Clear();
    //    WDT_WDI_Clear();
    BURN1_EN_Set();
    IN1_Clear();
    IN2_Clear();
    IN3_Clear();
    IN4_Clear();
    IN5_Clear();
    IN6_Clear();
    IN7_Clear();
    IN8_Clear();
    TRQ1_Clear();
    TRQ2_Clear();
    FAULT1_Clear();
    FAULT2_Clear();
    SLEEP1_Clear();
    SLEEP2_Clear();
}

void read_CMD(char *cmd)
{
    if (strstr(cmd, "CMD: GPIO ON ALL"))
        enableGPIOs();
    else if (strstr(cmd, "CMD: GPIO OFF ALL"))
        disableGPIOs();
    else if (strstr(cmd, "CMD: SW_3V3_1 ENABLE"))
        SW_3V3_EN1_Set();
    else if (strstr(cmd, "CMD: SW_3V3_1 DISABLE"))
        SW_3V3_EN1_Clear();
    else if (strstr(cmd, "CMD: SW_3V3_2 ENABLE"))
        SW_3V3_EN2_Set();
    else if (strstr(cmd, "CMD: SW_3V3_2 DISABLE"))
        SW_3V3_EN2_Clear();
    else if (strstr(cmd, "CMD: SW_5V_1 ENABLE"))
        SW_5V_EN1_Set();
    else if (strstr(cmd, "CMD: SW_5V_1 DISABLE"))
        SW_5V_EN1_Clear();
    else if (strstr(cmd, "CMD: SW_5V_2 ENABLE"))
        SW_5V_EN2_Set();
    else if (strstr(cmd, "CMD: SW_5V_2 DISABLE"))
        SW_5V_EN2_Clear();
    else if (strstr(cmd, "CMD: SW_5V_3 ENABLE"))
        SW_5V_EN3_Set();
    else if (strstr(cmd, "CMD: SW_5V_3 DISABLE"))
        SW_5V_EN3_Clear();
    else if (strstr(cmd, "CMD: SW_5V_4 ENABLE"))
        SW_5V_EN4_Set();
    else if (strstr(cmd, "CMD: SW_5V_4 DISABLE"))
        SW_5V_EN4_Clear();
    else if (strstr(cmd, "CMD: SW_12V ENABLE"))
    {
        SW_12V_EN1_Set();
        SW_5V_EN4_Set();
    }
    else if (strstr(cmd, "CMD: SW_12V DISABLE"))
    {
        SW_12V_EN1_Clear();
        SW_5V_EN4_Clear();
    }
    else if (strstr(cmd, "CMD: VBATT ENABLE"))
        SW_VBATT_EN_Set();
    else if (strstr(cmd, "CMD: VBATT DISABLE"))
        SW_VBATT_EN_Clear();
    else if (strstr(cmd, "CMD: WDT ENABLE"))
        WDT_WDI_Set();
    else if (strstr(cmd, "CMD: WDT DISABLE"))
        WDT_WDI_Clear();
    else if (strstr(cmd, "CMD: BURN ENABLE"))
    {
        BURN1_EN_Set();
        BURN2_EN_Set();
        BURN_5V_Set();
    }
    else if (strstr(cmd, "CMD: BURN DISABLE"))
    {
        BURN1_EN_Clear();
        BURN2_EN_Clear();
        BURN_5V_Clear();
    }
    else if (strstr(cmd, "CMD: BURN1 ENABLE"))
    {
        BURN1_EN_Set();
        BURN_5V_Set();
    }
    else if (strstr(cmd, "CMD: BURN1 DISABLE"))
    {
        BURN1_EN_Clear();
    }
    else if (strstr(cmd, "CMD: BURN2 ENABLE"))
    {
        BURN2_EN_Set();
        BURN_5V_Set();
    }
    else if (strstr(cmd, "CMD: BURN2 DISABLE"))
    {
        BURN2_EN_Clear();
    }
    else if (strstr(cmd, "CMD: HBRIDGE1 ENABLE"))
    {
        FAULT1_Set();
        IN1_Set();
        IN2_Set();
        IN3_Set();
        IN4_Set();
        TRQ1_Set();
        SLEEP1_Set();
    }
    else if (strstr(cmd, "CMD: HBRIDGE1 DISABLE"))
    {
        FAULT1_Clear();
        IN1_Clear();
        IN2_Clear();
        IN3_Clear();
        IN4_Clear();
        TRQ1_Clear();
        SLEEP1_Clear();
    }
    else if (strstr(cmd, "CMD: HBRIDGE2 ENABLE"))
    {
        FAULT2_Set();
        IN5_Set();
        IN6_Set();
        IN7_Set();
        IN8_Set();
        TRQ2_Set();
        SLEEP2_Set();
    }
    else if (strstr(cmd, "CMD: HBRIDGE2 DISABLE"))
    {
        FAULT2_Clear();
        IN5_Clear();
        IN6_Clear();
        IN7_Clear();
        IN8_Clear();
        TRQ2_Clear();
        SLEEP2_Clear();
    }
    else if (strstr(cmd, "CMD: FATFS"))
        FATFS_APP();
    else if (strstr(cmd, "CMD: BUFFER STATUS"))
        usart_print_buffer_status();
}

void delay_ms(int delay)
{
    for (uint8_t i = 0; i < delay; i++)
        asm("NOP");
}

// *****************************************************************************
// USART Buffer Management Functions
// *****************************************************************************

void usart_rx_buffer_init(void)
{
    rxBuffer.head = 0;
    rxBuffer.tail = 0;
    rxBuffer.count = 0;
    rxBuffer.overflow = false;
}

bool usart_rx_buffer_put(uint8_t data)
{
    if (rxBuffer.count >= RX_BUFFER_SIZE)
    {
        rxBuffer.overflow = true;
        return false;
    }
    
    rxBuffer.buffer[rxBuffer.head] = data;
    rxBuffer.head = (rxBuffer.head + 1) % RX_BUFFER_SIZE;
    rxBuffer.count++;
    return true;
}

bool usart_rx_buffer_get(uint8_t *data)
{
    if (rxBuffer.count == 0)
    {
        return false;
    }
    
    *data = rxBuffer.buffer[rxBuffer.tail];
    rxBuffer.tail = (rxBuffer.tail + 1) % RX_BUFFER_SIZE;
    rxBuffer.count--;
    return true;
}

uint16_t usart_rx_buffer_available(void)
{
    return rxBuffer.count;
}

void usart_rx_buffer_reset(void)
{
    rxBuffer.head = 0;
    rxBuffer.tail = 0;
    rxBuffer.count = 0;
    rxBuffer.overflow = false;
    commandLength = 0;
    usart_safe_write(bufferResetMessage, sizeof(bufferResetMessage) - 1);
}

void usart_tx_buffer_init(void)
{
    txBuffer.head = 0;
    txBuffer.tail = 0;
    txBuffer.count = 0;
}

bool usart_tx_buffer_put(uint8_t data)
{
    if (txBuffer.count >= TX_BUFFER_SIZE)
    {
        return false;
    }
    
    txBuffer.buffer[txBuffer.head] = data;
    txBuffer.head = (txBuffer.head + 1) % TX_BUFFER_SIZE;
    txBuffer.count++;
    return true;
}

bool usart_tx_buffer_get(uint8_t *data)
{
    if (txBuffer.count == 0)
    {
        return false;
    }
    
    *data = txBuffer.buffer[txBuffer.tail];
    txBuffer.tail = (txBuffer.tail + 1) % TX_BUFFER_SIZE;
    txBuffer.count--;
    return true;
}

uint16_t usart_tx_buffer_available(void)
{
    return txBuffer.count;
}

void usart_process_command(void)
{
    uint8_t data;
    
    /* Process all available data in the buffer */
    while (usart_rx_buffer_get(&data))
    {
        /* Check for end of command */
        if (data == '\r' || data == '\n')
        {
            if (commandLength > 0)
            {
                /* Null terminate the command */
                commandBuffer[commandLength] = '\0';
                
                /* Process the command */
                decode_pdu_packet(commandBuffer);
                // read_CMD(commandBuffer);  // Uncomment if you want to use the old command system
                
                /* Reset command buffer */
                commandLength = 0;
            }
        }
        else if (commandLength < sizeof(commandBuffer) - 1)
        {
            /* Add character to command buffer */
            commandBuffer[commandLength++] = data;
        }
        else
        {
            /* Command buffer overflow - reset */
            commandLength = 0;
            usart_safe_write(overflowMessage, sizeof(overflowMessage) - 1);
        }
    }
}

void usart_handle_error(USART_ERROR error)
{
    /* Clear the error */
    SERCOM3_USART_ErrorGet(); // This will clear the error flags
    
    /* Send error message */
    usart_safe_write(errorMessage, sizeof(errorMessage) - 1);
    
    /* Reset buffers on critical errors */
    if (error == USART_ERROR_OVERRUN)
    {
        usart_rx_buffer_reset();
    }
    
    /* Add specific error handling if needed */
    switch (error)
    {
        case USART_ERROR_OVERRUN:
            usart_safe_write("Buffer Overflow Error\r\n", 23);
            break;
        case USART_ERROR_FRAMING:
            usart_safe_write("Framing Error\r\n", 15);
            break;
        case USART_ERROR_PARITY:
            usart_safe_write("Parity Error\r\n", 14);
            break;
        default:
            usart_safe_write("Unknown Error\r\n", 15);
            break;
    }
}

void usart_safe_write(const char* message, size_t length)
{
    /* Check if transmitter is ready before writing */
    if (SERCOM3_USART_TransmitterIsReady())
    {
        SERCOM3_USART_Write((void*)message, length);
    }
}

void usart_print_buffer_status(void)
{
    char status_msg[64];
    int len = snprintf(status_msg, sizeof(status_msg), 
                      "RX Buffer: %d/%d, TX Buffer: %d/%d\r\n",
                      usart_rx_buffer_available(), RX_BUFFER_SIZE,
                      usart_tx_buffer_available(), TX_BUFFER_SIZE);
    if (len > 0 && len < sizeof(status_msg))
    {
        usart_safe_write(status_msg, len);
    }
}

/*******************************************************************************
 End of File
 */