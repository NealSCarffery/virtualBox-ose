/* $Id: VBoxUsbRt.h 62490 2016-07-22 18:41:49Z vboxsync $ */
/** @file
 * VBox USB R0 runtime
 */
/*
 * Copyright (C) 2011-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VBoxUsbRt_h___
#define ___VBoxUsbRt_h___

#include "VBoxUsbCmn.h"
#include "../cmn/VBoxUsbIdc.h"

#define VBOXUSBRT_MAX_CFGS 4

typedef struct VBOXUSB_PIPE_INFO {
    UCHAR       EndpointAddress;
    ULONG       NextScheduledFrame;
} VBOXUSB_PIPE_INFO;

typedef struct VBOXUSB_IFACE_INFO {
    USBD_INTERFACE_INFORMATION      *pInterfaceInfo;
    VBOXUSB_PIPE_INFO               *pPipeInfo;
} VBOXUSB_IFACE_INFO;

typedef struct VBOXUSB_RT
{
    UNICODE_STRING                  IfName;

    HANDLE                          hPipe0;
    HANDLE                          hConfiguration;
    uint32_t                        uConfigValue;

    uint32_t                        uNumInterfaces;
    USB_DEVICE_DESCRIPTOR           *devdescr;
    USB_CONFIGURATION_DESCRIPTOR    *cfgdescr[VBOXUSBRT_MAX_CFGS];

    VBOXUSB_IFACE_INFO              *pVBIfaceInfo;

    uint16_t                        idVendor, idProduct, bcdDevice;
    char                            szSerial[MAX_USB_SERIAL_STRING];
    BOOLEAN                         fIsHighSpeed;

    HVBOXUSBIDCDEV                  hMonDev;
    PFILE_OBJECT                    pOwner;
} VBOXUSB_RT, *PVBOXUSB_RT;

typedef struct VBOXUSBRT_IDC
{
    PDEVICE_OBJECT pDevice;
    PFILE_OBJECT pFile;
} VBOXUSBRT_IDC, *PVBOXUSBRT_IDC;

DECLHIDDEN(NTSTATUS) vboxUsbRtGlobalsInit();
DECLHIDDEN(VOID) vboxUsbRtGlobalsTerm();

DECLHIDDEN(NTSTATUS) vboxUsbRtInit(PVBOXUSBDEV_EXT pDevExt);
DECLHIDDEN(VOID) vboxUsbRtClear(PVBOXUSBDEV_EXT pDevExt);
DECLHIDDEN(NTSTATUS) vboxUsbRtRm(PVBOXUSBDEV_EXT pDevExt);
DECLHIDDEN(NTSTATUS) vboxUsbRtStart(PVBOXUSBDEV_EXT pDevExt);

DECLHIDDEN(NTSTATUS) vboxUsbRtDispatch(PVBOXUSBDEV_EXT pDevExt, PIRP pIrp);
DECLHIDDEN(NTSTATUS) vboxUsbRtCreate(PVBOXUSBDEV_EXT pDevExt, PIRP pIrp);
DECLHIDDEN(NTSTATUS) vboxUsbRtClose(PVBOXUSBDEV_EXT pDevExt, PIRP pIrp);

#endif /* #ifndef ___VBoxUsbRt_h___ */
