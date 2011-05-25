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








#include "PduR.h"

#include <string.h>
#include "debug.h"
#include "Det.h"
#if defined(USE_DEM)
#include "Dem.h"
#endif

#if PDUR_ZERO_COST_OPERATION == STD_OFF

#define PduRTpBuffer(_id) (&PduRConfig->TpBuffers[_id])
#define PduRTpRouteBuffer(_id) (PduRConfig->TpRouteBuffers[_id])


BufReq_ReturnType PduR_ARC_AllocateRxBuffer(PduIdType PduId, PduLengthType TpSduLength) {
	BufReq_ReturnType failRetVal = BUFREQ_BUSY;
	for (uint8 i = 0; PduRTpBuffer(i)->pduInfoPtr != NULL; i++) {
		if (PduRTpBuffer(i)->status == PDUR_BUFFER_FREE) {
			if (PduRTpBuffer(i)->pduInfoPtr->SduLength < TpSduLength) {
				failRetVal = BUFREQ_OVFL;
			} else {
				PduRTpRouteBuffer(PduId) = PduRTpBuffer(i);
				PduRTpRouteBuffer(PduId)->status = PDUR_BUFFER_RX_BUSY;
				return BUFREQ_OK;
			}
		}
	}
	return failRetVal;
}

BufReq_ReturnType PduR_ARC_AllocateTxBuffer(PduIdType PduId, uint16 length) {
	if (PduRTpRouteBuffer(PduId)->status == PDUR_BUFFER_TX_READY) {
		if (length >= PduRTpRouteBuffer(PduId)->pduInfoPtr->SduLength) {
			PduRTpRouteBuffer(PduId)->status = PDUR_BUFFER_TX_BUSY;
			return BUFREQ_OK;
		} else {
			return BUFREQ_NOT_OK;
		}
	}
	return BUFREQ_BUSY;
}

BufReq_ReturnType PduR_ARC_ReleaseRxBuffer(PduIdType PduId) {
	if (PduRTpRouteBuffer(PduId) == NULL) return BUFREQ_OK;
	if (PduRTpRouteBuffer(PduId)->status == PDUR_BUFFER_RX_BUSY) {
		PduRTpRouteBuffer(PduId)->status = PDUR_BUFFER_TX_READY;
		return BUFREQ_BUSY;
	}
	return BUFREQ_OK;
}

BufReq_ReturnType PduR_ARC_ReleaseTxBuffer(PduIdType PduId) {
	if (PduRTpRouteBuffer(PduId)->status == PDUR_BUFFER_TX_BUSY) {
		PduRTpRouteBuffer(PduId)->status = PDUR_BUFFER_FREE;
		PduRTpRouteBuffer(PduId) = NULL;
		return BUFREQ_OK;
	}
	return BUFREQ_NOT_OK;
}

Std_ReturnType PduR_ARC_Transmit(PduIdType PduId, const PduInfoType* PduInfo, uint8 serviceId) {
	PDUR_VALIDATE_INITIALIZED(serviceId,E_NOT_OK);
	PDUR_VALIDATE_PDUPTR(serviceId, PduInfo, E_NOT_OK);
	PDUR_VALIDATE_PDUID(serviceId, PduId, E_NOT_OK);

	Std_ReturnType retVal = E_OK;
	const PduRRoutingPath_type *route = PduRConfig->RoutingPaths[PduId];
	for (int i = 0; route->PduRDestPdus[i] != NULL; i++) {
		const PduRDestPdu_type * destination = route->PduRDestPdus[i];

		retVal |= PduR_ARC_RouteTransmit(destination, PduInfo);
	}
	return retVal;
}

void PduR_ARC_RxIndicationTT(const PduRDestPdu_type * destination, const PduInfoType *PduInfo, uint8 BufferLength) {
	Std_ReturnType retVal = E_OK;

	uint8 bytesToCopy = 0;
	if (PduInfo->SduLength > BufferLength) bytesToCopy = BufferLength;
	else bytesToCopy = PduInfo->SduLength;

	if (!memcpy(destination->TxBufferRef, PduInfo->SduDataPtr, bytesToCopy)) retVal |= E_NOT_OK;
	retVal |= PduR_ARC_RouteTransmit(destination, PduInfo);

	if (retVal != E_OK) {
#if defined(USE_DEM)
		Dem_ReportErrorStatus(PDUR_E_PDU_INSTANCE_LOST, DEM_EVENT_STATUS_FAILED);
#endif
	}
	/*
	// This is a gateway request which uses trigger transmit data provision. PDUR255
	if (destination->TxBufferRef->TxConfP) { // Transfer confirmation pending.
		// Enqueue the new I-PDU. This will flush the buffer if it is full according to the buffer specification.
		PduR_BufferQueue(destination->TxBufferRef, PduInfo->SduDataPtr);
		// TODO report PDUR_E_PDU_INSTANCE_LOST to DEM if needed.
	}

	if (destination->TxBufferRef->TxConfP) { // No transfer confirmation pending (anymore).
		uint8 val[PduInfo->SduLength];
		PduInfoType NewPduInfo = {
			.SduDataPtr = val,
			.SduLength = PduInfo->SduLength
		};
		PduR_BufferDeQueue(destination->TxBufferRef, val);
		PduR_BufferQueue(destination->TxBufferRef, PduInfo->SduDataPtr);
		retVal = PduR_ARC_RouteTransmit(destination, &NewPduInfo);
		if (retVal == E_OK) {
			setTxConfP(destination->TxBufferRef);
		}
	}
	*/
}

void PduR_ARC_RxIndicationDirect(const PduRDestPdu_type * destination, const PduInfoType *PduInfo) {
	Std_ReturnType retVal = PduR_ARC_RouteTransmit(destination, PduInfo);
	if (retVal != E_OK) {
#if defined(USE_DEM)
		Dem_ReportErrorStatus(PDUR_E_PDU_INSTANCE_LOST, DEM_EVENT_STATUS_FAILED);
#endif
	}
}

void PduR_ARC_RxIndication(PduIdType PduId, const PduInfoType* PduInfo, uint8 serviceId) {
	PDUR_VALIDATE_INITIALIZED(serviceId);
	PDUR_VALIDATE_PDUPTR(serviceId, PduInfo);
	PDUR_VALIDATE_PDUID(serviceId, PduId);

	const PduRRoutingPath_type *route = PduRConfig->RoutingPaths[PduId];

	for (int i = 0; route->PduRDestPdus[i] != NULL; i++) {
		const PduRDestPdu_type * destination = route->PduRDestPdus[i];

		if (PduR_IsUpModule(destination->DestModule)) {
			PduR_ARC_RouteRxIndication(destination, PduInfo);

		} else if (PduR_IsLoModule(destination->DestModule)) {

			if (PduR_IsTpModule(destination->DestModule)) { // TP Gateway
				if (PduR_ARC_ReleaseRxBuffer(PduId) == BUFREQ_BUSY) {
					// Transmit previous rx buffer
					PduR_ARC_RouteTransmit(destination, PduRTpRouteBuffer(PduId)->pduInfoPtr);
				}

			} else if (destination->DataProvision == PDUR_TRIGGER_TRANSMIT) {
				PduR_ARC_RxIndicationTT(destination, PduInfo, route->SduLength);

			} else if (destination->DataProvision == PDUR_DIRECT) {
				PduR_ARC_RxIndicationDirect(destination, PduInfo);

			}
		}
	}
}

void PduR_ARC_TxConfirmation(PduIdType PduId, uint8 result, uint8 serviceId) {
	PDUR_VALIDATE_INITIALIZED(serviceId);
	PDUR_VALIDATE_PDUID(serviceId, PduId);

	const PduRRoutingPath_type *route = PduRConfig->RoutingPaths[PduId];

	if (PduR_IsUpModule(route->SrcModule)) {
		PduR_ARC_RouteTxConfirmation(route, result);

	} else if (PduR_IsLoModule(route->SrcModule)) {
		// Release any buffer hold by this route.
		PduR_ARC_ReleaseTxBuffer(PduId);
	}
}

Std_ReturnType PduR_ARC_TriggerTransmit(PduIdType PduId, PduInfoType* PduInfo, uint8 serviceId) {
	PDUR_VALIDATE_INITIALIZED(serviceId, E_NOT_OK);
	PDUR_VALIDATE_PDUPTR(serviceId, PduInfo, E_NOT_OK);
	PDUR_VALIDATE_PDUID(serviceId, PduId, E_NOT_OK);

	Std_ReturnType retVal = E_OK;
	const PduRRoutingPath_type *route = PduRConfig->RoutingPaths[PduId];
	const PduRDestPdu_type * destination = route->PduRDestPdus[0];

	if (PduR_IsUpModule(route->SrcModule)) {
		retVal |= PduR_ARC_RouteTriggerTransmit(route, PduInfo);

	} else if (PduR_IsLoModule(route->SrcModule)) {
		if (destination->DataProvision == PDUR_TRIGGER_TRANSMIT) {
			uint8 bytesToCopy = 0;
			if (PduInfo->SduLength > route->SduLength) bytesToCopy = route->SduLength;
			else bytesToCopy = PduInfo->SduLength;

			if (!memcpy((void *)PduInfo->SduDataPtr, (void *)destination->TxBufferRef, bytesToCopy)) {
				retVal = E_NOT_OK;
			}
		}
	}
	return retVal;
}

BufReq_ReturnType PduR_ARC_ProvideRxBuffer(PduIdType PduId, PduLengthType TpSduLength, PduInfoType** PduInfoPtr, uint8 serviceId) {
	PDUR_VALIDATE_INITIALIZED(serviceId,BUFREQ_NOT_OK);
	PDUR_VALIDATE_PDUPTR(serviceId, PduInfoPtr, BUFREQ_NOT_OK);
	PDUR_VALIDATE_PDUID(serviceId, PduId, BUFREQ_NOT_OK);

	BufReq_ReturnType retVal = BUFREQ_NOT_OK;
	const PduRRoutingPath_type *route = PduRConfig->RoutingPaths[PduId];
	const PduRDestPdu_type * destination = route->PduRDestPdus[0];

	if (PduR_IsUpModule(destination->DestModule)) {
		retVal = PduR_ARC_RouteProvideRxBuffer(destination, TpSduLength, PduInfoPtr);

	} else if (PduR_IsLoModule(destination->DestModule)) {
		if (PduR_ARC_ReleaseRxBuffer(PduId) == BUFREQ_BUSY) {
			// Transmit previous rx buffer
			PduR_ARC_RouteTransmit(destination, PduRTpRouteBuffer(PduId)->pduInfoPtr);
		}

		retVal = PduR_ARC_AllocateRxBuffer(PduId, TpSduLength);
		if (retVal == BUFREQ_OK) {
			*PduInfoPtr = PduRTpRouteBuffer(PduId)->pduInfoPtr;
		}
	}
	return retVal;
}

BufReq_ReturnType PduR_ARC_ProvideTxBuffer(PduIdType PduId, PduInfoType** PduInfoPtr, uint16 Length, uint8 serviceId) {
	PDUR_VALIDATE_INITIALIZED(serviceId,BUFREQ_NOT_OK);
	PDUR_VALIDATE_PDUPTR(serviceId, PduInfoPtr, BUFREQ_NOT_OK);
	PDUR_VALIDATE_PDUID(serviceId, PduId, BUFREQ_NOT_OK);

	BufReq_ReturnType retVal = BUFREQ_NOT_OK;
	const PduRRoutingPath_type *route = PduRConfig->RoutingPaths[PduId];

	if (PduR_IsUpModule(route->SrcModule)) {
		retVal = PduR_ARC_RouteProvideTxBuffer(route, Length, PduInfoPtr);

	} else if (PduR_IsLoModule(route->SrcModule)) {
		retVal = PduR_ARC_AllocateTxBuffer(PduId, Length);
		if (retVal == BUFREQ_OK) {
			*PduInfoPtr = PduRTpRouteBuffer(PduId)->pduInfoPtr;
		}
	}
	return retVal;
}



#endif