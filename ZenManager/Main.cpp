/*
    ZenFinder
    --
    Searches connected USB devices for a Cronus Zen device

    Author: swedemafia
    Last update: 24 Jan 2023
*/

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <stdio.h>
#include <tchar.h>
#include <conio.h>

#pragma comment (lib, "cfgmgr32.lib")
#pragma comment (lib, "setupapi.lib")

#ifdef DEFINE_DEVPROPKEY
#undef DEFINE_DEVPROPKEY
#endif
#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }

// DEVPROP_TYPE_STRING
DEFINE_DEVPROPKEY(DEVPKEY_Device_BusReportedDeviceDesc, 0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 4);
DEFINE_DEVPROPKEY(DEVPKEY_Device_DeviceDesc, 0xa8b865dd, 0x2e3d, 0x4094, 0xad, 0x97, 0xe5, 0x93, 0xa7, 0xc, 0x75, 0xd6, 4);

// Size of array
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// Registry identifiers (HKEY_LOCAL_MACHINE/System/CurrentControlSet/Control/usbflags/vvvvpppprrrr)
TCHAR szPid[128] = { NULL }, szVid[128] = { NULL };

// FindZen return values
enum ZenStatus {
    Zen_UnableQueryUSB = 0,
    Zen_NotFound,
    Zen_Missing_VID,
    Zen_Missing_PID,
    Zen_Failed,
    Zen_Hidden
};

// See if a Cronus Zen is present on the system
ZenStatus FindZen(void) {
    CONFIGRET crGetDeviceID;
    DEVPROPTYPE dptPropertyType;
    DWORD dwRequiredSize;
    HDEVINFO hDevInfo;
    LPTSTR pszErrorMessage, pszToken, pszNextToken = NULL;
    SP_DEVINFO_DATA DeviceInfoData;
    TCHAR szDeviceInstanceID[MAX_DEVICE_ID_LEN];
    TCHAR szBusReportedDeviceDesc[1024], szDeviceDescription[1024];

    // Search for connected USB devices
    hDevInfo = SetupDiGetClassDevs(NULL, TEXT("USB"), NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);

    // Returns INVALID_HANDLE_VALUE if no USB found
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return Zen_UnableQueryUSB;

    DeviceInfoData.cbSize = sizeof(DeviceInfoData);

    // Enumerate list of present USB devices
    for (unsigned i = 0; ; i++) {
        
        // Check if no more devices are found
        if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData)) {
            break;
        }

        // Query current device information
        crGetDeviceID = CM_Get_Device_ID(DeviceInfoData.DevInst, szDeviceInstanceID, MAX_DEVICE_ID_LEN, 0);

        if (crGetDeviceID != CR_SUCCESS) {
            // Failed to get device information
            continue;
        }

        // Search for the Cronus Zen
        if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &dptPropertyType, (PBYTE)szBusReportedDeviceDesc, sizeof(szBusReportedDeviceDesc), &dwRequiredSize, 0)) {
            
            // See if we found a Cronus Zen ("Cronus Bridge")
            if (!_tcsncmp(szBusReportedDeviceDesc, TEXT("Cronus Bridge"), lstrlen(TEXT("Cronus Bridge")))) {
                // Cronus Zen found...
                _tprintf(TEXT("Cronus Zen found...\r\n"));

                // Get Cronus Zen device information...
                if (SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC, &dptPropertyType, (PBYTE)szDeviceDescription, sizeof(szDeviceDescription), &dwRequiredSize)) {
                    _tprintf(TEXT("\tDevice Instance ID: %s\r\n"), szDeviceInstanceID);
                    _tprintf(TEXT("\tDevice Description: %s\r\n"), szDeviceDescription);
                    _tprintf(TEXT("\tBus Reported Device Description: %s\r\n"), szBusReportedDeviceDesc);

                    // Determine Cronus vendor and product ID
                    pszToken = _tcstok_s(szDeviceInstanceID, TEXT("\\#&"), &pszNextToken);
                    while (pszToken != NULL) {

                        // Check for vendor ID
                        if (!_tcsncmp(pszToken, TEXT("VID_"), lstrlen(TEXT("VID_")))) {
                            _tcscpy_s(szVid, ARRAY_SIZE(szVid), pszToken);
                            _tprintf(TEXT("\tVendor ID: %s\r\n"), szVid+lstrlen(TEXT("VID_")));

                        // Check for product ID
                        } else if (!_tcsncmp(pszToken, TEXT("PID_"), lstrlen(TEXT("PID_")))) {
                            _tcscpy_s(szPid, ARRAY_SIZE(szPid), pszToken);
                            _tprintf(TEXT("\tProduct ID: %s\r\n"), szPid + lstrlen(TEXT("PID_")));
                        }

                        pszToken = _tcstok_s(NULL, TEXT("\\#&"), &pszNextToken);
                    }

                    // See if vendor ID found
                    if (szVid[0] == '\0')
                        return Zen_Missing_VID;

                    // See if product ID found
                    if (szPid[0] == '\0')
                        return Zen_Missing_PID;
                    
                    // Attempt to hide Cronus Zen
                    _tprintf(TEXT("\nAttempting to hide Cronus Zen...\r\n"));

                    const TCHAR szBuffer[128] = TEXT("Testing");
                    DWORD dwPropertyBufferSize = lstrlen(szBuffer);

                    if (SetupDiSetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC, (PBYTE)NULL, 0)) {
                        return Zen_Hidden;
                    } else {
                        // Get system error message
                        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, GetLastError(), 0, (LPWSTR)&pszErrorMessage, 0, NULL);

                        _tprintf(TEXT("\tSystem error: %s\r\n"), pszErrorMessage);
                    }

                    return Zen_Failed;
                }
            }
        }


    }

	return Zen_NotFound;
}

int _tmain(int argc, _TCHAR* argv[]) {

    // Search for a Cronus Zen and attempt to patch it
    switch (FindZen()) {
        case Zen_UnableQueryUSB:
            _tprintf(TEXT("FAILED: unable to query USB devices on system!\r\n"));
            break;
        case Zen_NotFound:
            _tprintf(TEXT("FAILED: unable to locate a Cronus Zen device!\r\n"));
            break;
        case Zen_Missing_VID:
            _tprintf(TEXT("FAILED: unable to identify Cronus Zen vendor ID!\r\n"));
            break;
        case Zen_Missing_PID:
            _tprintf(TEXT("FAILED: unable to identify Cronus Zen product ID!\r\n"));
            break;
        case Zen_Failed:
            _tprintf(TEXT("FAILED: Cronus Zen found but unable to hide!\r\n"));
            break;
        case Zen_Hidden:
            _tprintf(TEXT("SUCCESS: Cronus Zen device is now hidden!\r\n"));
            break;
    }

    _getch();

	return 0;
}