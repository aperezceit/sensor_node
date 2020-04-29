/**
 * @file main_nortos.c
 * @version 1.0
 * @date 07/05/2019
 * @author A.Irizar
 * @tittle main no Rtos
 * @brief Main no Rtos
 */

//*****************************************************************************
//                      INCLUDES
//*****************************************************************************
#include <stdint.h>
#include <stddef.h>
#include <NoRTOS.h>
#include "Board.h"

//*****************************************************************************
//                      GLOBALS
//*****************************************************************************
extern void *mainThread(void *arg0);

//*****************************************************************************
//
//! \brief Main No Rtos Function
//!
//! This function
//!        1. Initiates STARPORTS App
//!
//! \param[in]
//!
//! \return         None
//
//*****************************************************************************
int main(void)
{
    Board_init();                       // Call driver init functions
    NoRTOS_start();                     // Start NoRTOS
    mainThread(NULL);                   // Call mainThread function

    while (1) {;}

}
