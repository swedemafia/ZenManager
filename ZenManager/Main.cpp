/*
    ZenFinder
    --
    Searches connected USB devices for a Cronus Zen device
    Searches Registry for existing key detecting previous Cronus Zen device use on the system

    Author: swedemafia
    Last update: 28 Jan 2023
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

// Size of array
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// Product/vendor identifiers
TCHAR szPid[16] = { NULL }, szVid[16] = { NULL }, szRev[16] = { NULL };

// FindZen return values
enum ZenStatus {
    Zen_UnableQueryUSB = 0,
    Zen_NotFound,
    Zen_Missing_VID,
    Zen_Missing_PID,
    Zen_Missing_REV,
    Zen_Failed,
    Zen_Failed_Query,
    Zen_Found,
    Zen_Hidden
};

// See if a Cronus Zen is present on the system
ZenStatus FindZen(void) {
    DEVPROPTYPE dptPropertyType;
    DWORD dwRequiredSize;
    HDEVINFO hDevInfo;
    LPTSTR pszToken, pszNextToken = NULL;
    SP_DEVINFO_DATA DeviceInfoData;
    TCHAR szBusReportedDeviceDesc[1024], szDeviceDescription[1024], szHardwareID[1024];

    // Search for connected USB devices
    hDevInfo = SetupDiGetClassDevs(NULL, TEXT("USB"), NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);

    // Returns INVALID_HANDLE_VALUE if no USB found
    if(hDevInfo == INVALID_HANDLE_VALUE)
        return Zen_UnableQueryUSB;

    DeviceInfoData.cbSize = sizeof(DeviceInfoData);

    // Enumerate list of present USB devices
    for (unsigned i = 0; ; i++) {
        // Check if no more devices are found
        if(!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData)) {
            break;
        }

        // Search for the Cronus Zen
        if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &dptPropertyType, (PBYTE)szBusReportedDeviceDesc, sizeof(szBusReportedDeviceDesc), &dwRequiredSize, 0)) {
            
            // See if we found a Cronus Zen ("Cronus Bridge")
            if (!_tcsncmp(szBusReportedDeviceDesc, TEXT("Cronus Bridge"), lstrlen(TEXT("Cronus Bridge")))) {

                // Cronus Zen found...
                _tprintf(TEXT("Cronus Zen found...\r\n"));

                // Get Cronus Zen device information...
                if (SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_DEVICEDESC, &dptPropertyType, (PBYTE)szDeviceDescription, sizeof(szDeviceDescription), &dwRequiredSize)) {
                    _tprintf(TEXT("\tDevice Description: %s\r\n"), szDeviceDescription);
                    _tprintf(TEXT("\tBus Reported Device Description: %s\r\n"), szBusReportedDeviceDesc);

                    // Get hardware IDs that will be stored in the Registry
                    if (SetupDiGetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)szHardwareID, sizeof(szHardwareID), NULL)) {
                        _tprintf(TEXT("\tHardware IDs: %s\r\n"), szHardwareID);

                        // Determine Cronus vendor ID, product ID and revision
                        pszToken = _tcstok_s(szHardwareID, TEXT("\\#&"), &pszNextToken);

                        while (pszToken != NULL) {
                            // Check for vendor ID
                            if (!_tcsncmp(pszToken, TEXT("VID_"), lstrlen(TEXT("VID_")))) {
                                _tcscpy_s(szVid, ARRAY_SIZE(szVid), pszToken);
                                _tprintf(TEXT("\tVendor ID: %s\r\n"), szVid + lstrlen(TEXT("VID_")));
                            }
                            else if (!_tcsncmp(pszToken, TEXT("PID_"), lstrlen(TEXT("PID_")))) {
                                _tcscpy_s(szPid, ARRAY_SIZE(szPid), pszToken);
                                _tprintf(TEXT("\tProduct ID: %s\r\n"), szPid + lstrlen(TEXT("PID_")));
                            }
                            else if (!_tcsncmp(pszToken, TEXT("REV_"), lstrlen(TEXT("REV_")))) {
                                _tcscpy_s(szRev, ARRAY_SIZE(szRev), pszToken);
                                _tprintf(TEXT("\tRevision: %s\r\n"), szRev + lstrlen(TEXT("REV_")));
                            }

                            pszToken = _tcstok_s(NULL, TEXT("\\#&"), &pszNextToken);
                        }

                        _tprintf(TEXT("\r\n"));
                        _tprintf(TEXT("Registry entry location:\r\n\tHKEY_LOCAL_MACHINE\\System\\CurrentControlSet\\Control\\usbflags\\%s%s%s\r\n"), szVid + lstrlen(TEXT("VID_")), szPid + lstrlen(TEXT("PID_")), szPid + lstrlen(TEXT("REV_")));
                        _tprintf(TEXT("\r\n"));

                        // See if vendor ID found
                        if (szVid[0] == '\0')
                            return Zen_Missing_VID;

                        // See if product ID found
                        if (szPid[0] == '\0')
                            return Zen_Missing_PID;

                        // See if revision found
                        if (szRev[0] == '\0')
                            return Zen_Missing_REV;
                    }  else {
                        _tprintf(TEXT("\tUnable to locate Hardware IDs!\r\n"));
                    }

                    
                    /*
                    // Attempt to rename Device (elevated privileges required)
                    const TCHAR szBuffer[128] = TEXT("Testing");
                    DWORD dwPropertyBufferSize = _tcslen(szBuffer) * 2;

                    if (SetupDiSetDeviceRegistryProperty(hDevInfo, &DeviceInfoData, SPDRP_FRIENDLYNAME, (const BYTE*)szBuffer, dwPropertyBufferSize)) {
                        return Zen_Hidden;
                    } else {
                        // Get system error message
                        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, GetLastError(), 0, (LPWSTR)&pszErrorMessage, 0, NULL);

                        _tprintf(TEXT("\tSystem error: %s\r\n"), pszErrorMessage);
                    }

                    return Zen_Failed;
                    */

                    return Zen_Found;
                } else {
                    return Zen_Failed_Query;
                }
            }
        }
    }

	return Zen_NotFound;
}

void CheckRegistry(void) {

    // Registry identifiers (HKEY_LOCAL_MACHINE/System/CurrentControlSet/Control/usbflags/200800100100)

    const TCHAR *szSubKey = TEXT("System\\CurrentControlSet\\Control\\usbflags\\200800100100");
    LRESULT lResult;
    HKEY hKey;

    // Open key
    lResult = RegOpenKeyExW(HKEY_LOCAL_MACHINE, szSubKey, 0, KEY_READ, &hKey);

    if (lResult != ERROR_SUCCESS) {
        _tprintf(TEXT("Previous use of a Cronus Zen is NOT DETECTED on this system!\r\n"));
    } else {
        _tprintf(TEXT("Previous use of a Cronus Zen is DETECTED on this system!\r\n"));
        RegCloseKey(hKey); // Close key
    }

    _tprintf(TEXT("\r\n"));
}

int _tmain(int argc, _TCHAR* argv[]) {

    // Check Registry for previous use
    CheckRegistry();

    // Display status
    switch (FindZen()) {
    case Zen_UnableQueryUSB:
        _tprintf(TEXT("FAILED: unable to query USB devices on system!\r\n"));
        break;
    case Zen_NotFound:
        _tprintf(TEXT("FAILED: unable to locate a connected Cronus Zen device!\r\n"));
        break;
    case Zen_Missing_VID:
        _tprintf(TEXT("FAILED: unable to identify Cronus Zen vendor ID!\r\n"));
        break;
    case Zen_Missing_PID:
        _tprintf(TEXT("FAILED: unable to identify Cronus Zen product ID!\r\n"));
        break;
    case Zen_Missing_REV:
        _tprintf(TEXT("FAILED: unable to identify Cronus Zen revision!\r\n"));
        break;
    case Zen_Failed_Query:
        _tprintf(TEXT("FAILED: Cronus Zen found but unable to query registry device properties!\r\n"));
        break;
    case Zen_Found:
        _tprintf(TEXT("SUCCESS: A Cronus Zen is currently connected to the system!\r\n"));
        break;
    }

    _getch();

	return 0;
}