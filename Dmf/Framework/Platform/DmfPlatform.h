/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPlatform.h

Abstract:

    This file determines which main include files to include for the platform.

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework
    Win32 Application

--*/


#pragma once

#if defined(__cplusplus)
extern "C" 
{
#endif // defined(__cplusplus)

#if defined(DMF_WIN32_MODE)
    // Win32 Mode uses many of the User-mode APIs.
    //
    #define DMF_USER_MODE
    #include ".\DMF_WIN32_MODE\DmfIncludes_WIN32_MODE.h"
#endif

//////////////////////////////////////////////////////////////////////////////////////
// Definitions for all non-native platforms.
//////////////////////////////////////////////////////////////////////////////////////
//

typedef struct
{
    // Logging level.
    //
    ULONG TraceLoggingLevel;
    // Logging flags.
    //
    ULONG TraceLoggingFlags;
    // Context for possible future use by platform specific code.
    //
    void* PlatformSpecificContext;
} DMF_PLATFORM_PARAMETERS;

__forceinline
void
DMF_PLATFORM_PARAMETERS_INIT(
    _In_ DMF_PLATFORM_PARAMETERS* DmfPlatFormParameters
    )
{
    DmfPlatFormParameters->TraceLoggingLevel = 0;
    DmfPlatFormParameters->TraceLoggingFlags = 0x0;
    DmfPlatFormParameters->PlatformSpecificContext = NULL;
}

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L) 
#define STATUS_ABANDONED                 ((NTSTATUS)0x00000080L)
#define STATUS_ALERTED                   ((NTSTATUS)0x00000101L)
#if !defined(STATUS_TIMEOUT)
    // TODO: I don't understand this. Need to fix it.
    //
    #define STATUS_TIMEOUT                   ((NTSTATUS)0x00000102L)
#endif
#define STATUS_BUFFER_OVERFLOW           ((NTSTATUS)0x80000005L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_INSUFFICIENT_RESOURCES    ((NTSTATUS)0xC000009AL) 
#define STATUS_INVALID_DEVICE_REQUEST    ((NTSTATUS)0xC0000010L)
#define STATUS_ACCESS_DENIED             ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL          ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_NAME_COLLISION     ((NTSTATUS)0xC0000035L)
#define STATUS_NOT_SUPPORTED             ((NTSTATUS)0xC00000BBL)
#if !defined(STATUS_INVALID_PARAMETER)
    // TODO: I don't understand this. Need to fix it.
    //
    #define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#endif
#define STATUS_INTERNAL_ERROR            ((NTSTATUS)0xC00000E5L)
#define STATUS_CANCELLED                 ((NTSTATUS)0xC0000120L)
#define STATUS_INVALID_DEVICE_STATE      ((NTSTATUS)0xC0000184L)
#define STATUS_DEVICE_PROTOCOL_ERROR     ((NTSTATUS)0xC0000186L)
#define STATUS_INVALID_BUFFER_SIZE       ((NTSTATUS)0xC0000206L)
#define STATUS_NOT_FOUND                 ((NTSTATUS)0xC0000225L)
#define NT_SUCCESS(Status)  (((NTSTATUS)(Status)) >= 0)

#define STATUS_WAIT_0                    ((DWORD   )0x00000000L)
#define STATUS_WAIT_1                    (STATUS_WAIT_0 + 1)

#define STATUS_ABANDONED_WAIT_0          ((DWORD   )0x00000080L)
#define STATUS_USER_APC                  ((DWORD   )0x000000C0L)

#define PAGED_CODE()

// Include files from WDF.
//
#include ".\FromWdf\DmfFromWdf.h"

typedef enum
{
    DmfPlatformObjectTypeUndefined,
    DmfPlatformObjectTypeMemory,
    DmfPlatformObjectTypeWaitLock,
    DmfPlatformObjectTypeSpinLock,
    DmfPlatformObjectTypeTimer,
    DmfPlatformObjectTypeWorkItem,
    DmfPlatformObjectTypeCollection,
    DmfPlatformObjectTypeDevice,
    DmfPlatformObjectTypeQueue,
} DmfPlatformObjectType;

// Forward declaration for DMF Platform Object.
//
typedef struct _DMF_PLATFORM_OBJECT_ DMF_PLATFORM_OBJECT;

// Delete callback for each type of data.
//
typedef void (*DMF_Platform_ObjectDelete)(_In_ DMF_PLATFORM_OBJECT* PlatformObject);

// NOTE: This is analogous to WDFOBJECT.
//
struct _DMF_PLATFORM_OBJECT_
{
    DmfPlatformObjectType PlatformObjectType;
    // Pointer to:
    //    DMF_PLATFORM_MEMORY
    //    DMF_PLATFORM_WAITLOCK
    //    DMF_PLATFORM_SPINLOCK
    //    DMF_PLATFORM_TIMER
    //    DMF_PLATFORM_WORKITEM
    //    DMF_PLATFORM_COLLECTION
    //    DMF_PLATFORM_DEVICE
    //    DMF_PLATFORM_QUEUE
    //
    void* Data;
    WDF_OBJECT_ATTRIBUTES ObjectAttributes;
    // Maintains a list of child objects.
    //
    LIST_ENTRY ChildList;
    LONG NumberOfChildren;
    // TODO: Need this definition to go above as DMF_PLATFORM_CRITICAL_SECTION.
    //
    CRITICAL_SECTION ChildListLock;
    // Allows this structure to be inserted into the parent's list
    // of child objects so that the tree of objects can be deleted
    // automatically when the parent is deleted.
    //
    LIST_ENTRY ChildListEntry;
    // Maintains a list of contexts.
    //
    LIST_ENTRY ContextList;
    CRITICAL_SECTION ContextListLock;
    // Reference count.
    //
    LONG ReferenceCount;
    // Deletion callback for custom object data.
    //
    DMF_Platform_ObjectDelete ObjectDelete;
};

typedef struct
{
    LIST_ENTRY ListEntry;
    WDF_OBJECT_CONTEXT_TYPE_INFO ContextTypeInfo;
    VOID* ContextData;
} DMF_PLATFORM_CONTEXT;

// NOTE: Every platform defines platform specific versions of these
//       structures.
//
struct _DMF_PLATFORM_MEMORY;
struct _DMF_PLATFORM_SPINLOCK;
struct _DMF_PLATFORM_WAITLOCK;
struct _DMF_PLATFORM_TIMER;
struct _DMF_PLATFORM_WORKITEM;
struct _DMF_PLATFORM_COLLECTION;
struct _DMF_PLATFORM_QUEUE;
struct _DMF_PLATFORM_DEVICE;

// NOTE: Every platform defines platform specific versions of these
//       functions.
//

typedef
void
(*DmfPlatformHandlerTraceEvents)(
    _In_ ULONG DebugPrintLevel,
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ PCSTR   DebugMessage,
    _In_ va_list ArgumentList
    );

#define DMF_PLATFORM_TRACE_BUFFER_SIZE 1024

BOOLEAN
DmfPlatform_FormatStringTranslate(
    _In_ PCSTR DebugMessage,
    _Out_writes_(OutStringSize) CHAR* OutString,
    _In_ size_t OutStringSize
    );

// TODO: We should return possible return a context that is used later
//       so platform can store platform specific data. (Use global memory
//       for that for now.)
//
typedef
void
(*DmfPlatformHandler_PlatformInitialize)(
    _In_ DMF_PLATFORM_PARAMETERS* DmfPlatformParameters
    );

typedef
void
(*DmfPlatformHandler_PlatformUninitialize)(
    void
    );

typedef
void*
(*DmfPlatformHandler_Allocate)(
    _In_ size_t Size
    );

typedef
void
(*DmfPlatformHandler_Free)(
    _In_ void* Pointer
    );

typedef
BOOLEAN
(*DmfPlatformHandler_LockCreate)(
    _Inout_ CRITICAL_SECTION* Lock
    );

typedef
_Acquires_lock_(* CriticalSection)
void
(*DmfPlatformHandler_Lock)(
    _Inout_ CRITICAL_SECTION* Lock
    );

typedef
_Releases_lock_(* CriticalSection)
void
(*DmfPlatformHandler_Unlock)(
    _Inout_ CRITICAL_SECTION* Lock
    );

typedef
void
(*DmfPlatformHandler_LockDelete)(
    _Inout_ CRITICAL_SECTION* Lock
    );

typedef
BOOLEAN
(*DmfPlatformHandler_WdfTimerCreate)(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    );

typedef
BOOLEAN
(*DmfPlatformHandler_WdfTimerStart)(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ LONGLONG DueTime
    );

typedef
BOOLEAN
(*DmfPlatformHandler_WdfTimerStop)(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ BOOLEAN Wait
    );

typedef
void
(*DmfPlatformHandler_WdfTimerDelete)(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer
    );

typedef
BOOLEAN
(*DmfPlatformHandler_WdfWorkItemCreate)(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    );

typedef
BOOLEAN
(*DmfPlatformHandler_WdfWorkItemEnqueue)(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    );

typedef
VOID
(*DmfPlatformHandler_WdfWorkItemFlush)(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    );

typedef
void
(*DmfPlatformHandler_WdfWorkItemDelete)(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    );

typedef
BOOLEAN
(*DmfPlatformHandler_WdfWaitLockCreate)(
    _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    );

typedef
DWORD
(*DmfPlatformHandler_WdfWaitLockAcquire)(
    _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock,
    _In_ DWORD TimeoutMs
    );

typedef
void
(*DmfPlatformHandler_WdfWaitLockRelease)(
    _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    );

typedef
void
(*DmfPlatformHandler_WdfWaitLockDelete)(
    _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    );

typedef
BOOLEAN
(*DmfPlatformHandler_WdfSpinLockCreate)(
    _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

typedef
void
(*DmfPlatformHandler_WdfSpinLockAcquire)(
    _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

typedef
void
(*DmfPlatformHandler_WdfSpinLockRelease)(
    _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

typedef
void
(*DmfPlatformHandler_WdfSpinLockDelete)(
    _Out_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    );

// Structure of all the mapped handlers.
//
typedef struct
{
    DmfPlatformHandlerTraceEvents DmfPlatformHandlerTraceEvents;
    DmfPlatformHandler_PlatformInitialize DmfPlatformHandlerInitialize;
    DmfPlatformHandler_PlatformUninitialize DmfPlatformHandlerUninitialize;
    DmfPlatformHandler_Allocate DmfPlatformHandlerAllocate;
    DmfPlatformHandler_Free DmfPlatformHandlerFree;
    DmfPlatformHandler_LockCreate DmfPlatformHandlerLockCreate;
    DmfPlatformHandler_Lock DmfPlatformHandlerLock;
    DmfPlatformHandler_Unlock DmfPlatformHandlerUnlock;
    DmfPlatformHandler_LockDelete DmfPlatformHandlerLockDelete;
    DmfPlatformHandler_WdfTimerCreate DmfPlatformHandlerWdfTimerCreate;
    DmfPlatformHandler_WdfTimerStart DmfPlatformHandlerWdfTimerStart;
    DmfPlatformHandler_WdfTimerStop DmfPlatformHandlerWdfTimerStop;
    DmfPlatformHandler_WdfTimerDelete DmfPlatformHandlerWdfTimerDelete;
    DmfPlatformHandler_WdfWorkItemCreate DmfPlatformHandlerWdfWorkItemCreate;
    DmfPlatformHandler_WdfWorkItemEnqueue DmfPlatformHandlerWdfWorkItemEnqueue;
    DmfPlatformHandler_WdfWorkItemFlush DmfPlatformHandlerWdfWorkItemFlush;
    DmfPlatformHandler_WdfWorkItemDelete DmfPlatformHandlerWdfWorkItemDelete;
    DmfPlatformHandler_WdfWaitLockCreate DmfPlatformHandlerWdfWaitLockCreate;
    DmfPlatformHandler_WdfWaitLockAcquire DmfPlatformHandlerWdfWaitLockAcquire;
    DmfPlatformHandler_WdfWaitLockRelease DmfPlatformHandlerWdfWaitLockRelease;
    DmfPlatformHandler_WdfWaitLockDelete DmfPlatformHandlerWdfWaitLockDelete;
    DmfPlatformHandler_WdfSpinLockCreate DmfPlatformHandlerWdfSpinLockCreate;
    DmfPlatformHandler_WdfSpinLockAcquire DmfPlatformHandlerWdfSpinLockAcquire;
    DmfPlatformHandler_WdfSpinLockRelease DmfPlatformHandlerWdfSpinLockRelease;
    DmfPlatformHandler_WdfSpinLockDelete DmfPlatformHandlerWdfSpinLockDelete;
} DmfPlatform_Handlers;

// NOTE: All non-windows platforms need to include a platform specific version of this file.
//
#if defined(DMF_WIN32_MODE)
    // Win32 specific WDF support needed.
    //
    #include ".\DMF_WIN32_MODE\DmfPlatform_Win32.h"

    #include <hidusage.h>
    #include <hidpi.h>
    // NOTE: This is necessary in order to avoid redefinition errors. It is not clear why
    //       this is the case.
    //
    #define DEVPKEY_H_INCLUDED
#endif

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform.h
//
