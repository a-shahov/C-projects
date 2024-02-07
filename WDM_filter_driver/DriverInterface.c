#include <fileapi.h> // For CreateFile
#include <ioapiset.h> // For DeviceIoControl
#include <handleapi.h> // For CloseHandle
#include <errhandlingapi.h> // For GetLastError
#include <winioctl.h> // For CTL_CODE
#include <synchapi.h>
#include <stdio.h>
#include <conio.h>
#include <malloc.h>
#include <stdlib.h>

#include "TSFD_IOCTL.h"

#define OUT_LENGTH 0x800

typedef struct _RESTRICTION_POINT {
	UINT16 x;
	UINT16 y;
} RESTRICTION_POINT, *PRESTRICTION_POINT;

void caller(HANDLE DeviceHandle, int Sought)
{
	ULONG FeedBack;
	RESTRICTION_POINT RPointsHQ[9];
	RESTRICTION_POINT RPoints[5];
	UINT8 OutBuffer[OUT_LENGTH];
	DWORD Delay = 6000;

	if (Sought == 11) {
		for (int i = 0; i < 9; ++i) {
			scanf("%hu", &RPointsHQ[i].x);
			scanf("%hu", &RPointsHQ[i].y);
		}

		printf("9 points received\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_SEND_CALIBRATION_POINTS_HQ, (LPVOID)RPointsHQ, sizeof(RPointsHQ), (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}

	if (Sought == 0) {
		for (int i = 0; i < 5; ++i) {
			scanf("%hu", &RPoints[i].x);
			scanf("%hu", &RPoints[i].y);
		}

		printf("5 points received\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_SEND_CALIBRATION_POINTS, (LPVOID)RPoints, sizeof(RPoints), (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}

	if (Sought == 1) {
		printf("Select Device...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_IDENTIFY_DEVICE, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}

	if (Sought == 12) {
		printf("IOCTL_DISABLE_LARGE_TOUCHES...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_DISABLE_LARGE_TOUCHES, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		printf("Waiting for %d seconds\r\n", Delay / 1000);
		fflush(NULL);
		Sleep(Delay);
	}

	if (Sought == 13) {
		printf("IOCTL_ACTIVATE_LARGE_TOUCHES...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_ACTIVATE_LARGE_TOUCHES, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		printf("Waiting for %d seconds\r\n", Delay / 1000);
		fflush(NULL);
		Sleep(Delay);
	}

	if (Sought == 3) {
		printf("IOCTL_START_CALIBRATION...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_START_CALIBRATION, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}

	if (Sought == 2) {
		printf("IOCTL_STOP_CALIBRATION...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_STOP_CALIBRATION, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}

	if (Sought == 4) {
		printf("IOCTL_REMOVE_DRIVER...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_REMOVE_DRIVER, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}

	if (Sought == 6) {
		printf("IOCTL_GET_STATE_INFORMATION...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_GET_STATE_INFORMATION, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}

	if (Sought == 8) {
		printf("Disabling Device...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_DISABLE_DEVICE, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		printf("Waiting for %d seconds\r\n", Delay / 1000);
		fflush(NULL);
		Sleep(Delay);
	}

	if (Sought == 9) {
		printf("Activating Device...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_ACTIVATE_DEVICE, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		printf("Waiting for %d seconds\r\n", Delay / 1000);
		fflush(NULL);
		Sleep(Delay);
	}

	if (Sought == 10) {
		printf("IOCTL_RESET_TABCAL...\r\n");
		fflush(NULL);
		if (!DeviceIoControl(DeviceHandle, IOCTL_RESET_TABCAL, NULL, 0, (LPVOID)OutBuffer, OUT_LENGTH, &FeedBack, NULL)) {
			printf("DeviceIoControl has failed with error code: %u\r\n", GetLastError());
		}
		wprintf(L"%ls\r\n", (const wchar_t*)OutBuffer);
		fflush(NULL);
	}
}

int main(int argc, char **argv)
{
	HANDLE DeviceHandle = NULL;
	INT Parameters[100] = { 0 };

	for (int i = 1; i < argc; ++i) {
		Parameters[i - 1] = atoi(*(argv + i));
	}

	printf("starting...\r\n");

	DeviceHandle = CreateFile("\\\\.\\TouchscreenFilterConfigDeviceLink", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (DeviceHandle == INVALID_HANDLE_VALUE) {
		printf("CreateFile has failed with error code: %u\r\n", GetLastError());
	}
	else {
		printf("CreateFile has succeed\r\n");
		fflush(NULL);

		for (int i = 0; i < argc - 1; ++i) {
			caller(DeviceHandle, Parameters[i]);
		}

		CloseHandle(DeviceHandle);
	}

	return 0;
}
