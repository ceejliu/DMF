/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfPlatform_Win32.c

Abstract:

    Win32 specific platform support for lower edge of DMF.

    NOTE: Make sure to set "compile as C++" option.
    NOTE: Make sure to #define DMF_USER_MODE in UMDF Drivers.

Environment:

    Win32 Application

--*/

#include "DmfIncludeInternal.h"

#if defined(__cplusplus)
extern "C" 
{
#endif // defined(__cplusplus)

#if !defined(DMF_WIN32_MODE)
#error Must defined DMF_WIN32_MODE to include this file.
#endif

////////////////////////////////////////////////////////////////////////////
// Win32 Primitives for Memory Allocation
////////////////////////////////////////////////////////////////////////////
//

void*
DmfPlatformHandlerAllocate_Win32(
    _In_ size_t Size
    )
/*++

Routine Description:

    Allocates memory and sets it contents to zero.

Arguments:

    Amount of memory to allocate in bytes.

Return Value:

    Pointer to allocated memory or NULL if memory could not be allocated.

--*/
{
    void* returnValue;

    returnValue = malloc(Size);
    if (returnValue != NULL)
    {
        ZeroMemory(returnValue,
                   Size);
    }

    return returnValue;
}

void
DmfPlatformHandlerFree_Win32(
    _In_ void* Pointer
    )
/*++

Routine Description:

    Frees memory given an address of allocated memory.

Arguments:

    Pointer - The given address of memory to free.

Return Value:

    None

--*/
{
    free(Pointer);
}

////////////////////////////////////////////////////////////////////////////
// Win32 Primitives for Timer, Workitem, Locks
////////////////////////////////////////////////////////////////////////////
//

////////////////////////////////////////////////////////////////////////////
// Timer
//

// 'The pointer argument 'Instance' for function 'TimerCallback' can be marked as a pointer to const (con.3).'
//
#pragma warning(suppress:26461)
VOID 
CALLBACK 
TimerCallback(
    _In_ PTP_CALLBACK_INSTANCE Instance,
    _In_ VOID* Parameter,
    _In_ PTP_TIMER Timer
    )
/*++

Routine Description:

    timer callback function. Retrieve DMF Platform handle and call
    Platform's corresponding WDF callback.

Arguments:

    Instance - Platform specific data for the timer.
    Parameter - Caller's context.
    Timer - Timer handle of timer that has expired causing this callback.

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_TIMER* platformTimer;

    UNREFERENCED_PARAMETER(Timer);
    UNREFERENCED_PARAMETER(Instance);

    // Parameter should never be NULL. If it is, let it blow up and let us fix it.
    //
    platformObject = static_cast<DMF_PLATFORM_OBJECT*>(Parameter);
    platformTimer = (DMF_PLATFORM_TIMER*)platformObject->Data;

    // Call corresponding WDF timer callback passing WDFTIMER.
    //
    platformTimer->Config.EvtTimerFunc((WDFTIMER)platformObject);

    // Restart timer if needed.
    //
    if (platformTimer->Config.Period > 0)
    {
        WdfTimerStart((WDFTIMER)platformObject,
                      platformTimer->Config.Period);
    }
}

BOOLEAN
WdfTimerCreate_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
/*++

Routine Description:

    Create timer and assign Platform object so its timer-expiration
    callback can call the Platform object's corresponding WDF callback.

Arguments:

    PlatformTimer - Container where platform specific timer data is
                    initialized.
    PlatformObject - Parent of PlatformTimer.

Return Value:

    TRUE if timer is created; FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;

    PlatformTimer->PtpTimer = CreateThreadpoolTimer(TimerCallback,
                                                    (PVOID)PlatformObject,
                                                    nullptr);
    if (PlatformTimer->PtpTimer != NULL)
    {
        returnValue = TRUE;
    }
    else
    {
        returnValue = FALSE;
    }

    return returnValue;
}

_IRQL_requires_max_(DISPATCH_LEVEL)
BOOLEAN
WdfTimerStart_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ LONGLONG DueTime
    )
/*++

Routine Description:

    Create timer and assign Platform object so its timer-expiration
    callback can call the Platform object's corresponding WDF callback.

Arguments:

    PlatformTimer - Container with Win32 specific timer data.
    DueTime - Relative time in milliseconds when timer should expire.

Return Value:

    FALSE
    TODO: Return TRUE if timer was already in the queue.

--*/
{
    ULARGE_INTEGER dueTimeInteger;
    FILETIME dueTimeFile;

    dueTimeInteger.QuadPart = DueTime;
    dueTimeFile.dwHighDateTime = dueTimeInteger.HighPart;
    dueTimeFile.dwLowDateTime = dueTimeInteger.LowPart;

    SetThreadpoolTimer(PlatformTimer->PtpTimer,
                       &dueTimeFile,
                       0,
                       0);

    // Always tell caller timer was not in queue.
    //
    return FALSE;
}

BOOLEAN
WdfTimerStop_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer,
    _In_ BOOLEAN Wait
    )
/*++

Routine Description:

    Cancel an timer that has potentially started.

Arguments:

    PlatformTimer - Container with Win32 specific timer data.
    Wait - TRUE if this call waits for timer expiration callback to finish execution (if it
           has already started.)

Return Value:

    TRUE
    TODO: Return FALSE if timer was not yet started.

--*/
{
    SetThreadpoolTimer(PlatformTimer->PtpTimer,
                       NULL,
                       0,
                       0);
    if (Wait)
    {
        WaitForThreadpoolTimerCallbacks(PlatformTimer->PtpTimer, 
                                        TRUE);
    }

    return TRUE;
}

void
WdfTimerDelete_Win32(
    _In_ struct _DMF_PLATFORM_TIMER* PlatformTimer
    )
/*++

Routine Description:

    Delete timer.

Arguments:

    PlatformTimer - Container with Win32 specific timer data.

Return Value:

    None

--*/
{
    // Stop timer and wait in case it has started.
    //
    WdfTimerStop_Win32(PlatformTimer,
                       TRUE);
    // Delete the timer.
    //
    CloseThreadpoolTimer(PlatformTimer->PtpTimer);
    PlatformTimer->PtpTimer = NULL;
}

////////////////////////////////////////////////////////////////////////////
// Workitem
//

_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID
WorkitemCallback(
    _In_ WDFTIMER Timer
    )
/*++

Routine Description:

    Workitem callback function. Retrieve DMF Platform handle and call
    Platform's corresponding WDF callback.

Arguments:

    Timer - Timer handle of timer that has expired causing this callback.

Return Value:

    None

--*/
{
    DMF_PLATFORM_OBJECT* platformObject;
    DMF_PLATFORM_WORKITEM* platformWorkItem;

    platformObject = (DMF_PLATFORM_OBJECT*)WdfTimerGetParentObject(Timer);;
    platformWorkItem = (DMF_PLATFORM_WORKITEM*)platformObject->Data;
    // Call corresponding WDF workitem callback.
    //
    platformWorkItem->Config.EvtWorkItemFunc((WDFWORKITEM)platformObject);
}

BOOLEAN
WdfWorkItemCreate_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem,
    _In_ DMF_PLATFORM_OBJECT* PlatformObject
    )
/*++

Routine Description:

    Create workitem and assign Platform object so its timer-expiration
    callback can call the Platform object's corresponding WDF callback.

    NOTE: Workitem simply uses a timer callback since it uses threadpool anyway.

Arguments:

    PlatformWorkItem - Container where platform specific workitem data is
                       initialized.
    PlatformObject - Parent of PlatformWorkItem.

Return Value:

    TRUE if workitem is created; FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;
    WDF_TIMER_CONFIG timerConfig;
    NTSTATUS ntStatus;

    WDF_TIMER_CONFIG_INIT(&timerConfig, 
                          WorkitemCallback);
    ntStatus = WdfTimerCreate(&timerConfig,
                              &PlatformObject->ObjectAttributes,
                              &PlatformWorkItem->Timer);
    if (!NT_SUCCESS(ntStatus))
    {
        returnValue = FALSE;
        goto Exit;
    }

    returnValue = TRUE;

Exit:

    return returnValue;
}

BOOLEAN
WdfWorkItemEnqueue_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    )
/*++

Routine Description:

    Enqueue a workitem.

    NOTE: Workitem simply uses a timer callback since it uses threadpool anyway.

Arguments:

    PlatformWorkItem - Container with Win32 specific workitem data.

Return Value:

    TRUE

--*/
{
    WdfTimerStart(PlatformWorkItem->Timer,
                  0);
    return TRUE;
}

void
WdfWorkItemFlush_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    )
/*++

Routine Description:

    Cancel a workitem that has potentially started.

Arguments:

    PlatformWorkItem - Container with Win32 specific workitem data.

Return Value:

    None

--*/
{
    WdfTimerStop(PlatformWorkItem->Timer,
                 TRUE);
}

void
WdfWorkItemDelete_Win32(
    _In_ struct _DMF_PLATFORM_WORKITEM* PlatformWorkItem
    )
/*++

Routine Description:

    Delete workitem.

Arguments:

    PlatformWorkItem - Container with Win32 specific workitem data.

Return Value:

    None

--*/
{
    WdfTimerStop(PlatformWorkItem->Timer,
                 TRUE);
}

////////////////////////////////////////////////////////////////////////////
// Waitlock
//

BOOLEAN
WdfWaitLockCreate_Win32(
    _Out_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    )
/*++

Routine Description:

    Create waitlock.

Arguments:

    PlatformWaitLock - Container where platform specific waitlock data is
                       initialized.

Return Value:

    TRUE if waitlock is created; FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;

    PlatformWaitLock->Event = CreateEvent(NULL,
                                          FALSE,
                                          TRUE,
                                          NULL);
    if (PlatformWaitLock->Event != NULL)
    {
        returnValue = TRUE;
    }
    else
    {
        returnValue = FALSE;
    }

    return returnValue;
}

DWORD
WdfWaitLockAcquire_Win32(
    _Inout_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock,
    _In_ DWORD TimeoutMs
    )
/*++

Routine Description:

    Acquire a waitlock.

Arguments:

    PlatformWaitLock - Container with Win32 specific waitlock data.

Return Value:

    DWORD - TODO

--*/
{
    DWORD returnValue;

    returnValue = WaitForSingleObject(PlatformWaitLock->Event,
                                      (DWORD)TimeoutMs);

    return returnValue;
}

VOID
WdfWaitLockRelease_Win32(
    _Inout_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    )
/*++

Routine Description:

    Release a waitlock.

Arguments:

    PlatformWaitLock - Container with Win32 specific waitlock data.

Return Value:

    None

--*/
{
    SetEvent(PlatformWaitLock->Event);
}

void
WdfWaitLockDelete_Win32(
    _Inout_ struct _DMF_PLATFORM_WAITLOCK* PlatformWaitLock
    )
/*++

Routine Description:

    Delete a waitlock.

Arguments:

    PlatformWaitLock - Container with Win32 specific waitlock data.

Return Value:

    None

--*/
{
    CloseHandle(PlatformWaitLock->Event);
    PlatformWaitLock->Event = NULL;
}

////////////////////////////////////////////////////////////////////////////
// SpinLock
//

BOOLEAN
WdfSpinLockCreate_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
/*++

Routine Description:

    Create spinlock.

Arguments:

    PlatformSpinLock - Container where platform specific spinlock data is
                       initialized.

Return Value:

    TRUE if spinlock is created; FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;

    InitializeCriticalSection(&PlatformSpinLock->SpinLock);

    returnValue = TRUE;

    return returnValue;
}

_Acquires_lock_(PlatformSpinLock->SpinLock)
VOID
WdfSpinLockAcquire_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
/*++

Routine Description:

    Acquire spinlock.

Arguments:

    PlatformSpinLock - Container where platform specific spinlock data is
                       initialized.

Return Value:

    None

--*/
{
    EnterCriticalSection(&PlatformSpinLock->SpinLock);
}

_Releases_lock_(PlatformSpinLock->SpinLock) 
VOID
WdfSpinLockRelease_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
/*++

Routine Description:

    Release spinlock.

Arguments:

    PlatformSpinLock - Container where platform specific spinlock data is
                       initialized.

Return Value:

    None

--*/
{
    LeaveCriticalSection(&PlatformSpinLock->SpinLock);
}

void
WdfSpinLockDelete_Win32(
    _Inout_ struct _DMF_PLATFORM_SPINLOCK* PlatformSpinLock
    )
/*++

Routine Description:

    Delete spinlock.

Arguments:

    PlatformSpinLock - Container where platform specific spinlock data is
                       initialized.

Return Value:

    None

--*/
{
    DeleteCriticalSection(&PlatformSpinLock->SpinLock);
}

////////////////////////////////////////////////////////////////////////////
// Lock
//

BOOLEAN
DmfPlatformHandlerLockCreate_Win32(
    _Inout_ CRITICAL_SECTION* Lock
    )
/*++

Routine Description:

    Creates a lock for internal use.

Arguments:

    Lock - CRITICAL_SECTION used internally.

Return Value:

    TRUE if the lock is created; FALSE, otherwise.

--*/
{
    BOOLEAN returnValue;

    InitializeCriticalSection(Lock);

    returnValue = TRUE;

    return returnValue;
}

_Acquires_lock_(* Lock)
void
DmfPlatformHandlerLock_Win32(
    _Inout_ CRITICAL_SECTION* Lock
    )
/*++

Routine Description:

    Acquire a given internal lock.

Arguments:

    Lock - The given internal lock.

Return Value:

    None

--*/
{
    EnterCriticalSection(Lock);
}

_Releases_lock_(* Lock)
void
DmfPlatformHandlerUnlock_Win32(
    _Inout_ CRITICAL_SECTION* Lock
    )
/*++

Routine Description:

    Release a given internal lock.

Arguments:

    Lock - The given internal lock.

Return Value:

    None

--*/
{
    LeaveCriticalSection(Lock);
}

void
DmfPlatformHandlerLockDelete_Win32(
    _Inout_ CRITICAL_SECTION* Lock
    )
/*++

Routine Description:

    Delete a given internal lock.

Arguments:

    Lock - The given internal lock.

Return Value:

    None

--*/
{
    DeleteCriticalSection(Lock);
}

////////////////////////////////////////////////////////////////////////////
// Platform Initialization
//

ULONG DmfPlatform_LoggingLevel = TRACE_LEVEL_INFORMATION;
ULONG DmfPlatform_LoggingFlags = 0xFFFFFFFF;

void
DmfPlatformHandlerInitialize_Win32(
    _In_ DMF_PLATFORM_PARAMETERS* DmfPlatformParameters
    )
/*++

Routine Description:

    Platform specific initialization. Here is where global memory needed by the
    Platform can be initialized. Also, this is where a thread is started to 
    monitor power events so that WDF callbacks can be simulated.

Arguments:

    None

Return Value:

    None

--*/
{
    // NOTE: Platform can start deamon or thread or allocate resources
    //       that are stored in global memory.
    //

    DmfPlatform_LoggingLevel = DmfPlatformParameters->TraceLoggingLevel;
    DmfPlatform_LoggingFlags = DmfPlatformParameters->TraceLoggingFlags;
}

void
DmfPlatformHandlerUninitialize_Win32(
    void
    )
/*++

Routine Description:

    Platform specific uninitialization. Release all resources acquired
    during initialization.

Arguments:

    None

Return Value:

    None

--*/
{
    // NOTE: Platform does inverse of what it did above.
    //
}

VOID
DmfPlatformHandlerTraceEvents_Win32(
    _In_ ULONG DebugPrintLevel,
    _In_ ULONG DebugPrintFlag,
    _Printf_format_string_ _In_ PCSTR DebugMessage,
    _In_ va_list ArgumentList
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
    if ((DebugPrintLevel <= DmfPlatform_LoggingLevel) &&
        (DebugPrintFlag & DmfPlatform_LoggingFlags))
    {
        CHAR translatedDebugMessage[DMF_PLATFORM_TRACE_BUFFER_SIZE];

        DmfPlatform_FormatStringTranslate(DebugMessage,
                                          translatedDebugMessage,
                                          ARRAYSIZE(translatedDebugMessage));
        vprintf_s(translatedDebugMessage,
                  ArgumentList);
        printf("\n");
    }
}

////////////////////////////////////////////////////////////////////////////
// Platform Handlers Table
//

// NOTE: This exact name is defined in each platform, but only a single 
//       instance is compiled.
//
DmfPlatform_Handlers DmfPlatformHandlersTable =
{
    DmfPlatformHandlerTraceEvents_Win32,
    DmfPlatformHandlerInitialize_Win32,
    DmfPlatformHandlerUninitialize_Win32,
    DmfPlatformHandlerAllocate_Win32,
    DmfPlatformHandlerFree_Win32,
    DmfPlatformHandlerLockCreate_Win32,
    DmfPlatformHandlerLock_Win32,
    DmfPlatformHandlerUnlock_Win32,
    DmfPlatformHandlerLockDelete_Win32,
    WdfTimerCreate_Win32,
    WdfTimerStart_Win32,
    WdfTimerStop_Win32,
    WdfTimerDelete_Win32,
    WdfWorkItemCreate_Win32,
    WdfWorkItemEnqueue_Win32,
    WdfWorkItemFlush_Win32,
    WdfWorkItemDelete_Win32,
    WdfWaitLockCreate_Win32,
    WdfWaitLockAcquire_Win32,
    WdfWaitLockRelease_Win32,
    WdfWaitLockDelete_Win32,
    WdfSpinLockCreate_Win32,
    WdfSpinLockAcquire_Win32,
    WdfSpinLockRelease_Win32,
    WdfSpinLockDelete_Win32,
};

#if defined(__cplusplus)
}
#endif // defined(__cplusplus)

// eof: DmfPlatform.c
//
