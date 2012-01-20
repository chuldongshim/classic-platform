/* -------------------------------- Arctic Core ------------------------------
 * Arctic Core - the open source AUTOSAR platform http://arccore.com
 *
 * Copyright (C) 2009  ArcCore AB <contact@arccore.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 * -------------------------------- Arctic Core ------------------------------*/

#include "Lin.h"
#include "LinIf_Cbk.h"
#include "mpc55xx.h"
#include "Det.h"
#include "Mcu.h"
#include <stdlib.h>
#include <string.h>
#include "LinIf_Cbk.h"
#include "Os.h"
#include "isr.h"
#include "irq.h"
#include "arc.h"

#define LIN_MAX_MSG_LENGTH 8

#define LINFLEX(exp) (volatile struct LINFLEX_tag *)(0xFFE40000 + (0x4000 * exp))

/* LIN145: Reset -> LIN_UNINIT: After reset, the Lin module shall set its state to LIN_UNINIT. */
static Lin_DriverStatusType LinDriverStatus = LIN_UNINIT;

static Lin_StatusType LinChannelStatus[LIN_CONTROLLER_CNT];
static Lin_StatusType LinChannelOrderedStatus[LIN_CONTROLLER_CNT];

/* static buffers, holds one frame at a time */
static uint8 LinBufTx[LIN_CONTROLLER_CNT][LIN_MAX_MSG_LENGTH];
static uint8 LinBufRx[LIN_CONTROLLER_CNT][LIN_MAX_MSG_LENGTH];

typedef volatile union {
    vuint16_t R;
    struct {
        vuint16_t LINS:4;
          vuint16_t:2;
        vuint16_t RMB:1;
          vuint16_t:1;
        vuint16_t RBSY:1;      // LCH vuint16_t RXBUSY:1;
        vuint16_t RPS:1;       // LCH vuint16_t RDI:1;
        vuint16_t WUF:1;
        vuint16_t DBFF:1;
        vuint16_t DBEF:1;
        vuint16_t DRF:1;
        vuint16_t DTF:1;
        vuint16_t HRF:1;
    } B;
} LinSRtype;   /* LINFLEX LIN Status Register */

typedef volatile union {
    vuint16_t R;
    struct {
        vuint16_t SZF:1;
        vuint16_t OCF:1;
        vuint16_t BEF:1;
        vuint16_t CEF:1;
        vuint16_t SFEF:1;
        vuint16_t SDEF:1;
        vuint16_t IDPEF:1;
        vuint16_t FEF:1;
        vuint16_t BOF:1;
          vuint16_t:6;
        vuint16_t NF:1;
    } B;
} LinESRType;                      /* LINFLEX LIN Error Status Register */


/* Development error macros. */
#if ( LIN_DEV_ERROR_DETECT == STD_ON )
#define VALIDATE(_exp,_api,_err ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_LIN,0,_api,_err); \
          return; \
        }

#define VALIDATE_W_RV(_exp,_api,_err,_rv ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_LIN,0,_api,_err); \
          return (_rv); \
        }
#else
#define VALIDATE(_exp,_api,_err )
#define VALIDATE_W_RV(_exp,_api,_err,_rv )
#endif

static void ResyncDriver(uint8 Channel)
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);

	 /* Disable tx irq */
	 /* Disable Rx Interrupt */
	 /* Disable Rx Interrupt */

	/* Disable transmitter and receiver. */

  	/* Clear flags  */

	/* Prepare module for resynchronization. */
	 /* LIN Resynchronize. First set then cleared. */
	/* Resynchronize module. */
	 /* LIN Resynchronize. First set then cleared. */

	/* Enable transmitter and receiver. */

	/* Clear set flags again */
}


void LinInterruptRx(uint8 Channel)
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);

    if (LinChannelStatus[Channel]==LIN_RX_BUSY) {
		if (1 == LINFLEXHw->LINSR.B.DRF) {
			/* Clear flags */
			LINFLEXHw->LINSR.B.DRF = 1;

			/* receive complete */
			LinChannelStatus[Channel] = LIN_RX_OK;
 		}
	}
    else
    {
    	/* error */
    }
}

void LinInterruptTx(uint8 Channel)
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);

    if (LinChannelStatus[Channel]==LIN_TX_BUSY) {
		if (1 == LINFLEXHw->LINSR.B.DTF) {
			/* Clear flags */
			LINFLEXHw->LINSR.B.DTF = 1;

			/* receive complete */
			LinChannelStatus[Channel] = LIN_TX_OK;
 		}
	}
    else{
    	/* error */
    }

}

void LinInterruptErr(uint8 Channel)
{
    volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);

    /* Error handling */
	static uint32 errCnt=0;
	errCnt++;
	if(LinChannelStatus[Channel]==LIN_RX_BUSY){
		LinChannelStatus[Channel] = LIN_RX_ERROR;
	}else if (LinChannelStatus[Channel]==LIN_TX_BUSY){
		LinChannelStatus[Channel] = LIN_TX_ERROR;
	}

	/* Clear flags  */
	LINFLEXHw->LINESR.R=0xffffffff;
}

static void LinInterruptRxA(){LinInterruptRx(LIN_CTRL_A);}
static void LinInterruptTxA(){LinInterruptTx(LIN_CTRL_A);}
static void LinInterruptErrA(){LinInterruptErr(LIN_CTRL_A);}

static void LinInterruptRxB(){LinInterruptRx(LIN_CTRL_B);}
static void LinInterruptTxB(){LinInterruptTx(LIN_CTRL_B);}
static void LinInterruptErrB(){LinInterruptErr(LIN_CTRL_B);}

void Lin_Init( const Lin_ConfigType* Config )
{
	(void)Config;
	uint8 i;

	VALIDATE( (LinDriverStatus == LIN_UNINIT), LIN_INIT_SERVICE_ID, LIN_E_STATE_TRANSITION );
	/* VALIDATE( (Config!=0), LIN_INIT_SERVICE_ID, LIN_E_INVALID_POINTER ); */

	for (i=0;i<LIN_CONTROLLER_CNT;i++)
	{
		/* LIN171: On entering the state LIN_INIT, the Lin module shall set each channel into
		 * state LIN_CH_UNINIT. */
		LinChannelStatus[i] = LIN_CH_UNINIT;
		LinChannelOrderedStatus[i]=LIN_CH_OPERATIONAL;
	}

	/* LIN146: LIN_UNINIT -> LIN_INIT: The Lin module shall transition from LIN_UNINIT
	 * to LIN_INIT when the function Lin_Init is called. */
	LinDriverStatus = LIN_INIT;
}

void Lin_DeInit()
{
  LinDriverStatus = LIN_UNINIT;
}

void Lin_WakeupValidation( void )
{

}

void Lin_InitChannel(  uint8 Channel,   const Lin_ChannelConfigType* Config )
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);
	enum
	{
	  LIN_PRIO = 3
	};

	VALIDATE( (Config!=0), LIN_INIT_CHANNEL_SERVICE_ID, LIN_E_INVALID_POINTER );
	VALIDATE( (LinDriverStatus != LIN_UNINIT), LIN_INIT_CHANNEL_SERVICE_ID, LIN_E_UNINIT );
	VALIDATE( (Channel < LIN_CONTROLLER_CNT), LIN_INIT_CHANNEL_SERVICE_ID, LIN_E_INVALID_CHANNEL );

	/* Install the interrupt */
	switch(Channel){
	case 0:
		ISR_INSTALL_ISR2("LinIsrRxA", LinInterruptRxA, (IrqType)(LINFLEX_0_RXI),LIN_PRIO, 0);
		ISR_INSTALL_ISR2("LinIsrTxA", LinInterruptTxA, (IrqType)(LINFLEX_0_TXI),LIN_PRIO, 0);
		ISR_INSTALL_ISR2("LinIsrErrA", LinInterruptErrA, (IrqType)(LINFLEX_0_ERR),LIN_PRIO, 0);
		break;
	case 1:
		ISR_INSTALL_ISR2("LinIsrRxB", LinInterruptRxB, (IrqType)(LINFLEX_1_RXI),LIN_PRIO, 0);
		ISR_INSTALL_ISR2("LinIsrTxB", LinInterruptTxB, (IrqType)(LINFLEX_1_TXI),LIN_PRIO, 0);
		ISR_INSTALL_ISR2("LinIsrErrB", LinInterruptErrB, (IrqType)(LINFLEX_1_ERR),LIN_PRIO, 0);
		break;
	default:
		break;
	}

	/* configure and enable channel */
	LINFLEXHw->LINCR1.R = 0; /* Reset all bits */
	LINFLEXHw->LINCR1.B.INIT = 1; /* Go to init mode */
	LINFLEXHw->LINCR1.B.MBL = 3; /* 13 bit synch */
	LINFLEXHw->LINCR1.B.MME = 3; /* Master mode */

	LINFLEXHw->LINIER.R = 0; /* Reset all bits */
	LINFLEXHw->LINIER.B.BEIE = 1; /* Bit error */
	LINFLEXHw->LINIER.B.CEIE = 1; /* Checksum error */
	LINFLEXHw->LINIER.B.HEIE = 1; /* Header error */
	LINFLEXHw->LINIER.B.BOIE = 1; /* Buffer overrun error */
	LINFLEXHw->LINIER.B.FEIE = 1; /* Framing error */

	LINFLEXHw->LINIER.B.DRIE = 1; /* Data Reception Complete */
	LINFLEXHw->LINIER.B.DTIE = 1; /* Data Transmitted */

	/*
	 * Tx/ Rx baud = fperiph_set_1_clk / (16 � LFDIV)
	 *
	 * To program LFDIV = 25.62d,
	 * LINFBRR = 16 � 0.62 = 9.92, nearest real number 10d = 0xA
	 * LINIBRR = mantissa (25.620d) = 25d = 0x19
	 */
	float lfdiv = (float)McuE_GetPeripheralClock( PERIPHERAL_CLOCK_LIN_A) / (float)((16*Config->LinChannelBaudRate));

	LINFLEXHw->LINFBRR.B.DIV_F = (uint8)((lfdiv - (uint32)lfdiv) * 16); /* Fraction bits */
	LINFLEXHw->LINIBRR.B.DIV_M = (uint16)lfdiv; /* Integer bits */

	LINFLEXHw->LINCR2.R = 0; /* Reset all bits */
	LINFLEXHw->LINCR2.B.IOBE = 1; /* Idle on bit error */
	LINFLEXHw->LINCR2.B.IOPE = 1; /* Idle on parity error */

	/* Status regs */
    LINFLEXHw->LINSR.R = 0xffffffff; /* Reset all bits */
    LINFLEXHw->LINESR.R = 0xffffffff; /* Reset all bits */

	LINFLEXHw->LINCR1.B.INIT = 0; /* Go to normal mode */

	LinChannelStatus[Channel]=LIN_CH_OPERATIONAL;
}

void Lin_DeInitChannel( uint8 Channel )
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);
	VALIDATE( (Channel < LIN_CONTROLLER_CNT), LIN_DEINIT_CHANNEL_SERVICE_ID, LIN_E_INVALID_CHANNEL );

	/* LIN178: The function Lin_DeInitChannel shall only be executable when the LIN
	 * channel state-machine is in state LIN_CH_OPERATIONAL. */
	if(LinChannelStatus[Channel] != LIN_CH_UNINIT){
		/* Disable */
        LINFLEXHw->LINCR1.R = 0; /* Reset all bits */
	    LINFLEXHw->LINIER.R = 0; /* Reset all bits */
	    LINFLEXHw->LINSR.R = 0xffffffff; /* Reset all bits */
	    LINFLEXHw->LINESR.R = 0xffffffff; /* Reset all bits */

		LinChannelStatus[Channel]=LIN_CH_UNINIT;
	}
}

Std_ReturnType Lin_SendHeader(  uint8 Channel,  Lin_PduType* PduInfoPtr )
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);
	imask_t state;

	/* LIN021 */
    Irq_Save(state);
	if(LinChannelStatus[Channel] == LIN_TX_BUSY || LinChannelStatus[Channel] == LIN_TX_ERROR ||
	   LinChannelStatus[Channel] == LIN_RX_BUSY || LinChannelStatus[Channel] == LIN_RX_ERROR)
	{
		ResyncDriver(Channel);
		LinChannelStatus[Channel]=LIN_CH_OPERATIONAL;
	}
    Irq_Restore(state);


	VALIDATE_W_RV( (LinDriverStatus != LIN_UNINIT), LIN_SEND_HEADER_SERVICE_ID, LIN_E_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_UNINIT), LIN_SEND_HEADER_SERVICE_ID, LIN_E_CHANNEL_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (Channel < LIN_CONTROLLER_CNT), LIN_SEND_HEADER_SERVICE_ID, LIN_E_INVALID_CHANNEL, E_NOT_OK);
	/* Send header is used to wake the net in this implementation(no actual header is sent */
	/* VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_SLEEP), LIN_SEND_HEADER_SERVICE_ID, LIN_E_STATE_TRANSITION, E_NOT_OK); */
	VALIDATE_W_RV( (PduInfoPtr != NULL), LIN_SEND_HEADER_SERVICE_ID, LIN_E_INVALID_POINTER, E_NOT_OK);

	/* Calculate the time out value for the frame.(10 � NDATA + 45) � 1.4 according to LIN1.3 */
	uint16 timeOutValue = (uint16)( ( ( 10 * PduInfoPtr->DI + 45 ) * 14 ) / 10);

	LINFLEXHw->BIDR.R = 0; /* Clear reg */

	/* Checksum */
	if (PduInfoPtr->Cs != LIN_ENHANCED_CS){ /*Frame identifiers 60 (0x3C) to 61 (0x3D) shall always use classic checksum */
		LINFLEXHw->BIDR.B.CCS = 1;
	}
	/* Length */
	LINFLEXHw->BIDR.B.DFL = PduInfoPtr->DI - 1;

	/* Id */
	LINFLEXHw->BIDR.B.ID = PduInfoPtr->Pid & 0x7f; /* Without parity bit */

	/* Direction */
	if (PduInfoPtr->Drc == LIN_MASTER_RESPONSE)
	{
		LINFLEXHw->BIDR.B.DIR = 1;

		/* write to buffer reg */
		for(int i = 0; i < PduInfoPtr->DI;i++)
		{
			/* convenient with freescale reg file */
			switch(i)
			{
			case 0:
				LINFLEXHw->BDRL.B.DATA0 = PduInfoPtr->SduPtr[0];
				break;
			case 1:
				LINFLEXHw->BDRL.B.DATA1 = PduInfoPtr->SduPtr[1];
				break;
			case 2:
				LINFLEXHw->BDRL.B.DATA2 = PduInfoPtr->SduPtr[2];
				break;
			case 3:
				LINFLEXHw->BDRL.B.DATA3 = PduInfoPtr->SduPtr[3];
				break;
			case 4:
				LINFLEXHw->BDRM.B.DATA4 = PduInfoPtr->SduPtr[4];
				break;
			case 5:
				LINFLEXHw->BDRM.B.DATA5 = PduInfoPtr->SduPtr[5];
				break;
			case 6:
				LINFLEXHw->BDRM.B.DATA6 = PduInfoPtr->SduPtr[6];
				break;
			case 7:
				LINFLEXHw->BDRM.B.DATA7 = PduInfoPtr->SduPtr[7];
				break;
			}
		}

		LinChannelStatus[Channel]=LIN_TX_BUSY;
	}
	else
	{
		LinChannelStatus[Channel]=LIN_RX_BUSY;
	}

	/* Request transmission of header */
	LINFLEXHw->LINCR2.B.HTRQ = 1;

	return E_OK;
}

Std_ReturnType Lin_SendResponse(  uint8 Channel,   Lin_PduType* PduInfoPtr )
{
	VALIDATE_W_RV( (LinDriverStatus != LIN_UNINIT), LIN_SEND_RESPONSE_SERVICE_ID, LIN_E_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_UNINIT), LIN_SEND_RESPONSE_SERVICE_ID, LIN_E_CHANNEL_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (Channel < LIN_CONTROLLER_CNT), LIN_SEND_RESPONSE_SERVICE_ID, LIN_E_INVALID_CHANNEL, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_SLEEP), LIN_SEND_RESPONSE_SERVICE_ID, LIN_E_STATE_TRANSITION, E_NOT_OK);
	VALIDATE_W_RV( (PduInfoPtr != NULL), LIN_SEND_RESPONSE_SERVICE_ID, LIN_E_INVALID_POINTER, E_NOT_OK);

	/* The response is sent from within the header in this implementation since this is a master only implementation */
	return E_OK;
}

Std_ReturnType Lin_GoToSleep(  uint8 Channel )
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);

	VALIDATE_W_RV( (LinDriverStatus != LIN_UNINIT), LIN_GO_TO_SLEEP_SERVICE_ID, LIN_E_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_UNINIT), LIN_GO_TO_SLEEP_SERVICE_ID, LIN_E_CHANNEL_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (Channel < LIN_CONTROLLER_CNT), LIN_GO_TO_SLEEP_SERVICE_ID, LIN_E_INVALID_CHANNEL, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_SLEEP), LIN_GO_TO_SLEEP_SERVICE_ID, LIN_E_STATE_TRANSITION, E_NOT_OK);

	if (LinChannelOrderedStatus[Channel]!=LIN_CH_SLEEP){
		LinChannelOrderedStatus[Channel]=LIN_CH_SLEEP;

		LINFLEXHw->LINCR1.B.SLEEP = 1;

		LINFLEXHw->LINIER.B.WUIE = 1; /* enable wake-up irq */
	}
	return E_OK;
}

Std_ReturnType Lin_GoToSleepInternal(  uint8 Channel )
{
	VALIDATE_W_RV( (LinDriverStatus != LIN_UNINIT), LIN_GO_TO_SLEEP_INTERNAL_SERVICE_ID, LIN_E_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_UNINIT), LIN_GO_TO_SLEEP_INTERNAL_SERVICE_ID, LIN_E_CHANNEL_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (Channel < LIN_CONTROLLER_CNT), LIN_GO_TO_SLEEP_INTERNAL_SERVICE_ID, LIN_E_INVALID_CHANNEL, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_SLEEP), LIN_GO_TO_SLEEP_INTERNAL_SERVICE_ID, LIN_E_STATE_TRANSITION, E_NOT_OK);
	Lin_GoToSleep(Channel);
	return E_OK;
}

Std_ReturnType Lin_WakeUp( uint8 Channel )
{
	volatile struct LINFLEX_tag * LINFLEXHw = LINFLEX(Channel);

	VALIDATE_W_RV( (LinDriverStatus != LIN_UNINIT), LIN_WAKE_UP_SERVICE_ID, LIN_E_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_UNINIT), LIN_WAKE_UP_SERVICE_ID, LIN_E_CHANNEL_UNINIT, E_NOT_OK);
	VALIDATE_W_RV( (Channel < LIN_CONTROLLER_CNT), LIN_WAKE_UP_SERVICE_ID, LIN_E_INVALID_CHANNEL, E_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] == LIN_CH_SLEEP), LIN_WAKE_UP_SERVICE_ID, LIN_E_STATE_TRANSITION, E_NOT_OK);

	/* Disable wake interrupt */
	LINFLEXHw->LINIER.B.WUIE = 0;
	/* Clear sleep bit */
	LINFLEXHw->LINCR1.B.SLEEP = 0;

	LinChannelStatus[Channel]=LIN_CH_OPERATIONAL;
	return E_OK;
}

Lin_StatusType Lin_GetStatus( uint8 Channel, uint8** Lin_SduPtr )
{
	VALIDATE_W_RV( (LinDriverStatus != LIN_UNINIT), LIN_GETSTATUS_SERVICE_ID, LIN_E_UNINIT, LIN_NOT_OK);
	VALIDATE_W_RV( (LinChannelStatus[Channel] != LIN_CH_UNINIT), LIN_GETSTATUS_SERVICE_ID, LIN_E_CHANNEL_UNINIT, LIN_NOT_OK);
	VALIDATE_W_RV( (Channel < LIN_CONTROLLER_CNT), LIN_GETSTATUS_SERVICE_ID, LIN_E_INVALID_CHANNEL, LIN_NOT_OK);
	VALIDATE_W_RV( (Lin_SduPtr!=NULL), LIN_GETSTATUS_SERVICE_ID, LIN_E_INVALID_POINTER, LIN_NOT_OK);

	imask_t state;
    Irq_Save(state);
	Lin_StatusType res = LinChannelStatus[Channel];
	/* We can only check for valid sdu ptr when LIN_RX_OK */
	if(LinChannelStatus[Channel] == LIN_RX_OK || LinChannelStatus[Channel] == LIN_RX_ERROR){
		*Lin_SduPtr = LinBufRx[Channel];
		if(LinChannelStatus[Channel] == LIN_RX_ERROR){
			ResyncDriver(Channel);
		}
		LinChannelStatus[Channel]=LIN_CH_OPERATIONAL;
	} else if(LinChannelStatus[Channel] == LIN_TX_OK || LinChannelStatus[Channel] == LIN_TX_ERROR){
		if(LinChannelStatus[Channel] == LIN_TX_ERROR){
			ResyncDriver(Channel);
		}
		LinChannelStatus[Channel]=LIN_CH_OPERATIONAL;
	}
    Irq_Restore(state);
	return res;
}




