/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPlatform.c

Abstract:

    This is the common code for all non-native WDF platforms. Functions are here are the top edge of
    WDF support for non-WDF platforms.

    NOTE: Make sure to set "compile as C++" option.

    NOTE: The function headers' arguments are explained in cases where the parameter
          is used in a specific manner in the function or has special meaning. All other
          parameters are annotated with "(See MSDN for meanings of the rest of the parameters.)"

Environment:

    Non-native WDF platforms.

--*/

#include "DmfIncludeInternal.h"

#if defined(DMF_WDF_DRIVER)
#include "DmfPlatform.tmh"
#endif

#if defined(__cplusplus)
extern "C" 
{
#endif // defined(__cplusplus)

#if !defined(DMF_WDF_DRIVER)

///////////////////////////////////////////////////////////////////////////////////
// WDFOBJECT
///////////////////////////////////////////////////////////////////////////////////
//

typedef
void
(*DmfPlatform_PlatformObjectDeletionFunction)(_In_ DMF_PLATFORM_OBJECT* PlatformObject);

DMF_PLATFORM_OBJECT*
DmfPlatformObjectCreate(
    _In_ DMF_PLATFORM_OBJECT* Parent,
    _In_ DMF_Platform_ObjectDelete ObjectDeleteCallback
    );

NTSTATUS
DmfPlatform_CustomContextAllocate(
    _In_ WDFOBJECT Object,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* Attributes
    )
/*++

Routine Description:

    Internal helper function that allocates a given Custom Context.
    All objects can have a custom context assigned. This helper function is called
    by all object creation functions.

Arguments:

    Object - The object to which the allocated given Custom Context is added to.
    Attributes - Describes the Custom Context to allocate.

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;

    if (Attributes != WDF_NO_OBJECT_ATTRIBUTES && 
        Attributes->ContextTypeInfo != NULL &&
        Attributes->ContextTypeInfo->ContextSize > 0)
    {
        ntStatus = WdfObjectAllocateContext(Object,
                                            Attributes,
                                            NULL);
    }
    else
    {
        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

DMF_PLATFORM_OBJECT*
DmfPlatform_ObjectCreateProlog(
    _In_ DmfPlatform_PlatformObjectDeletionFunction PlatformDeletionFunction,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* Attributes,
    _In_ size_t SizeOfPlatformSpecificData,
    _In_ DmfPlatformObjectType PlatformObjectType
    )
/*++

Routine Description:

    Internal helper function that allocates and sets up a DMF_PLATFORM_OBJECT.

Arguments:

    PlatformDeletionFunction - The object custom deletion function to be called to 
                               delete the object platform specific data.
    Attributes - Passed by the caller that wants to create the object.
    SizeOfPlatformSpecificData - Size of the platform specific data that is allocated.
    PlatformObjectType - Indicates the platform specific object type.

Return Value:

    The created DMF Platform object or NULL if an error occurs.

--*/
{
    DMF_PLATFORM_OBJECT* parentObject;
    DMF_PLATFORM_OBJECT* platformObject;

    if (Attributes != NULL)
    {
        parentObject = (DMF_PLATFORM_OBJECT*)Attributes->ParentObject;
    }
    else
    {
        parentObject = NULL;
    }

    // Allocate the object.
    //
    platformObject = DmfPlatformObjectCreate(parentObject,
                                             PlatformDeletionFunction);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatformObjectCreate fails");
        goto Exit;
    }

    // Allocate the platform specific object container.
    //
    platformObject->Data = (void*)DmfPlatformHandlersTable.DmfPlatformHandlerAllocate(SizeOfPlatformSpecificData);
    if (platformObject->Data == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "platformObject->Data fails");
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
        goto Exit;
    }

    // This identifier is used for debug purposes.
    //
    platformObject->PlatformObjectType = PlatformObjectType;

    // Save the passed attributes for later use by object's functions as needed.
    //
    if (Attributes != NULL)
    {
        memcpy(&platformObject->ObjectAttributes,
               Attributes,
               sizeof(WDF_OBJECT_ATTRIBUTES));
    }

    NTSTATUS ntStatus;

    ntStatus = DmfPlatform_CustomContextAllocate(platformObject,
                                                 Attributes);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_CustomContextAllocate fails: ntStatus=0x08X", ntStatus);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
    }

Exit:

    return platformObject;
}

NTSTATUS
DmfPlatform_WdfObjectAllocateContext(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFOBJECT Handle,
    _In_ WDF_OBJECT_ATTRIBUTES* ContextAttributes,
    _Outptr_opt_ VOID** Context
    )
/*++

Routine Description:

    Platform implementation of WdfObjectAllocateContext().

    TODO: Use ContextSizeOverride.

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    Handle - DMF Platform object.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_CONTEXT* platformContext;

    UNREFERENCED_PARAMETER(DriverGlobals);

    DmfAssert(ContextAttributes != NULL);

    platformObject = (DMF_PLATFORM_OBJECT*)Handle;

    platformContext = (DMF_PLATFORM_CONTEXT*)DmfPlatformHandlersTable.DmfPlatformHandlerAllocate(sizeof(DMF_PLATFORM_CONTEXT));
    if (platformContext == NULL)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatformHandlerAllocate fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    platformContext->ContextData = DmfPlatformHandlersTable.DmfPlatformHandlerAllocate(ContextAttributes->ContextTypeInfo->ContextSize);
    if (platformContext->ContextData == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatformHandlerAllocate fails");
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformContext);
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    memcpy(&platformContext->ContextTypeInfo,
           ContextAttributes->ContextTypeInfo,
           sizeof(WDF_OBJECT_CONTEXT_TYPE_INFO));

    InitializeListHead(&platformContext->ListEntry);

    DmfPlatformHandlersTable.DmfPlatformHandlerLock(&platformObject->ContextListLock);
    InsertTailList(&platformObject->ContextList,
                   &platformContext->ListEntry);
    DmfPlatformHandlersTable.DmfPlatformHandlerUnlock(&platformObject->ContextListLock);

    if (Context != NULL)
    {
        *Context = platformContext->ContextData;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

VOID*
DmfPlatform_WdfObjectGetTypedContextWorker(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFOBJECT Handle,
    _In_ PCWDF_OBJECT_CONTEXT_TYPE_INFO TypeInfo
    )
/*++

Routine Description:

    Platform implementation of WdfObjectGetTypedContextWorker().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    Handle - Platform object.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    VOID*

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_CONTEXT* platformContext;
    LIST_ENTRY* listEntry;
    VOID* returnValue;

    UNREFERENCED_PARAMETER(DriverGlobals);

    returnValue = NULL;
    platformObject = (DMF_PLATFORM_OBJECT*)Handle;
    DmfPlatformHandlersTable.DmfPlatformHandlerLock(&platformObject->ContextListLock);
    listEntry = platformObject->ContextList.Flink;
    while (listEntry != &platformObject->ContextList)
    {
        platformContext = CONTAINING_RECORD(listEntry,
                                            DMF_PLATFORM_CONTEXT,
                                            ListEntry);
        if (TypeInfo->ContextName == platformContext->ContextTypeInfo.ContextName)
        {
            returnValue = platformContext->ContextData;
            break;
        }
        listEntry = listEntry->Flink;
    }
    DmfPlatformHandlersTable.DmfPlatformHandlerUnlock(&platformObject->ContextListLock);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfObjectDelete(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFOBJECT Object
    )
/*++

Routine Description:

    Platform implementation of WdfObjectDelete().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    Object - DMF Platform object.

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_OBJECT* childObject;
    LIST_ENTRY* listEntry;
    LONG newReferenceCount;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Object;

    // Decrement reference count.
    //
    newReferenceCount = InterlockedDecrement(&platformObject->ReferenceCount);

    // Always call clean up callback for each decrement.
    //
    if (platformObject->ObjectAttributes.EvtCleanupCallback != NULL)
    {
        platformObject->ObjectAttributes.EvtCleanupCallback(platformObject);
    }

    // Actually destroy object when reference count is zero.
    //
    if (newReferenceCount == 0)
    {
        // NOTE: Do not hold the ChildListLock while deleting because of potential 
        //       for deadlock when removing child from parent list.
        //       Caller should not be doing anything else with object while it
        //       is being deleted.
        //
        listEntry = platformObject->ChildList.Flink;
        while (listEntry != &platformObject->ChildList)
        {
            DmfAssert(platformObject->NumberOfChildren >= 0);
            childObject = CONTAINING_RECORD(listEntry,
                                            DMF_PLATFORM_OBJECT,
                                            ChildListEntry);
            // NOTE: Number of children is decremented when object is removed from
            //       list. It is not removed from list now.
            //
            WdfObjectDelete(childObject);
            // The first object is the next object in the list (if present).
            //
            listEntry = platformObject->ChildList.Flink;
        }

        // Call the destroy callback.
        //
        if (platformObject->ObjectAttributes.EvtDestroyCallback != NULL)
        {
            platformObject->ObjectAttributes.EvtDestroyCallback(platformObject);
        }

        // Remove this object from its parent.
        //
        if (platformObject->ObjectAttributes.ParentObject != NULL)
        {
            DMF_PLATFORM_OBJECT* parentPlatformObject = (DMF_PLATFORM_OBJECT*)platformObject->ObjectAttributes.ParentObject;
            DmfPlatformHandlersTable.DmfPlatformHandlerLock(&parentPlatformObject->ChildListLock);
            DmfAssert(parentPlatformObject->NumberOfChildren > 0);
            RemoveEntryList(&platformObject->ChildListEntry);
            parentPlatformObject->NumberOfChildren--;
            DmfAssert(parentPlatformObject->NumberOfChildren >= 0);
            DmfPlatformHandlersTable.DmfPlatformHandlerUnlock(&parentPlatformObject->ChildListLock);
        }
        // Free the memory used by this object.
        //
        if (platformObject->ObjectDelete != NULL)
        {
            // Free the object specific data.
            //
            platformObject->ObjectDelete(platformObject);
        }
        // Free the container for object specific data.
        //
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        // Free the object's critical section.
        //
        DmfPlatformHandlersTable.DmfPlatformHandlerLockDelete(&platformObject->ChildListLock);
        DmfPlatformHandlersTable.DmfPlatformHandlerLockDelete(&platformObject->ContextListLock);
        // Free the object's main memory.
        //
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
    }
}

DMF_PLATFORM_OBJECT*
DmfPlatformObjectCreate(
    _In_ DMF_PLATFORM_OBJECT* Parent,
    _In_ DMF_Platform_ObjectDelete ObjectDeleteCallback
    )
/*++

Routine Description:

    Creates the equivalent of a Platform specific WDFOBJECT. This is the parent 
    container for all other Platform versions of WDF objects. It contains all the
    information needed to support WDF APIs as well as a pointer to opaque Platform
    specific information.

Arguments:

    Parent - The parent DMF_PLATORM_OBJECT that should be assigned to the newly 
             created DMF_PLATFORM_OBJECT or NULL if not specified.
    ObjectDeleteCallback - The Platform specific deletion callback that deletes
                           Platform specific information in the object's 
                           opaque buffer.

Return Value:

    The address of the newly created DMF_PLATFORM_OBJECT (WDFOBJECT) or NULL if
    the object could not be created.

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;

    platformObject = (DMF_PLATFORM_OBJECT*)DmfPlatformHandlersTable.DmfPlatformHandlerAllocate(sizeof(DMF_PLATFORM_OBJECT));
    if (platformObject == NULL)
    {
        goto Exit;
    }

    platformObject->ReferenceCount = 1;

    // Set object delete callback. It deletes platform specific constructs
    // allocated when the object is created.
    //
    platformObject->ObjectDelete = ObjectDeleteCallback;

    // Initialize the list of contexts.
    //
    InitializeListHead(&platformObject->ContextList);
    DmfPlatformHandlersTable.DmfPlatformHandlerLockCreate(&platformObject->ContextListLock);

    // Initialize the list of children.
    //
    InitializeListHead(&platformObject->ChildList);
    DmfPlatformHandlersTable.DmfPlatformHandlerLockCreate(&platformObject->ChildListLock);
    
    DmfAssert(platformObject->ChildListEntry.Flink == NULL);
    DmfAssert(platformObject->ChildListEntry.Blink == NULL);

    // Initialize this object as not a child.
    //
    InitializeListHead(&platformObject->ChildListEntry);

    if (Parent != NULL)
    {
        DmfPlatformHandlersTable.DmfPlatformHandlerLock(&Parent->ChildListLock);
        InsertTailList(&Parent->ChildList,
                       &platformObject->ChildListEntry);
        Parent->NumberOfChildren++;
        DmfPlatformHandlersTable.DmfPlatformHandlerUnlock(&Parent->ChildListLock);
    }

Exit:

    return platformObject;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFMEMORY
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfMemoryDelete(
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
/*++

Routine Description:

    Deletes the private Platform specific data for a given memory.

Arguments:

    PlatformObject - The container of the given memory.

Return Value:

    None

--*/
{
    DmfAssert(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeMemory);

    DMF_PLATFORM_MEMORY* platformMemory = (DMF_PLATFORM_MEMORY*)PlatformObject->Data;

    if (platformMemory->NeedToDeallocate)
    {
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformMemory->DataMemory);
    }
}

_Must_inspect_result_
_When_(PoolType == 1 || PoolType == 257, _IRQL_requires_max_(APC_LEVEL))
_When_(PoolType == 0 || PoolType == 256, _IRQL_requires_max_(DISPATCH_LEVEL))
NTSTATUS
DmfPlatform_WdfMemoryCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* Attributes,
    _In_ _Strict_type_match_ POOL_TYPE PoolType,
    _In_opt_ ULONG PoolTag,
    _In_ _When_(BufferSize == 0, __drv_reportError(BufferSize cannot be zero)) size_t BufferSize,
    _Out_ WDFMEMORY* Memory,
    _Outptr_opt_result_bytebuffer_(BufferSize) VOID** Buffer
    )
/*++

Routine Description:

    Platform implementation of WdfMemoryCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(PoolType);
    UNREFERENCED_PARAMETER(PoolTag);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(DmfPlatformWdfMemoryDelete,
                                                    Attributes,
                                                    sizeof(DMF_PLATFORM_MEMORY),
                                                    DmfPlatformObjectTypeMemory);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_MEMORY* platformMemory = (DMF_PLATFORM_MEMORY*)platformObject->Data;

    platformMemory->DataMemory = (CHAR*)DmfPlatformHandlersTable.DmfPlatformHandlerAllocate(BufferSize);
    if (platformMemory->DataMemory == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "platformMemory->DataMemory fails");
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
        goto Exit;
    }

    platformMemory->NeedToDeallocate = TRUE;
    platformMemory->Size = BufferSize;

    *Memory = (WDFMEMORY)platformObject;
    if (Buffer != NULL)
    {
        *Buffer = platformMemory->DataMemory;
    }

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfMemoryCreatePreallocated(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* Attributes,
    _In_ __drv_aliasesMem VOID* Buffer,
    _In_ _When_(BufferSize == 0, __drv_reportError(BufferSize cannot be zero)) size_t BufferSize,
    _Out_ WDFMEMORY* Memory
    )
/*++

Routine Description:

    Platform implementation of WdfMemoryCreatePreallocated().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(DmfPlatformWdfMemoryDelete,
                                                    Attributes,
                                                    sizeof(DMF_PLATFORM_MEMORY),
                                                    DmfPlatformObjectTypeMemory);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_MEMORY* platformMemory;

    platformObject->PlatformObjectType = DmfPlatformObjectTypeMemory;

    platformMemory = (DMF_PLATFORM_MEMORY*)platformObject->Data;

    platformMemory->DataMemory = Buffer;
    platformMemory->NeedToDeallocate = FALSE;
    platformMemory->Size = BufferSize;

    *Memory = (WDFMEMORY)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID*
DmfPlatform_WdfMemoryGetBuffer(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFMEMORY Memory,
    _Out_opt_ size_t* BufferSize
    )
/*++

Routine Description:

    Platform implementation of WdfMemoryGetBuffer().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    VOID*

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_MEMORY* platformMemory;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Memory;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeMemory);
    platformMemory = (DMF_PLATFORM_MEMORY*)platformObject->Data;
    if (BufferSize != NULL)
    {
        *BufferSize = platformMemory->Size;
    }

    return platformMemory->DataMemory;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFSYNC
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfWaitLockDelete(
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
/*++

Routine Description:

    Deletes the private Platform specific data for a given wait lock.

Arguments:

    PlatformObject - The container of the given wait lock.

Return Value:

    None

--*/
{
    DmfAssert(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeWaitLock);

    DMF_PLATFORM_WAITLOCK* platformWaitLock = (DMF_PLATFORM_WAITLOCK*)PlatformObject->Data;

    DmfPlatformHandlersTable.DmfPlatformHandlerWdfWaitLockDelete(platformWaitLock);
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfWaitLockCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* LockAttributes,
    _Out_ WDFWAITLOCK* Lock
    )
/*++

Routine Description:

    Platform implementation of WdfWaitLockCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    BOOLEAN waitEventCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(DmfPlatformWdfWaitLockDelete,
                                                    LockAttributes,
                                                    sizeof(DMF_PLATFORM_WAITLOCK),
                                                    DmfPlatformObjectTypeWaitLock);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_WAITLOCK* platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

    waitEventCreated = DmfPlatformHandlersTable.DmfPlatformHandlerWdfWaitLockCreate(platformWaitLock);
    if (! waitEventCreated)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatformHandlerWdfWaitLockCreate fails");
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
        goto Exit;
    }

    *Lock = (WDFWAITLOCK)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_When_(Timeout == NULL, _IRQL_requires_max_(PASSIVE_LEVEL))
_When_(Timeout != NULL && *Timeout == 0, _IRQL_requires_max_(DISPATCH_LEVEL))
_When_(Timeout != NULL && *Timeout != 0, _IRQL_requires_max_(PASSIVE_LEVEL))
_Always_(_When_(Timeout == NULL, _Acquires_lock_(Lock)))
_When_(Timeout != NULL && return == STATUS_SUCCESS, _Acquires_lock_(Lock))
_When_(Timeout != NULL, _Must_inspect_result_)
NTSTATUS
DmfPlatform_WdfWaitLockAcquire(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ _Requires_lock_not_held_(_Curr_) WDFWAITLOCK Lock,
    _In_opt_ LONGLONG* Timeout
    )
/*++

Routine Description:

    Platform implementation of WdfWaitLockAcquire().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WAITLOCK* platformWaitLock;
    DWORD returnValue;
    LONGLONG timeoutMs;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Lock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWaitLock);
    platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

    if (Timeout == NULL)
    {
        timeoutMs = INFINITE;
    }
    else
    {
        timeoutMs = WDF_REL_TIMEOUT_IN_MS(*Timeout);
    }

    returnValue = DmfPlatformHandlersTable.DmfPlatformHandlerWdfWaitLockAcquire(platformWaitLock,
                                                                         (DWORD)timeoutMs);
    if (returnValue == WAIT_OBJECT_0)
    {
        ntStatus = STATUS_SUCCESS;
    }
    else if (returnValue == WAIT_TIMEOUT)
    {
        ntStatus = STATUS_TIMEOUT;
    }
    else
    {
        ntStatus = STATUS_UNSUCCESSFUL;
    }

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfWaitLockRelease(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ _Requires_lock_held_(_Curr_) _Releases_lock_(_Curr_) WDFWAITLOCK Lock
    )
/*++

Routine Description:

    Platform implementation of WdfWaitLockRelease().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WAITLOCK* platformWaitLock;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Lock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWaitLock);
    platformWaitLock = (DMF_PLATFORM_WAITLOCK*)platformObject->Data;

    DmfPlatformHandlersTable.DmfPlatformHandlerWdfWaitLockRelease(platformWaitLock);
}

void
DmfPlatformWdfSpinLockDelete(
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
/*++

Routine Description:

    Deletes the private Platform specific data for a given spin lock.

Arguments:

    PlatformObject - The container of the given spin lock.

Return Value:

    None

--*/
{
    DmfAssert(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);

    DMF_PLATFORM_SPINLOCK* platformSpinLock = (DMF_PLATFORM_SPINLOCK*)PlatformObject->Data;

    DmfPlatformHandlersTable.DmfPlatformHandlerWdfSpinLockDelete(platformSpinLock);
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfSpinLockCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* SpinLockAttributes,
    _Out_ WDFSPINLOCK* SpinLock
    )
/*++

Routine Description:

    Platform implementation of WdfSpinLockCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    BOOLEAN spinLockCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(SpinLockAttributes);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(DmfPlatformWdfSpinLockDelete,
                                                    SpinLockAttributes,
                                                    sizeof(DMF_PLATFORM_SPINLOCK),
                                                    DmfPlatformObjectTypeSpinLock);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_SPINLOCK* platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

    spinLockCreated = DmfPlatformHandlersTable.DmfPlatformHandlerWdfSpinLockCreate(platformSpinLock);
    if (!spinLockCreated)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatformHandlerWdfSpinLockCreate fails");
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
        goto Exit;
    }

    *SpinLock = (WDFSPINLOCK)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_raises_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfSpinLockAcquire(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ _Requires_lock_not_held_(_Curr_) _Acquires_lock_(_Curr_) _IRQL_saves_ WDFSPINLOCK SpinLock
    )
/*++

Routine Description:

    Platform implementation of WdfSpinLockAcquire().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_SPINLOCK* platformSpinLock;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)SpinLock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);
    platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

    DmfPlatformHandlersTable.DmfPlatformHandlerWdfSpinLockAcquire(platformSpinLock);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_min_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfSpinLockRelease(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ _Requires_lock_held_(_Curr_) _Releases_lock_(_Curr_) _IRQL_restores_ WDFSPINLOCK SpinLock
    )
/*++

Routine Description:

    Platform implementation of WdfSpinLockRelease().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_SPINLOCK* platformSpinLock;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)SpinLock;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeSpinLock);
    platformSpinLock = (DMF_PLATFORM_SPINLOCK*)platformObject->Data;

    DmfPlatformHandlersTable.DmfPlatformHandlerWdfSpinLockRelease(platformSpinLock);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFTIMER
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfTimerDelete(
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
/*++

Routine Description:

    Deletes the private Platform specific data for a given timer.

Arguments:

    PlatformObject - The container of the given timer.

Return Value:

    None

--*/
{
    DmfAssert(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);

    DMF_PLATFORM_TIMER* platformTimer = (DMF_PLATFORM_TIMER*)PlatformObject->Data;

    DmfPlatformHandlersTable.DmfPlatformHandlerWdfTimerDelete(platformTimer);
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfTimerCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDF_TIMER_CONFIG* Config,
    _In_ WDF_OBJECT_ATTRIBUTES* Attributes,
    _Out_ WDFTIMER* Timer
    )
/*++

Routine Description:

    Platform implementation of WdfTimerCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    BOOLEAN timerCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(DmfPlatformWdfTimerDelete,
                                                    Attributes,
                                                    sizeof(DMF_PLATFORM_TIMER),
                                                    DmfPlatformObjectTypeTimer);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_TIMER* platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    timerCreated = DmfPlatformHandlersTable.DmfPlatformHandlerWdfTimerCreate(platformTimer,
                                                                             platformObject);
    if (! timerCreated)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatformHandlerWdfTimerCreate fails: ntStatus=0x08X", ntStatus);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
        goto Exit;
    }

    memcpy(&platformTimer->Config,
           Config,
           sizeof(WDF_TIMER_CONFIG));

    *Timer = (WDFTIMER)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
DmfPlatform_WdfTimerStart(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFTIMER Timer,
    _In_ LONGLONG DueTime
    )
/*++

Routine Description:

    Platform implementation of WdfTimerStart().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    Always FALSE indicating that timer was not in the queue.

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    returnValue = DmfPlatformHandlersTable.DmfPlatformHandlerWdfTimerStart(platformTimer,
                                                                           DueTime);

    // Always tell caller timer was not in queue.
    //
    return returnValue;
}

_When_(Wait == __true, _IRQL_requires_max_(PASSIVE_LEVEL))
_When_(Wait == __false, _IRQL_requires_max_(DISPATCH_LEVEL))
BOOLEAN
DmfPlatform_WdfTimerStop(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFTIMER Timer,
    _In_ BOOLEAN Wait
    )
/*++

Routine Description:

    Platform implementation of WdfTimerStop().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    TRUE indicates that the timer was already started.
    FALSE indicates that the timer was not yet started.

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;
    BOOLEAN returnValue;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeTimer);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    returnValue = DmfPlatformHandlersTable.DmfPlatformHandlerWdfTimerStop(platformTimer,
                                                                          Wait);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfTimerGetParentObject(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFTIMER Timer
    )
/*++

Routine Description:

    Platform implementation of WdfTimerGetParentObject().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    WDFOBJECT of the parent of the timer.

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Timer;
    return platformObject->ObjectAttributes.ParentObject;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFWORKITEM
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlatformWdfWorkItemDelete(
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
/*++

Routine Description:

    Deletes the private Platform specific data for a given workitem.

Arguments:

    PlatformObject - The container of the given workitem.

Return Value:

    None

--*/
{
    DmfAssert(PlatformObject->PlatformObjectType == DmfPlatformObjectTypeWorkItem);

    DMF_PLATFORM_WORKITEM* platformWorkItem = (DMF_PLATFORM_WORKITEM*)PlatformObject->Data;

    DmfPlatformHandlersTable.DmfPlatformHandlerWdfWorkItemDelete(platformWorkItem);
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfWorkItemCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDF_WORKITEM_CONFIG* Config,
    _In_ WDF_OBJECT_ATTRIBUTES* Attributes,
    _Out_ WDFWORKITEM* WorkItem
    )
/*++

Routine Description:

    Platform implementation of WdfWorkItemCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    BOOLEAN workitemCreated;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(DmfPlatformWdfWorkItemDelete,
                                                    Attributes,
                                                    sizeof(DMF_PLATFORM_WORKITEM),
                                                    DmfPlatformObjectTypeWorkItem);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_WORKITEM* platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    workitemCreated = DmfPlatformHandlersTable.DmfPlatformHandlerWdfWorkItemCreate(platformWorkItem,
                                                                            platformObject);
    if (!workitemCreated)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatformHandlerWdfWorkItemCreate fails: ntStatus=0x08X", ntStatus);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
        goto Exit;
    }

    memcpy(&platformWorkItem->Config,
           Config,
           sizeof(WDF_WORKITEM_CONFIG));

    *WorkItem = (WDFWORKITEM)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfWorkItemEnqueue(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFWORKITEM WorkItem
    )
/*++

Routine Description:

    Platform implementation of WdfWorkItemEnqueue().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWorkItem);
    platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    // Cause the workitem callback to execute as soon as possible.
    //
    DmfPlatformHandlersTable.DmfPlatformHandlerWdfWorkItemEnqueue(platformWorkItem);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfWorkItemGetParentObject(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFWORKITEM WorkItem
    )
/*++

Routine Description:

    Platform implementation of WdfWorkItemGetParentObject().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    WDFOBJECT of the parent of the given workitem.

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    return platformObject->ObjectAttributes.ParentObject;
}

_IRQL_requires_max_(PASSIVE_LEVEL)
VOID
DmfPlatform_WdfWorkItemFlush(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFWORKITEM WorkItem
    )
/*++

Routine Description:

    Platform implementation of WdfWorkItemFlush().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)WorkItem;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeWorkItem);
    platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;

    // Cause the workitem callback to execute as soon as possible.
    //
    DmfPlatformHandlersTable.DmfPlatformHandlerWdfWorkItemFlush(platformWorkItem);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFCOLLECTION
///////////////////////////////////////////////////////////////////////////////////
//

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfCollectionCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* CollectionAttributes,
    _Out_ WDFCOLLECTION* Collection
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(NULL,
                                                    CollectionAttributes,
                                                    sizeof(DMF_PLATFORM_COLLECTION),
                                                    DmfPlatformObjectTypeCollection);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_COLLECTION* platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    WDF_OBJECT_ATTRIBUTES spinlockAttributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&spinlockAttributes);
    ntStatus = WdfSpinLockCreate(&spinlockAttributes,
                                 &platformCollection->ListLock);
    if (!NT_SUCCESS(ntStatus))
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "WdfSpinLockCreate fails: ntStatus=0x08X", ntStatus);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject->Data);
        DmfPlatformHandlersTable.DmfPlatformHandlerFree(platformObject);
        goto Exit;
    }

    InitializeListHead(&platformCollection->List);
    platformCollection->ItemCount = 0;

    *Collection = (WDFCOLLECTION)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG
DmfPlatform_WdfCollectionGetCount(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFCOLLECTION Collection
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionGetCount().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    The number of items in the collection.

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformcollection;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformcollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    return platformcollection->ItemCount;
}

// List entry for each element in the collection.
//
typedef struct
{
    // Allows insertion into the list.
    //
    LIST_ENTRY ListEntry;
    // Key of item added into the list.
    //
    WDFOBJECT Object;
} COLLECTION_ENTRY;

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfCollectionAdd(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFCOLLECTION Collection,
    _In_ WDFOBJECT Object
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionAdd().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    Object - DMF Platform object.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    collectionEntry = (COLLECTION_ENTRY*)DmfPlatformHandlersTable.DmfPlatformHandlerAllocate(sizeof(COLLECTION_ENTRY));
    if (NULL == collectionEntry)
    {
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    collectionEntry->Object = Object;

    WdfSpinLockAcquire(platformCollection->ListLock);

    InitializeListHead(&collectionEntry->ListEntry);

    InsertTailList(&platformCollection->List,
                   &collectionEntry->ListEntry);
    platformCollection->ItemCount++;

    WdfSpinLockRelease(platformCollection->ListLock);

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfCollectionRemove(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFCOLLECTION Collection,
    _In_ WDFOBJECT Item
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionRemove().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    listEntry = platformCollection->List.Flink;
    while (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        collectionEntry = CONTAINING_RECORD(listEntry,
                                            COLLECTION_ENTRY,
                                            ListEntry);
        if (collectionEntry->Object == Item)
        {
            RemoveEntryList(listEntry);
            platformCollection->ItemCount--;
            break;
        }
        else
        {
            listEntry = listEntry->Flink;
        }
    }

    WdfSpinLockRelease(platformCollection->ListLock);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfCollectionRemoveItem(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFCOLLECTION Collection,
    _In_ ULONG Index
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionRemoveItem().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;
    ULONG currentItemIndex;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    currentItemIndex = 0;
    listEntry = platformCollection->List.Flink;
    while (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        if (currentItemIndex == Index)
        {
            collectionEntry = CONTAINING_RECORD(listEntry,
                                                COLLECTION_ENTRY,
                                                ListEntry);
            RemoveEntryList(listEntry);
            platformCollection->ItemCount--;
            break;
        }
        else
        {
            currentItemIndex++;
            listEntry = listEntry->Flink;
        }
    }

    WdfSpinLockRelease(platformCollection->ListLock);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfCollectionGetItem(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFCOLLECTION Collection,
    _In_ ULONG Index
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionGetItem().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    The returned item at the zero-based index in the list (which still remains in the collection).

--*/
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;
    ULONG currentItemIndex;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    returnValue = NULL;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    currentItemIndex = 0;
    listEntry = platformCollection->List.Flink;
    while (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        if (currentItemIndex == Index)
        {
            collectionEntry = CONTAINING_RECORD(listEntry,
                                                COLLECTION_ENTRY,
                                                ListEntry);
            returnValue = collectionEntry->Object;
            break;
        }
        else
        {
            currentItemIndex++;
            listEntry = listEntry->Flink;
        }
    }

    WdfSpinLockRelease(platformCollection->ListLock);

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfCollectionGetFirstItem(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFCOLLECTION Collection
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionGetFirstItem().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    The returned item at the first index in the list (which still remains in the collection).

--*/
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    returnValue = NULL;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    listEntry = platformCollection->List.Flink;
    if (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        collectionEntry = CONTAINING_RECORD(listEntry,
                                            COLLECTION_ENTRY,
                                            ListEntry);
        returnValue = collectionEntry->Object;
    }
    else
    {
        returnValue = NULL;
    }

    WdfSpinLockRelease(platformCollection->ListLock);
    
    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFOBJECT
DmfPlatform_WdfCollectionGetLastItem(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFCOLLECTION Collection
    )
/*++

Routine Description:

    Platform implementation of WdfCollectionGetLastItem().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    The returned item at the last index in the list (which still remains in the collection).

--*/
{
    WDFOBJECT returnValue;
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_COLLECTION* platformCollection;
    COLLECTION_ENTRY* collectionEntry;
    LIST_ENTRY* listEntry;

    UNREFERENCED_PARAMETER(DriverGlobals);

    platformObject = (DMF_PLATFORM_OBJECT*)Collection;
    DmfAssert(platformObject->PlatformObjectType == DmfPlatformObjectTypeCollection);
    platformCollection = (DMF_PLATFORM_COLLECTION*)platformObject->Data;

    returnValue = NULL;

    WdfSpinLockAcquire(platformCollection->ListLock);
    
    listEntry = platformCollection->List.Blink;
    if (listEntry != &platformCollection->List)
    {
        DmfAssert(platformCollection->ItemCount > 0);
        collectionEntry = CONTAINING_RECORD(listEntry,
                                            COLLECTION_ENTRY,
                                            ListEntry);
        returnValue = collectionEntry->Object;
    }
    else
    {
        returnValue = NULL;
    }

    WdfSpinLockRelease(platformCollection->ListLock);
    
    return returnValue;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFDEVICE
///////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetPnpPowerEventCallbacks(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ PWDFDEVICE_INIT DeviceInit,
    _In_ WDF_PNPPOWER_EVENT_CALLBACKS* PnpPowerEventCallbacks
    )
/*++

Routine Description:

    Platform implementation of WdfDeviceInitSetPnpPowerEventCallbacks().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(PnpPowerEventCallbacks);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetPowerPolicyEventCallbacks(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ PWDFDEVICE_INIT DeviceInit,
    _In_ WDF_POWER_POLICY_EVENT_CALLBACKS* PowerPolicyEventCallbacks
    )
/*++

Routine Description:

    Platform implementation of WdfDeviceInitSetPowerPolicyEventCalbacks().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(PowerPolicyEventCallbacks);
}

_Must_inspect_result_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS
DmfPlatform_WdfDeviceCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _Inout_ PWDFDEVICE_INIT* DeviceInit,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* DeviceAttributes,
    _Out_ WDFDEVICE* Device
    )
/*++

Routine Description:

    Platform implementation of WdfDeviceCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(NULL,
                                                    DeviceAttributes,
                                                    sizeof(DMF_PLATFORM_DEVICE),
                                                    DmfPlatformObjectTypeDevice);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    // TODO: Possible future support.
    //
    // DMF_PLATFORM_DEVICE* platformDevice = (DMF_PLATFORM_DEVICE*)platformObject->Data;

    *Device = (WDFDEVICE)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetFileObjectConfig(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ PWDFDEVICE_INIT DeviceInit,
    _In_ WDF_FILEOBJECT_CONFIG* FileObjectConfig,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* FileObjectAttributes
    )
/*++

Routine Description:

    Platform implementation of WdfDeviceInitSetfileObjectConfig().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(FileObjectConfig);
    UNREFERENCED_PARAMETER(FileObjectAttributes);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetCharacteristics(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ PWDFDEVICE_INIT DeviceInit,
    _In_ ULONG DeviceCharacteristics,
    _In_ BOOLEAN OrInValues
    )
/*++

Routine Description:

    Platform implementation of WdfDeviceInitSetCharacteristics().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(DeviceCharacteristics);
    UNREFERENCED_PARAMETER(OrInValues);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfDeviceInitSetDeviceClass(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ PWDFDEVICE_INIT DeviceInit,
    _In_ CONST GUID* DeviceClassGuid
    )
/*++

Routine Description:

    Platform implementation of WdfDeviceInitSetDeviceClass().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(DeviceInit);
    UNREFERENCED_PARAMETER(DeviceClassGuid);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFIOQUEUE
///////////////////////////////////////////////////////////////////////////////////
//

// NOTE: There is only dummy support for create function to prevent need to make
//       many changes in the rest of the code.
//       Once there is a platform that can make use of a queue, then this support
//       can be added.
//

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
DmfPlatform_WdfIoQueueCreate(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFDEVICE Device,
    _In_ WDF_IO_QUEUE_CONFIG* Config,
    _In_opt_ WDF_OBJECT_ATTRIBUTES* QueueAttributes,
    _Out_opt_ WDFQUEUE* Queue
    )
/*++

Routine Description:

    Platform implementation of WdfIoQueueCreate().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    NTSTATUS

--*/
{
    NTSTATUS ntStatus;
    DMF_PLATFORM_OBJECT* platformObject;

    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(Device);

    ntStatus = STATUS_UNSUCCESSFUL;

    platformObject = DmfPlatform_ObjectCreateProlog(NULL,
                                                    QueueAttributes,
                                                    sizeof(DMF_PLATFORM_QUEUE),
                                                    DmfPlatformObjectTypeQueue);
    if (platformObject == NULL)
    {
        TraceEvents(TRACE_LEVEL_ERROR, DMF_TRACE, "DmfPlatform_ObjectCreateProlog fails: ntStatus=0x08X", ntStatus);
        goto Exit;
    }

    DMF_PLATFORM_QUEUE* platformQueue = (DMF_PLATFORM_QUEUE*)platformObject->Data;

    memcpy(&platformQueue->Config,
           Config,
           sizeof(WDF_IO_QUEUE_CONFIG));

    *Queue = (WDFQUEUE)platformObject;

    ntStatus = STATUS_SUCCESS;

Exit:

    return ntStatus;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DmfPlatform_WdfIoQueueGetDevice(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFQUEUE Queue
    )
/*++

Routine Description:

    Platform implementation of WdfIoQueueGetDevice().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    WDFDEVICE that is the parent of the given queue.

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(Queue);

    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////
// WDFREQUEST
///////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
DmfPlatform_WdfRequestComplete(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFREQUEST Request,
    _In_ NTSTATUS Status
    )
/*++

Routine Description:

    Platform implementation of WdfRequestComplete().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    None

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(Request);
    UNREFERENCED_PARAMETER(Status);
}

///////////////////////////////////////////////////////////////////////////////////
// WDFFILEOBJECT
///////////////////////////////////////////////////////////////////////////////////
//

_IRQL_requires_max_(DISPATCH_LEVEL)
WDFDEVICE
DmfPlatform_WdfFileObjectGetDevice(
    _In_ WDF_DRIVER_GLOBALS* DriverGlobals,
    _In_ WDFFILEOBJECT FileObject
    )
/*++

Routine Description:

    Platform implementation of WdfFileObjectGetDevice().

Arguments:

    DriverGlobals - Internal platform data for possible future use.
    (See MSDN for meanings of the rest of the parameters.)

Return Value:

    WDFDEVICE that is the parent of the given file object.

--*/
{
    UNREFERENCED_PARAMETER(DriverGlobals);
    UNREFERENCED_PARAMETER(FileObject);

    return NULL;
}

#elif defined(DMF_USER_MODE)
    // Use WDF directly.
    //
#elif !defined(DMF_USER_MODE)
    // Use WDF directly.
    //
#endif

///////////////////////////////////////////////////////////////////////////////////
// Initialization
///////////////////////////////////////////////////////////////////////////////////
//

void
DmfPlaformNotImplemented(
    void
    )
/*++

Routine Description:

    Default handler for all WDF APIs that are not yet implemented. It tells the caller
    that the code for that API needs to be written.

Arguments:

    None

Return Value:

    None

--*/
{
    DmfAssert(FALSE);
}

// Table of all function pointers.
//
WDFFUNC WdfFunctions[WdfFunctionTableNumEntries] = 
{
    // Defaults to NULL.
    //
};

WDF_DRIVER_GLOBALS WdfDriverGlobalsBuffer;
WDF_DRIVER_GLOBALS* WdfDriverGlobals = &WdfDriverGlobalsBuffer;
// FALSE = "All functions are always available".
//
BOOLEAN WdfClientVersionHigherThanFramework = FALSE;
ULONG WdfFunctionCount= WdfFunctionTableNumEntries;
ULONG WdfStructureCount= WDF_STRUCTURE_TABLE_NUM_ENTRIES;

void
DMF_PlatformInitialize(
    _In_ DMF_PLATFORM_PARAMETERS* DmfPlatformParameters
    )
/*++

Routine Description:

    Initializes the table of functions that allow WDF calls to be routed to the
    internal support. Also calls the platform specific initialization callback.

Arguments:

    None

Return Value:

    None

--*/
{
    ULONG functionTableIndex;

    // Set default function for all functions to "not implemented".
    //
    for (functionTableIndex = 0; functionTableIndex < WdfFunctionTableNumEntries; functionTableIndex++)
    {
        switch (functionTableIndex)
        {
            // WDFSYNC
            //
            case WdfWaitLockCreateTableIndex:
                WdfFunctions[WdfWaitLockCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfWaitLockCreate;
                break;
            case WdfWaitLockAcquireTableIndex:
                WdfFunctions[WdfWaitLockAcquireTableIndex] = (WDFFUNC)DmfPlatform_WdfWaitLockAcquire;
                break;
            case WdfWaitLockReleaseTableIndex:
                WdfFunctions[WdfWaitLockReleaseTableIndex] = (WDFFUNC)DmfPlatform_WdfWaitLockRelease;
                break;
            case WdfSpinLockCreateTableIndex:
                WdfFunctions[WdfSpinLockCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfSpinLockCreate;
                break;
            case WdfSpinLockAcquireTableIndex:
                WdfFunctions[WdfSpinLockAcquireTableIndex] = (WDFFUNC)DmfPlatform_WdfSpinLockAcquire;
                break;
            case WdfSpinLockReleaseTableIndex:
                WdfFunctions[WdfSpinLockReleaseTableIndex] = (WDFFUNC)DmfPlatform_WdfSpinLockRelease;
                break;
            // WDFOBJECT
            //
            case WdfObjectDeleteTableIndex:
                WdfFunctions[WdfObjectDeleteTableIndex] = (WDFFUNC)DmfPlatform_WdfObjectDelete;
                break;
            case WdfObjectGetTypedContextWorkerTableIndex:
                WdfFunctions[WdfObjectGetTypedContextWorkerTableIndex] = (WDFFUNC)DmfPlatform_WdfObjectGetTypedContextWorker;
                break;
            case WdfObjectAllocateContextTableIndex:
                WdfFunctions[WdfObjectAllocateContextTableIndex] = (WDFFUNC)DmfPlatform_WdfObjectAllocateContext;
                break;
            // WDFDEVICE
            //
            case WdfDeviceCreateTableIndex:
                WdfFunctions[WdfDeviceCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceCreate;
                break;
            case WdfDeviceInitSetPnpPowerEventCallbacksTableIndex:
                WdfFunctions[WdfDeviceInitSetPnpPowerEventCallbacksTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceInitSetPnpPowerEventCallbacks;
                break;
            case WdfDeviceInitSetPowerPolicyEventCallbacksTableIndex:
                WdfFunctions[WdfDeviceInitSetPowerPolicyEventCallbacksTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceInitSetPowerPolicyEventCallbacks;
                break;
            // WDFMEMORY
            //
            case WdfMemoryCreateTableIndex:
                WdfFunctions[WdfMemoryCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfMemoryCreate;
                break;
            case WdfMemoryCreatePreallocatedTableIndex:
                WdfFunctions[WdfMemoryCreatePreallocatedTableIndex] = (WDFFUNC)DmfPlatform_WdfMemoryCreatePreallocated;
                break;
            case WdfMemoryGetBufferTableIndex:
                WdfFunctions[WdfMemoryGetBufferTableIndex] = (WDFFUNC)DmfPlatform_WdfMemoryGetBuffer;
                break;
            // WDFIOQUEUE
            //
            case WdfIoQueueCreateTableIndex:
                WdfFunctions[WdfIoQueueCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfIoQueueCreate;
                break;
            case WdfIoQueueGetDeviceTableIndex:
                WdfFunctions[WdfIoQueueGetDeviceTableIndex] = (WDFFUNC)DmfPlatform_WdfIoQueueGetDevice;
                break;
            // WDFFILEOBJECT
            //
            case WdfFileObjectGetDeviceTableIndex:
                WdfFunctions[WdfFileObjectGetDeviceTableIndex] = (WDFFUNC)DmfPlatform_WdfFileObjectGetDevice;
                break;
            case WdfDeviceInitSetFileObjectConfigTableIndex:
                WdfFunctions[WdfDeviceInitSetFileObjectConfigTableIndex] = (WDFFUNC)DmfPlatform_WdfDeviceInitSetFileObjectConfig;
                break;
            // WDFCOLLECTION
            //
            case WdfCollectionCreateTableIndex:
                WdfFunctions[WdfCollectionCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionCreate;
                break;
            case WdfCollectionGetCountTableIndex:
                WdfFunctions[WdfCollectionGetCountTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetCount;
                break;
            case WdfCollectionAddTableIndex:
                WdfFunctions[WdfCollectionAddTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionAdd;
                break;
            case WdfCollectionRemoveTableIndex:
                WdfFunctions[WdfCollectionRemoveTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionRemove;
                break;
            case WdfCollectionRemoveItemTableIndex:
                WdfFunctions[WdfCollectionRemoveItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionRemoveItem;
                break;
            case WdfCollectionGetItemTableIndex:
                WdfFunctions[WdfCollectionGetItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetItem;
                break;
            case WdfCollectionGetFirstItemTableIndex:
                WdfFunctions[WdfCollectionGetFirstItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetLastItem;
                break;
            case WdfCollectionGetLastItemTableIndex:
                WdfFunctions[WdfCollectionGetLastItemTableIndex] = (WDFFUNC)DmfPlatform_WdfCollectionGetLastItem;
                break;
            // WDFTIMER
            //
            case WdfTimerCreateTableIndex:
                WdfFunctions[WdfTimerCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerCreate;
                break;
            case WdfTimerStartTableIndex:
                WdfFunctions[WdfTimerStartTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerStart;
                break;
            case WdfTimerStopTableIndex:
                WdfFunctions[WdfTimerStopTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerStop;
                break;
            case WdfTimerGetParentObjectTableIndex:
                WdfFunctions[WdfTimerGetParentObjectTableIndex] = (WDFFUNC)DmfPlatform_WdfTimerGetParentObject;
                break;
            // WDFWORKITEM
            //
            case WdfWorkItemCreateTableIndex:
                WdfFunctions[WdfWorkItemCreateTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemCreate;
                break;
            case WdfWorkItemEnqueueTableIndex:
                WdfFunctions[WdfWorkItemEnqueueTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemEnqueue;
                break;
            case WdfWorkItemGetParentObjectTableIndex:
                WdfFunctions[WdfWorkItemGetParentObjectTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemGetParentObject;
                break;
            case WdfWorkItemFlushTableIndex:
                WdfFunctions[WdfWorkItemFlushTableIndex] = (WDFFUNC)DmfPlatform_WdfWorkItemFlush;
                break;
            // NOT IMPLEMENTED
            //
            default:
            {
                WdfFunctions[functionTableIndex] = DmfPlaformNotImplemented;
                break;
            }
        }
    }

    // Perform platform specific initialization.
    //
    DmfPlatformHandlersTable.DmfPlatformHandlerInitialize(DmfPlatformParameters);
}

void
DMF_PlatformUninitialize(
    _In_ WDFDEVICE WdfDevice
    )
/*++

Routine Description:

    Performs uninitialization including freeing all memory allocated by the given
    parent object and its children.

Arguments:

    WdfDevice - The given parent object.

Return Value:

    None

--*/
{
    if (WdfDevice != NULL)
    {
        WdfObjectDelete(WdfDevice);
    }

    // Perform platform specific uninitialization.
    //
    DmfPlatformHandlersTable.DmfPlatformHandlerUninitialize();
}

///////////////////////////////////////////////////////////////////////////////////
// Tracing
///////////////////////////////////////////////////////////////////////////////////
//

// TODO: Convert %!STATUS! to 0x%08X.
//

VOID
TraceEvents(
    _In_ ULONG DebugPrintLevel,
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ PCSTR DebugMessage,
    ...
    )
/*++

Routine Description:

    Outputs logging information.

Arguments:

    DebugPrintLevel - The message level.
    DebugPrintFlag - The message flag.
    DebugMessageFormat - Printf format string.
    ... - Arguments to output formatted by DebugMessageFormat.

Return Value:

    None

--*/
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    DmfPlatformHandlersTable.DmfPlatformHandlerTraceEvents(DebugPrintLevel,
                                                           DebugPrintFlag,
                                                           DebugMessage,
                                                           argumentList);
    va_end(argumentList);
}

VOID
TraceInformation(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ PCSTR DebugMessage,
    ...
    )
/*++

Routine Description:

    Outputs logging information at TRACE_LEVEL_INFORMATION.

Arguments:

    DebugPrintFlag - The message flag.
    DebugMessageFormat - Printf format string.
    ... - Arguments to output formatted by DebugMessageFormat.

Return Value:

    None

--*/
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    DmfPlatformHandlersTable.DmfPlatformHandlerTraceEvents(TRACE_LEVEL_INFORMATION,
                                                           DebugPrintFlag,
                                                           DebugMessage,
                                                           argumentList);
    va_end(argumentList);
}

VOID
TraceVerbose(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ PCSTR DebugMessage,
    ...
    )
/*++

Routine Description:

    Outputs logging information at TRACE_LEVEL_VERBOSE.

Arguments:

    DebugPrintFlag - The message flag.
    DebugMessageFormat - Printf format string.
    ... - Arguments to output formatted by DebugMessageFormat.

Return Value:

    None

--*/
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    DmfPlatformHandlersTable.DmfPlatformHandlerTraceEvents(TRACE_LEVEL_VERBOSE,
                                                           DebugPrintFlag,
                                                           DebugMessage,
                                                           argumentList);
    va_end(argumentList);
}

VOID
TraceError(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ PCSTR DebugMessage,
    ...
    )
/*++

Routine Description:

    Outputs logging information at TRACE_LEVEL_ERROR.

Arguments:

    DebugPrintFlag - The message flag.
    DebugMessageFormat - Printf format string.
    ... - Arguments to output formatted by DebugMessageFormat.

Return Value:

    None

--*/
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    DmfPlatformHandlersTable.DmfPlatformHandlerTraceEvents(TRACE_LEVEL_ERROR,
                                                           DebugPrintFlag,
                                                           DebugMessage,
                                                           argumentList);
    va_end(argumentList);
}

VOID
FuncEntryArguments(
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ PCSTR DebugMessage,
    ...
    )
/*++

Routine Description:

    Outputs logging information with arguments during function entry at TRACE_LEVEL_VERBOSE.

Arguments:

    DebugPrintFlag - The message flag.
    DebugMessageFormat - Printf format string.
    ... - Arguments to output formatted by DebugMessageFormat.

Return Value:

    None

--*/
{
    va_list argumentList;

    va_start(argumentList,
             DebugMessage);

    TraceEvents(TRACE_LEVEL_VERBOSE,
                DebugPrintFlag,
                DebugMessage,
                argumentList);

    va_end(argumentList);
}

BOOLEAN
DmfPlatform_FormatStringTranslate(
    _In_ PCSTR DebugMessage,
    _Out_writes_(OutStringSize) CHAR* OutString,
    _In_ size_t OutStringSize
    )
/*++

Routine Description:

    Replace special WPP tracing format specifiers with 0x%X so that printf
    can output the values.

Arguments:

    DebugMessage - Original format message to filter.
    OutString - Translated format message.
    OutStringSize - Size in bytes of OutString buffer.

Return Value:

    TRUE if the OutString buffer is large enough.

--*/
{
    BOOLEAN returnValue;
    CHAR* currentCharacterIn;
    CHAR* currentCharacterOut;
    size_t remainingBytes;
    CHAR replacementFormat[] = "0x%X";
    size_t replacementFormatSize;

    if (strlen(DebugMessage) + sizeof(CHAR) > OutStringSize)
    {
        strcpy_s(OutString,
                 OutStringSize,
                 "Format string is too long.");
        returnValue = FALSE;
        goto Exit;
    }

    replacementFormatSize = strlen(replacementFormat);
    currentCharacterIn = (CHAR*)DebugMessage;
    currentCharacterOut = OutString;
    remainingBytes = OutStringSize;
    while (*currentCharacterIn)
    {
        if (*currentCharacterIn == '%')
        {
            if (*(currentCharacterIn + 1) == '!')
            {
                // Special string.
                //

                // Skip '%'
                //
                currentCharacterIn++;
                // Skip '!'
                //
                currentCharacterIn++;
                // Skip characters inside '!' and '!'.
                //
                while ((*currentCharacterIn) &&
                        (*currentCharacterIn != '!'))
                {
                    currentCharacterIn++;
                }
                if (*currentCharacterIn == '!')
                {
                    // Skip trailing !''.
                    //
                    currentCharacterIn++;
                }
                else
                {
                    // It is a malformed string that starts with %! but
                    // has no trailing !. Just overwrite target buffer 
                    // with error message and get out.
                    //
                    strcpy_s(OutString,
                             OutStringSize,
                             "Error in format string: expected trailing '!'");
                    returnValue = FALSE;
                    goto Exit;
                }
                // Replace with default format string.
                //
                strcat_s(currentCharacterOut,
                         remainingBytes,
                         replacementFormat);
                if (remainingBytes >= replacementFormatSize)
                {
                    remainingBytes -= replacementFormatSize;
                }
                else
                {
                    // It is a malformed string where there is not enough
                    // space in the target buffer for the rest of the
                    // translated string.
                    //
                    strcpy_s(OutString,
                             OutStringSize,
                             "Error in format string: not enough space");
                    returnValue = FALSE;
                    goto Exit;
                }
                currentCharacterOut += replacementFormatSize;
                continue;
            }
            else
            {
                // Not a special string...fall through and keep copying.
                //
            }
        }
        // Copy the current compatible character.
        //
        *currentCharacterOut = *currentCharacterIn;
        currentCharacterOut++;
        remainingBytes--;
        currentCharacterIn++;
    }
    // Zero terminate the string.
    //
    *currentCharacterOut = '\0';
    // Indicate the original string was translated.
    //
    returnValue = TRUE;

Exit:

    return returnValue;
}

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform.c
//
