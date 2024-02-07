#pragma once

#include <ntddk.h>
#include <ntstrsafe.h>
#include <wdmsec.h> // Need to additionally link Wdmsec.lib

#include "TSFD_IOCTL.h"

#define FIDO_EXTENSION 0
#define EDO_EXTENSION 1
#define FIVE_CALIBRATION_POINTS 5
#define NINE_CALIBRATION_POINTS 9
#define SIX_POINTS_IN_PACKET 6
#define MAX_VALUE 32767 // See DoFloatingPointCalculationForCalibrationNine and DoFloatingPointCalculationForCalibrationFive
#define WAITING_TIME 30 // In seconds, See DispatchDevCTL IOCTL_IDENTIFY_DEVICE
#define TMP_BUFFER_SIZE 0x600 // See DispatchDevCTL, GetAndDeleteRMSZValueFromRegistry
#define MAX_WIDTH 600 // There is a relationship MAX_WIDTH / MAX_HEIGHT = 0.57
#define MAX_HEIGHT 1050

int _fltused = 0; // It is necessary for the correct floating calculation

PDEVICE_OBJECT Edo = NULL; // Global pointer to Extra Device Object (EDO), see function CreateEDO which is using in AddDevice
ULONG NumberDevices = 0; // Global variable which counts the number of FiDO, see function AddDevice
UNICODE_STRING RegistryPathService; // Registry path to SYSTEM\\CurrentControlSet\\Services
const UNICODE_STRING Basis = RTL_CONSTANT_STRING(L"Troubleshoot: "); // It is using in DispatchDevCTL as start string for every debug message
const UNICODE_STRING RegistryPathClass = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\{745a17a0-74d3-11d0-b6fe-00a0c90f57da}");
const UNICODE_STRING MyDriverName = RTL_CONSTANT_STRING(L"TouchscreenFilterDriver"); // See GetAndDeleteRMSZValueFromRegistry

/*
* Keys in the registry associated with tabcal
*/
const UNICODE_STRING EnumHIDPath = RTL_CONSTANT_STRING(L"\\Registry\\Machine\\System\\CurrentControlSet\\Enum\\HID"); // See EnumerateAndDeleteTabCalValues
const UNICODE_STRING TabCalValue = RTL_CONSTANT_STRING(L"LinearityData"); // DeleteTabCalValue
const UNICODE_STRING EnumId = RTL_CONSTANT_STRING(L"VID_1FF7&PID_0013"); // See EnumerateAndDeleteTabCalValues

/*
* Information in registry about the frame which we want to process
* See CheckDeviceProperty
*/
const UNICODE_STRING SoughtHardwareID = RTL_CONSTANT_STRING(L"HID\\VID_1FF7&PID_0013");
const UNICODE_STRING SoughtDeviceDescription = RTL_CONSTANT_STRING(L"HID-compliant touch screen");

/*
* String with state information
*/
const UNICODE_STRING StateInformation = 
	RTL_CONSTANT_STRING(
		L"{\r\n\t\"Number devices\": \"%u\",\r\n"
		L"\t\"Calibration was started\": \"%u\",\r\n"
		L"\t\"Points is available\": \"%u\",\r\n"
		L"\t\"Number calibration points\": \"%u\",\r\n"
		L"\t\"Device is active\": \"%u\",\r\n"
		L"\t\"Device is selected\": \"%u\",\r\n"
		L"\t\"Large touches is active\": \"%u\",\r\n"
		L"\t\"Tabcal data is installed in the registry\": \"%u\"\r\n}"
	);

/*
* My GUID {540f94e5-09b3-40d8-9456-818d45e324eb}
*/
const GUID EdoGuid = { 0x540f94e5L, 0x09b3, 0x40d8, {0x94, 0x56, 0x81, 0x8d, 0x45, 0xe3, 0x24, 0xeb} };

typedef struct _COMMON_DEVICE_EXTENSION {
	ULONG flag; //Fido or Edo
	IO_REMOVE_LOCK RemoveLock;
	PDRIVER_OBJECT DriverObject;
} COMMON_DEVICE_EXTENSION, *PCOMMON_DEVICE_EXTENSION;

typedef struct _CALIBRATION_POINT {
	UINT16 x;
	UINT16 y;
} CALIBRATION_POINT, *PCALIBRATION_POINT;

typedef struct _DEVICE_EXTENSION {
	COMMON_DEVICE_EXTENSION;
	PDEVICE_OBJECT LowerDevice;
	PDEVICE_OBJECT Pdo;
	PDEVICE_OBJECT Fido;
	BOOLEAN ActiveLargeTouches;
	BOOLEAN DeviceTarget;
	BOOLEAN DeviceActive;
	ULONG IdInstance; // SYSTEM\CurrentControlSet\Control\Class\{745a17a0-74d3-11d0-b6fe-00a0c90f57da}\IdInstance
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

typedef struct _EXTRA_DEVICE_EXTENSION {
	COMMON_DEVICE_EXTENSION;
	PDEVICE_OBJECT TargetDevice;
	UNICODE_STRING EDOSymLinkName;
	UNICODE_STRING RegistryPath;
	KEVENT SyncEvent;
	KEVENT SyncCloseCreateEvent;
	CALIBRATION_POINT CPoints[NINE_CALIBRATION_POINTS]; // The array is allocated immediately for the maximum number of points
	USHORT NumberPoints;
	BOOLEAN CheckPoints;
	BOOLEAN Start;
	BOOLEAN TabCalValuesInReg;
} EXTRA_DEVICE_EXTENSION, *PEXTRA_DEVICE_EXTENSION;

#pragma pack(push, 1)

typedef struct _INPUT_TOUCH_DATA {
	UINT8 unknownFlag; // unknown flag usually was 0x04 and 0x07
	UINT8 id; // 0x01, 0x02 etc.
	UINT16 x; // 0 - 32767
	UINT16 y;
	UINT16 width; // 0 - 32767
	UINT16 height;
} INPUT_TOUCH_DATA, *PINPUT_TOUCH_DATA;

typedef struct _INPUT_TOUCH_PACKET {
	UINT8 unknown; // always 2 ?
	INPUT_TOUCH_DATA touches[SIX_POINTS_IN_PACKET];
	UINT8 number_points;
} INPUT_TOUCH_PACKET, *PINPUT_TOUCH_PACKET;

#pragma pack(pop)


// The main functions of the driver
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT Pdo);
NTSTATUS DispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchDevCTL(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DispatchAny(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID DriverUnload(IN PDRIVER_OBJECT DriverObject);

// Request completion functions
NTSTATUS UsageNotificationCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PDEVICE_EXTENSION Pdx);
NTSTATUS StartDeviceCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PDEVICE_EXTENSION Pdx);
NTSTATUS ReadCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

// Auxiliary user functions
NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS Status, ULONG_PTR Information);
NTSTATUS IntendedForFiDO(PDEVICE_OBJECT DeviceObject, PIRP Irp);
VOID RemoveDevice(PDEVICE_OBJECT DeviceObject);
__declspec(noinline) VOID DoFloatingPointCalculationForCalibrationNine(PINPUT_TOUCH_PACKET Buffer, PDEVICE_EXTENSION Pdx);
__declspec(noinline) VOID DoFloatingPointCalculationForCalibrationFive(PINPUT_TOUCH_PACKET Buffer, PDEVICE_EXTENSION Pdx);
NTSTATUS GetDwordValueFromRegistry(PULONG Value, PCWSTR PValueName);
NTSTATUS SetDwordValueInRegistry(ULONG Value, PCWSTR PValueName);
NTSTATUS DeleteKeyValueInRegistry(PUNICODE_STRING PRegistryPath, PCWSTR PValueName);
__declspec(noinline) NTSTATUS CheckPointsLayoutFive(CALIBRATION_POINT Prp[]);
__declspec(noinline) NTSTATUS CheckPointsLayoutNine(CALIBRATION_POINT Prp[]);
NTSTATUS GetAndCheckCalibrationPoints(PVOID Buffer, ULONG InLength, ULONG NumberPoints);
VOID ClearAllDeviceTargeting();
VOID SetAllDeviceActive();
BOOLEAN CheckDeviceProperty(PDEVICE_OBJECT Pdo);
ULONG GetDeviceTypeToUse(PDEVICE_OBJECT Pdo);
NTSTATUS CreateEDO(PDRIVER_OBJECT DriverObject);
NTSTATUS GetIdInstance(PDEVICE_OBJECT Pdo, PULONG IdInstance);
NTSTATUS RecordPointsInRegistry(PCWSTR PValueName, ULONG NumberPoints);
NTSTATUS RetrievePointsFromRegistry(PCWSTR PValueName, ULONG NumberPoints);
NTSTATUS SendInformation(PWCHAR Data, PVOID Buffer, ULONG OutLength, PULONG Info);
VOID ResetInputData(PINPUT_TOUCH_PACKET Buffer);
NTSTATUS SetSingleDeviceTarget();
NTSTATUS AdhereNewDataToData(PWCHAR Data, ULONG BufSize, NTSTRSAFE_PCWSTR NewData);
VOID DoCalcCoeff(DOUBLE A[], DOUBLE B[], DOUBLE C[], ULONG Index, PCALIBRATION_POINT Vec1, PCALIBRATION_POINT Vec2);
DOUBLE Fx(DOUBLE A, DOUBLE B, DOUBLE C, DOUBLE y);
DOUBLE Fy(DOUBLE A, DOUBLE B, DOUBLE C, DOUBLE x);
NTSTATUS GetAndDeleteRMSZValueFromRegistry();
NTSTATUS EnumerateAndDeleteTabCalValues(BOOLEAN NeedDelete);
NTSTATUS GetInformationFromRegistry(HANDLE Hkey, KEY_INFORMATION_CLASS KeyInfo, ULONG Tag, PVOID *PInf);
NTSTATUS EnumerateIdDevices(PWCHAR HidDeviceName, BOOLEAN NeedDelete);
BOOLEAN StrInStr(PWCHAR SubStr, SIZE_T SubSize, PWCHAR Str, SIZE_T Size);
NTSTATUS DeleteTabCalValue(PWCHAR HidDeviceName, BOOLEAN NeedDelete);
