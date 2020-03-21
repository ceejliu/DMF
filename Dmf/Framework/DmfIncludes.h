/*++

    Copyright (c) Microsoft Corporation. All rights reserved.
    Licensed under the MIT license.

Module Name:

    DmfIncludes.h

Abstract:

Environment:

    Kernel-mode Driver Framework
    User-mode Driver Framework

--*/

#if defined(DMF_WIN32_MODE) 

    #include "..\Platform\DmfPlatform.h"

#else

    #define DMF_WDF_DRIVER
    #define DMF_INCLUDE_TMH

    #if defined(DMF_USER_MODE)
        // UMDF driver.
        //
        #include "DmfIncludes_USER_MODE.h"
    #else
        // KMDF driver.
        //
        #define DMF_KERNEL_MODE
        #include "DmfIncludes_KERNEL_MODE.h"
     #endif

    #include <hidusage.h>
    #include <hidpi.h>
    // NOTE: This is necessary in order to avoid redefinition errors. It is not clear why
    //       this is the case.
    //
    #define DEVPKEY_H_INCLUDED

#endif

// eof: DmfIncludes.h
//
