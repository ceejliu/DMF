/*++

    Copyright (c) 2018 Microsoft Corporation. All rights reserved

Module Name:

    DmfInterface.c

Abstract:

   Instantiate Dmf Library Modules used by THIS driver.
   
   (This is the only code file in the driver that contains unique code for this driver. All other code the
   driver executes is in the Dmf Framework.)

Environment:

    User-mode Driver Framework

--*/

// The Dmf Library and the Dmf Library Modules this driver uses.
//
#include <initguid.h>

#pragma warning(push)
#pragma warning(disable:4324)
#include "DmfModules.Library.Tests.h"
#include "DmfModules.Library.Tests.Trace.h"
#pragma warning(pop)

#if defined(DMF_INCLUDE_TMH)
#include "Trace.h"
#include "DmfInterface.tmh"
#endif

#include <conio.h>
#include <time.h>

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfDeviceModulesAdd(
    _In_ WDFDEVICE Device,
    _In_ PDMFMODULE_INIT DmfModuleInit
    )
/*++

Routine Description:

    Add all the Dmf Modules used by this driver.

Arguments:

    Device - WDFDEVICE handle.
    DmfModuleInit - Opaque structure to be passed to DMF_DmfModuleAdd.

Return Value:

    NTSTATUS

--*/
{
    DMF_MODULE_ATTRIBUTES moduleAttributes;

    UNREFERENCED_PARAMETER(Device);
    UNREFERENCED_PARAMETER(DmfModuleInit);

    PAGED_CODE();

    // Tests_BufferPool
    // ----------------
    //
    DMF_Tests_BufferPool_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // Tests_BufferQueue
    // ----------------
    //
    DMF_Tests_BufferQueue_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // Tests_RingBuffer
    // ----------------
    //
    DMF_Tests_RingBuffer_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // Tests_PingPongBuffer
    // --------------------
    //
    DMF_Tests_PingPongBuffer_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // Tests_HashTable
    // ---------------
    //
    DMF_Tests_HashTable_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // Tests_String
    // -------------
    //
    DMF_Tests_String_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);

    // Tests_AlertableSleep
    // --------------------
    //
    DMF_Tests_AlertableSleep_ATTRIBUTES_INIT(&moduleAttributes);
    DMF_DmfModuleAdd(DmfModuleInit,
                     &moduleAttributes,
                     WDF_NO_OBJECT_ATTRIBUTES,
                     NULL);
}

void
__cdecl
main()
{
    NTSTATUS ntStatus;                                                          
    WDFDEVICE device;                                                           
    PDMFDEVICE_INIT dmfDeviceInit;                                              
    DMF_EVENT_CALLBACKS dmfCallbacks;                                           
    PWDFDEVICE_INIT deviceInit;

    srand((unsigned)time(NULL));

Start:
    printf("Starting...\n");

    DMF_PLATFORM_PARAMETERS dmfPlatformParameters;

    DMF_PLATFORM_PARAMETERS_INIT(&dmfPlatformParameters);
    dmfPlatformParameters.TraceLoggingLevel = TRACE_LEVEL_INFORMATION;
    dmfPlatformParameters.TraceLoggingFlags = 0xFFFFFFFF;
    DMF_PlatformInitialize(&dmfPlatformParameters);

    deviceInit = (PWDFDEVICE_INIT)&deviceInit;
    dmfDeviceInit = DMF_DmfDeviceInitAllocate(deviceInit);                      
                                                                                
    DMF_DmfDeviceInitHookPnpPowerEventCallbacks(dmfDeviceInit,                  
                                                NULL);                          
    DMF_DmfDeviceInitHookFileObjectConfig(dmfDeviceInit,                        
                                          NULL);                                
    DMF_DmfDeviceInitHookPowerPolicyEventCallbacks(dmfDeviceInit,               
                                                   NULL);                       
                                                                                
    ntStatus = WdfDeviceCreate(&deviceInit,                                     
                               WDF_NO_OBJECT_ATTRIBUTES,                        
                               &device);                                        
    if (! NT_SUCCESS(ntStatus))                                                 
    {                                                                           
        goto Exit;                                                              
    }                                                                           
                                                                                
    DMF_EVENT_CALLBACKS_INIT(&dmfCallbacks);                                    
    dmfCallbacks.EvtDmfDeviceModulesAdd = DmfDeviceModulesAdd;                  
    DMF_DmfDeviceInitSetEventCallbacks(dmfDeviceInit,                           
                                       &dmfCallbacks);                          
                                                                                
    ntStatus = DMF_ModulesCreate(device,                                        
                                 &dmfDeviceInit);                               
    if (! NT_SUCCESS(ntStatus))                                                 
    {                                                                           
        goto Exit;                                                              
    }                                                                           
                                                                                
    ULONG millisecondsToSleep = (rand() % 60) * 1000;
    printf("Waiting %d seconds...", millisecondsToSleep/1000);
    Sleep(millisecondsToSleep);
    printf("Wait satisfied.\n");

Exit:                                                                           
                                                                                
    if (dmfDeviceInit != NULL)                                                  
    {                                                                           
        DMF_DmfDeviceInitFree(&dmfDeviceInit);                                  
    }

    // Perform platform specific uninitialization including freeing all
    // allocated resources.
    //
    DMF_PlatformUninitialize(device);

    goto Start;
}

// eof: DmfInterface.c
//
