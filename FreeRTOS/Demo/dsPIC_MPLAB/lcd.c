/*
 * FreeRTOS V202112.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://aws.amazon.com/freertos
 *
 */

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Demo includes. */
#include "lcd.h"

/*
 * The LCD is written to by more than one task so is controlled by this
 * 'gatekeeper' task.  This is the only task that is actually permitted to
 * access the LCD directly.  Other tasks wanting to display a message send
 * the message to the gatekeeper.
 */
static void vLCDTask( void *pvParameters );

/*
 * Setup the peripherals required to communicate with the LCD.
 */
static void prvSetupLCD( void );

/*
 * Move to the first (0) or second (1) row of the LCD.
 */
static void prvLCDGotoRow( unsigned short usRow );

/*
 * Write a string of text to the LCD.
 */
static void prvLCDPutString( char *pcString );

/*
 * Clear the LCD.
 */
static void prvLCDClear( void );

/*-----------------------------------------------------------*/

/* Brief delay to permit the LCD to catch up with commands. */
#define lcdVERY_SHORT_DELAY	( 1 )
#define lcdSHORT_DELAY		( 4 / portTICK_PERIOD_MS )
#define lcdLONG_DELAY		( 15 / portTICK_PERIOD_MS )

/* LCD commands. */
#define lcdCLEAR			( 0x01 )
#define lcdHOME				( 0x02 )
#define lcdLINE2			( 0xc0 )

/* SFR that seems to be missing from the standard header files. */
#define PMAEN				*( ( unsigned short * ) 0x60c )

/* LCD R/W signal. */
#define  lcdRW  LATDbits.LATD5

/* LCD lcdRS signal. */
#define  lcdRS  LATBbits.LATB15

/* LCD lcdE signal . */
#define  lcdE   LATDbits.LATD4

/* Control signal pin direction. */
#define  RW_TRIS	TRISDbits.TRISD5
#define  RS_TRIS	TRISBbits.TRISB15
#define  E_TRIS		TRISDbits.TRISD4

/* Port for LCD data */
#define  lcdDATA      LATE
#define  lcdDATAPORT  PORTE

/* I/O setup for data Port. */
#define  TRISDATA  TRISE

/* The length of the queue used to send messages to the LCD gatekeeper task. */
#define lcdQUEUE_SIZE		3
/*-----------------------------------------------------------*/

/* The queue used to send messages to the LCD task. */
QueueHandle_t xLCDQueue;

static void prvLCDCommand( char cCommand );
static void prvLCDData( char cChar );

/*-----------------------------------------------------------*/

QueueHandle_t xStartLCDTask( void )
{
	/* Create the queue used by the LCD task.  Messages for display on the LCD
	are received via this queue. */
	xLCDQueue = xQueueCreate( lcdQUEUE_SIZE, sizeof( xLCDMessage ) );

	/* Start the task that will write to the LCD.  The LCD hardware is
	initialised from within the task itself so delays can be used. */
	xTaskCreate( vLCDTask, "LCD", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL );

	return xLCDQueue;
}
/*-----------------------------------------------------------*/

static void prvLCDGotoRow( unsigned short usRow )
{
	if( usRow == 0 )
	{
		prvLCDCommand( lcdHOME );
	}
	else
	{
		prvLCDCommand( lcdLINE2 );
	}
}
/*-----------------------------------------------------------*/

static void prvLCDCommand( char cCommand )
{
	/* Prepare RD0 - RD7. */
	lcdDATA &= 0xFF00;

	/* Command byte to lcd. */
    lcdDATA |= cCommand;

	/* Ensure lcdRW is 0. */
	lcdRW = 0;
    lcdRS = 0;

	/* Toggle lcdE line. */
    lcdE = 1;
    vTaskDelay( lcdVERY_SHORT_DELAY );
    lcdE = 0;

   	vTaskDelay( lcdSHORT_DELAY );
}
/*-----------------------------------------------------------*/

static void prvLCDData( char cChar )
{
	/* ensure lcdRW is 0. */
	lcdRW = 0;

	/* Assert register select to 1. */
    lcdRS = 1;

	/* Prepare RD0 - RD7. */
	lcdDATA &= 0xFF00;

	/* Data byte to lcd. */
    lcdDATA |= cChar;
    lcdE = 1;
 	Nop();
    Nop();
    Nop();

	/* Toggle lcdE signal. */
    lcdE = 0;

	/* Negate register select to 0. */
    lcdRS = 0;

	vTaskDelay( lcdVERY_SHORT_DELAY );
}
/*-----------------------------------------------------------*/

static void prvLCDPutString( char *pcString )
{
	/* Write out each character with appropriate delay between each. */
	while( *pcString )
	{
		prvLCDData( *pcString );
		pcString++;
		vTaskDelay( lcdSHORT_DELAY );
	}
}
/*-----------------------------------------------------------*/

static void prvLCDClear( void )
{
	prvLCDCommand( lcdCLEAR );
}
/*-----------------------------------------------------------*/

static void prvSetupLCD( void )
{
	/* Wait for proper power up. */
	vTaskDelay( lcdLONG_DELAY );

	/* Set initial states for the data and control pins */
	LATE &= 0xFF00;

	/* R/W state set low. */
    lcdRW = 0;

	/* lcdRS state set low. */
	lcdRS = 0;

	/* lcdE state set low. */
	lcdE = 0;

	/* Set data and control pins to outputs */
	TRISE &= 0xFF00;

	/* lcdRW pin set as output. */
 	RW_TRIS = 0;

	/* lcdRS pin set as output. */
	RS_TRIS = 0;

	/* lcdE pin set as output. */
	E_TRIS = 0;

	/* 1st LCD initialization sequence */
	lcdDATA &= 0xFF00;
    lcdDATA |= 0x0038;
    lcdE = 1;
    Nop();
    Nop();
    Nop();

	/* Toggle lcdE signal. */
    lcdE = 0;

	vTaskDelay( lcdSHORT_DELAY );
	vTaskDelay( lcdSHORT_DELAY );
	vTaskDelay( lcdSHORT_DELAY );

	/* 2nd LCD initialization sequence */
	lcdDATA &= 0xFF00;
    lcdDATA |= 0x0038;
    lcdE = 1;
    Nop();
    Nop();
    Nop();

	/* Toggle lcdE signal. */
    lcdE = 0;

    vTaskDelay( lcdSHORT_DELAY );

	/* 3rd LCD initialization sequence */
	lcdDATA &= 0xFF00;
    lcdDATA |= 0x0038;
    lcdE = 1;
    Nop();
    Nop();
    Nop();

	/* Toggle lcdE signal. */
    lcdE = 0;

	vTaskDelay( lcdSHORT_DELAY );


	/* Function set. */
    prvLCDCommand( 0x38 );

	/* Display on/off control, cursor blink off (0x0C). */
    prvLCDCommand( 0x0C );

	/* Entry mode set (0x06). */
    prvLCDCommand( 0x06 );

	prvLCDCommand( lcdCLEAR );
}
/*-----------------------------------------------------------*/

static void vLCDTask( void *pvParameters )
{
xLCDMessage xMessage;
unsigned short usRow = 0;

	/* Initialise the hardware.  This uses delays so must not be called prior
	to the scheduler being started. */
	prvSetupLCD();

	/* Welcome message. */
	prvLCDPutString( "www.FreeRTOS.org" );

	for( ;; )
	{
		/* Wait for a message to arrive that requires displaying. */
		while( xQueueReceive( xLCDQueue, &xMessage, portMAX_DELAY ) != pdPASS );

		/* Clear the current display value. */
		prvLCDClear();

		/* Switch rows each time so we can see that the display is still being
		updated. */
		prvLCDGotoRow( usRow & 0x01 );
		usRow++;
		prvLCDPutString( xMessage.pcMessage );

		/* Delay the requested amount of time to ensure the text just written
		to the LCD is not overwritten. */
		vTaskDelay( xMessage.xMinDisplayTime );
	}
}




