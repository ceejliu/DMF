/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfFromWdf.h

Abstract:

    Includes files needed to compile Wdf APIs in non-native WDF platforms.

Environment:

    Win32 Application

--*/

#pragma once

#define WDF_EVERYTHING_ALWAYS_AVAILABLE
#include ".\Platform\FromWdf\wudfwdm.h"
#include ".\Platform\FromWdf\wdftypes.h"
#include ".\Platform\FromWdf\wdfglobals.h"
#include ".\Platform\FromWdf\wdffuncenum.h"

typedef VOID (*WDFFUNC) (VOID);
extern WDFFUNC WdfFunctions[WdfFunctionTableNumEntries];

#include ".\Platform\FromWdf\wdfobject.h"
#include ".\Platform\FromWdf\wdfcore.h"
#include ".\Platform\FromWdf\wdfdriver.h"

// It is not used in non-Windows platforms but the header files still 
// need to compile.
//
typedef struct _PNP_BUS_INFORMATION
{
    GUID BusTypeGuid;
    INTERFACE_TYPE LegacyBusType;
    ULONG BusNumber;
} PNP_BUS_INFORMATION, *PPNP_BUS_INFORMATION;
#define WDF_DEVICE_NO_WDMSEC_H
#include ".\Platform\FromWdf\wdfdevice.h"

#include ".\Platform\FromWdf\wdfmemory.h"
#include ".\Platform\FromWdf\wdfsync.h"
#include ".\Platform\FromWdf\wdftimer.h"
#include ".\Platform\FromWdf\wdfworkitem.h"
#include ".\Platform\FromWdf\wdfcollection.h"
#include ".\Platform\FromWdf\wdfstring.h"
#include ".\Platform\FromWdf\wdfregistry.h"

// Dummy IO_STACK_LOCATION parameter so that the include files compile.
// It is not used in non-Windows platforms but the header files still 
// need to compile.
//
typedef struct 
{
    ULONG Dummy;
} IO_STACK_LOCATION, *PIO_STACK_LOCATION;
#include ".\Platform\FromWdf\wdfrequest.h"

#include ".\Platform\FromWdf\wdfio.h"
#include ".\Platform\FromWdf\wdffileobject.h"

// eof: DmfFromWdf.h
//
