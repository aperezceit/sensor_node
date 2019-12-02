/*
 * Copyright (c) 2015-2017, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== uartecho.c ========
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <STARPORTS_App.h>
#include <unistd.h>

/* Driver Header files */
#include <ti/drivers/UART.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/Timer.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/Watchdog.h>
#include <ti/drivers/net/wifi/wlan.h>
#include <ti/drivers/net/wifi/simplelink.h>

#include "platform.h"

#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>

#include "hal_UART.h"
#include "hal_Timer.h"
#include "hal_GPIO.h"
#include "hal_WD.h"
#include "hal_I2C.h"
#include "hal_SPI.h"
#include "wifi.h"
#include "file_system.h"

#include "DS1374.h"

#include "Sensors.h"
#include "hal_LORA.h"

/* Example/Board Header files */
#include "Board.h"


#define TIMEOUT_MS 10000 // 10 seconds Timeout

#define MYRTC 1

uint8_t Timer0Event = 0;
uint8_t Timer1Event = 0;
uint8_t LPDSOut = 0;

struct Node MyNode = {NODEID,1200,MODE_NORMAL_LORA,16,"PORTS0"};
struct TMP006_Data MyTMP006 = {TMP006_ID};
struct ADXL355_Data MyADXL = {ADXL355_ID,500,256};
struct BME280_Data MyBME = {BME280_ID};
struct LDC1000_Data MyLDC = {LDC1000_ID,16};
struct Vbat_Data MyVbat = {VBAT_ID,16};
/*
 *  ======== mainThread ========
 */

void WakeupFromLPDS(uint_least8_t);

UART_Handle uart0;  // uart0 is now global to access the handle in UART_PRINT kind debug messages
                    // Can be removed in final deployment
I2C_Handle i2c;

SPI_Handle spi;

Watchdog_Handle wd; // Watchdog Handle

void *mainThread(void *arg0)
{

    uint8_t FirstOTAA=1;
    int upctr;

    /* Timer Handle */
    Timer_Handle timer0;

    /* UART Handles */
    UART_Handle uart1;

    /* RTC */

    unsigned char Mess[32];

    struct LoraNode MyLoraNode;

    unsigned char Command[128];
    unsigned char buf[128];
    uint8_t ret;
    uint8_t sz;
    uint8_t NTxFail;

    uint8_t macAddress[SL_MAC_ADDR_LEN];
    uint8_t macAddressLen = SL_MAC_ADDR_LEN;
    unsigned char hexstr[16];
    uint8_t len;


    /* wifi var */
    int16_t         sockId;
    int16_t         status = -1;

    /* Open a Watchdog driver instance */
    // wd = Startup_Watchdog(Board_WATCHDOG0, TIMEOUT_MS);

    // Power_enablePolicy();

    /* Get Param Values from internal filesystem */
    // Get MyNode.WakeUpInterval --> DS1374_Write_WdAlmb(i2c, MyNode.WakeUpInterval);
    // Get MyNode.Mode --> Determine Wireless Interface
    // Get MyNode.NCycles --> Get Value only if Mode is Storage
    // Get MyNode.SSID[] --> Get Value only if Mode is WiFi


    /* GPIO */
    // GPIOs Configured and set as outputs in Low state
    // GPIO_setConfig(Board_TIME_MEAS, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    // GPIO_write(Board_TIME_MEAS,1);
    ret = GPIO_Config();

    Node_Enable();

    RN2483_Clear();
    usleep(10000);

    /* UART0 is for sending debug information */
    uart0 = Startup_UART(Board_UART0, 115200);
    /* UART1 connects to RN2483 */
    uart1 = Startup_UART(Board_UART1, 57600);

    /* FYLE SYSTEM*/
    //Muestra estado memoria y lista de archivos
    SPI_init(); //necesario inicializar SPI antes de sl_Start
    sl_Start(0, 0, 0); // necesario arrancar device antes de trabajar con file system
    // st_showStorageInfo(); //no es necesario ver la info, se puede omitir
    // st_listFiles(1);
    // newFile();

    // ret = sl_NetCfgGet(SL_NETCFG_MAC_ADDRESS_GET,NULL,&macAddressLen,&macAddress[0]);
    // len = Uint8Array2Char(macAddress, macAddressLen, hexstr);

    UART_write(uart0, "\r\nInitiating CheckDataRateLora...\r\n", 35);

    RN2483_Set();
    // Recoge texto de inicializaci�n del RN2483
    sz = GetLine_UART(uart1, buf);
    UART_PRINT(buf);
    if (sz==0) {
        // Buffer size is too large
    }


    // Select DataRate
    uint8_t run=1;
    while (run) {
        memset(&buf,0, sizeof(buf));
        UART_write(uart0,"Select Data Rate (0-5): ",24);
        sz = GetLine_UART(uart0, buf);
        if (buf[0] >= '0' && buf[0]<='5') {
            sprintf(Command,"mac set dr %c\r\n",buf[0]);
            UART_write(uart1, (const char *)(Command), strlen(Command));
            sz = GetLine_UART(uart1, buf);
            if (strncmp(buf,"ok",2)==0) {
                UART_write(uart0,"mac set dr successful\r\n",23);
                // Join ABP
                ret = Join_Abp_Lora(uart1);
                if (ret==SUCCESS_ABP_LORA) {
                    // Send Data
                    strcpy(Command,"mac tx uncnf 1 BEBE\r\n");
                    UART_write(uart1, (const char *)(Command), strlen(Command));
                    sz = GetLine_UART(uart1, buf);
                    if (strncmp(buf,"ok",2)==0) {
                        sz = GetLine_UART(uart1, buf);
                        if (strncmp(buf,"mac_tx_ok",9)==0) {
                            UART_write(uart0,"Data successfully sent.\r\n",25);
                            UART_write(uart0,"Date Reached the Server?: (Y/N): ",33);
                            sz = GetLine_UART(uart0, buf);
                            if (buf[0] == 'Y' || buf[0]=='y') {
                                run = 0;
                                ret = Mac_Save(uart1);
                                if (ret==0) {
                                    UART_write(uart0,"mac save successful\r\n",21);
                                }
                            } else {
                                UART_write(uart0,"Try another data rate.\r\n",24);
                            }
                        }
                    }
                }
            }
        } else {
            UART_write(uart0,"Wrong DataRate. Try Again. \r\n",29);
        }
    }


//    // Join OTAA??
//    memset(&buf,0, sizeof(buf));
//    UART_write(uart0,"Join OTAA? (Y/N): ",18);
//    sz = GetLine_UART(uart0, buf);
//
//
//    if (buf[0]=='Y' || buf[0]=='y') {
//        ret = Join_Otaa_Lora(uart1);
//        if (ret==SUCCESS_OTAA_LORA) {
//            strcpy(Mess,"Join_Otaa_Lora() Success\r\n");
//            UART_write(uart0, Mess, strlen(Mess));
//
//            /* Enter DevAddr */
//            memset(&buf,0, sizeof(buf));
//            UART_write(uart0,"Enter DevAddr: ",15);
//            sz = GetLine_UART(uart0, buf);
//            strncpy(MyLoraNode.DevAddr, buf,8);
//
//            UART_write(uart0, "DevAddr entered is ", 19);
//            UART_write(uart0, &MyLoraNode.DevAddr, 8);
//            UART_write(uart0,"\r\n",2);
//
//            memset(&buf,0, sizeof(buf));
//            strcpy(Command,"mac set devaddr ");
//            strncat(Command, &MyLoraNode.DevAddr,8);
//            strcat(Command, "\r\n");
//            UART_write(uart1, (const char *)Command, strlen(Command));
//            UART_PRINT(Command);
//            sz = GetLine_UART(uart1, buf);
//            if (strncmp(buf,"ok",2)==0) {
//                UART_write(uart0,"mac set devaddr successful\r\n",28);
//            }
//
//            /* Enter AppsKey */
//            memset(&buf,0, sizeof(buf));
//            UART_write(uart0,"Enter AppsKey: ",15);
//            sz = GetLine_UART(uart0, buf);
//            strncpy(MyLoraNode.AppsKey, buf,32);
//
//            UART_write(uart0, "AppsKey entered is ", 19);
//            UART_write(uart0, &MyLoraNode.AppsKey, 32);
//            UART_write(uart0,"\r\n",2);
//
//            memset(&buf,0, sizeof(buf));
//            strcpy(Command,"mac set appskey ");
//            strncat(Command, &MyLoraNode.AppsKey,32);
//            strcat(Command, "\r\n");
//            UART_write(uart1, (const char *)Command, strlen(Command));
//            UART_PRINT(Command);
//            sz = GetLine_UART(uart1, buf);
//            if (strncmp(buf,"ok",2)==0) {
//                UART_write(uart0,"mac set appskey successful\r\n",28);
//            }
//
//            /* Enter NwksKey */
//            memset(&buf,0, sizeof(buf));
//            UART_write(uart0,"Enter NwksKey: ",15);
//            sz = GetLine_UART(uart0, buf);
//            strncpy(MyLoraNode.NwksKey, buf,32);
//
//            UART_write(uart0, "NwksKey entered is ", 19);
//            UART_write(uart0, &MyLoraNode.NwksKey, 32);
//            UART_write(uart0,"\r\n",2);
//
//            memset(&buf,0, sizeof(buf));
//            strcpy(Command,"mac set nwkskey ");
//            strncat(Command, &MyLoraNode.NwksKey,32);
//            strcat(Command, "\r\n");
//            UART_write(uart1, (const char *)Command, strlen(Command));
//            UART_PRINT(Command);
//            sz = GetLine_UART(uart1, buf);
//            if (strncmp(buf,"ok",2)==0) {
//                UART_write(uart0,"mac set nwkskey successful\r\n",28);
//            }
//
//        } else {
//            strcpy(Mess,"Join_Otaa_Lora() Failed\r\n");
//            UART_write(uart0, Mess, strlen(Mess));  //cambiado a uart0
//            Mess[0] = '('; Mess[1] = ret+48; Mess[2]=')';
//            UART_write(uart0, Mess,3);  //cambiado a uart0
//            UART_write(uart0, "\r\n",2);    //cambiado a uart0
//        }
//    }
//
//    /* Save the results to RN2384 EEPROM */
//    ret = Mac_Save(uart1);
//    if (ret==0) {
//        UART_write(uart0,"mac save successful\r\n",21);
//    }

}


void WakeupFromLPDS(uint_least8_t var) {
    LPDSOut = 1;
}

