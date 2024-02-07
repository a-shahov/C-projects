#include "TouchscreenFilterDriver.h"

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	RtlFreeUnicodeString(&RegistryPathService);
}

NTSTATUS CompleteRequest(PIRP Irp, NTSTATUS Status, ULONG_PTR Information)
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}

NTSTATUS IntendedForFiDO(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PCOMMON_DEVICE_EXTENSION Cdx = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	if (Cdx->flag == EDO_EXTENSION) {
		Status = STATUS_INVALID_PARAMETER;
		return CompleteRequest(Irp, Status, 0);
	}

	return STATUS_SUCCESS;
}

NTSTATUS DispatchAny(PDEVICE_OBJECT DeviceObject, PIRP Irp)  
{
	NTSTATUS Status;
	PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	Status = IntendedForFiDO(DeviceObject, Irp);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	Status = IoAcquireRemoveLock(&Pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(Status)) {
		return CompleteRequest(Irp, Status, 0);
	}

	IoSkipCurrentIrpStackLocation(Irp);
	Status = IoCallDriver(Pdx->LowerDevice, Irp);
	IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);

	return Status;
}

VOID RemoveDevice(PDEVICE_OBJECT DeviceObject)
{
	PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;

	if (Edx->TargetDevice == DeviceObject) {
		Edx->TargetDevice = NULL;
	}
	IoDetachDevice(Pdx->LowerDevice);
	IoDeleteDevice(DeviceObject);
	InterlockedDecrement(&NumberDevices);
	if (!NumberDevices) {
		IoDeleteSymbolicLink(&Edx->EDOSymLinkName);
		IoDeleteDevice(Edo);
	}
}

NTSTATUS UsageNotificationCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PDEVICE_EXTENSION Pdx)
{
	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}
	if (!(Pdx->LowerDevice->Flags & DO_POWER_PAGABLE)) {
		DeviceObject->Flags &= ~DO_POWER_PAGABLE;
	}
	IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);
	return STATUS_SUCCESS;
}

NTSTATUS StartDeviceCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PDEVICE_EXTENSION Pdx)
{
	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}
	if (Pdx->LowerDevice->Characteristics & FILE_REMOVABLE_MEDIA) {
		DeviceObject->Characteristics |= FILE_REMOVABLE_MEDIA;
	}
	IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);
	return STATUS_SUCCESS;
}

NTSTATUS DispatchPnp(PDEVICE_OBJECT DeviceObject, PIRP Irp) 
{
	NTSTATUS Status;
	PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG fcn = Stack->MinorFunction;
	PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;

	Status = IntendedForFiDO(DeviceObject, Irp);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	Status = IoAcquireRemoveLock(&Pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(Status)) {
		return CompleteRequest(Irp, Status, 0);
	}

	if (fcn == IRP_MN_DEVICE_USAGE_NOTIFICATION) {
		if ((!DeviceObject->AttachedDevice) || (DeviceObject->AttachedDevice->Flags & DO_POWER_PAGABLE)) {
			DeviceObject->Flags |= DO_POWER_PAGABLE;
		}
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutineEx(DeviceObject, Irp,
			(PIO_COMPLETION_ROUTINE)UsageNotificationCompletionRoutine, (PVOID)Pdx, TRUE, TRUE, TRUE);
		return IoCallDriver(Pdx->LowerDevice, Irp);
	}

	if (fcn == IRP_MN_START_DEVICE) {
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutineEx(DeviceObject, Irp,
			(PIO_COMPLETION_ROUTINE)StartDeviceCompletionRoutine, (PVOID)Pdx, TRUE, TRUE, TRUE);
		return IoCallDriver(Pdx->LowerDevice, Irp);
	}
	
	if (fcn == IRP_MN_REMOVE_DEVICE) {
		IoSkipCurrentIrpStackLocation(Irp);
		Status = IoCallDriver(Pdx->LowerDevice, Irp);
		IoReleaseRemoveLockAndWait(&Pdx->RemoveLock, Irp);
		if (NumberDevices == 1) {
			IoReleaseRemoveLockAndWait(&Edx->RemoveLock, NULL);
			KeWaitForSingleObject((PVOID)&Edx->SyncCloseCreateEvent, Executive, KernelMode, FALSE, NULL);
		}
		RemoveDevice(DeviceObject); 
		return Status;
	}

	IoSkipCurrentIrpStackLocation(Irp);
	Status = IoCallDriver(Pdx->LowerDevice, Irp);
	IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);
	return Status; 
}

NTSTATUS DispatchPower(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS Status;
	PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

	Status = IntendedForFiDO(DeviceObject, Irp);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	PoStartNextPowerIrp(Irp);
	Status = IoAcquireRemoveLock(&Pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(Status)) {
		return CompleteRequest(Irp, Status, 0);
	}
	IoSkipCurrentIrpStackLocation(Irp);
	Status = PoCallDriver(Pdx->LowerDevice, Irp);
	IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);
	return Status;
}

inline DOUBLE Fx(DOUBLE A, DOUBLE B, DOUBLE C, DOUBLE y)
{
	return -(B * y + C) / A;
}

inline DOUBLE Fy(DOUBLE A, DOUBLE B, DOUBLE C, DOUBLE x)
{
	return -(A * x + C) / B;
}

VOID DoCalcCoeff(DOUBLE A[], DOUBLE B[], DOUBLE C[], ULONG Index, PCALIBRATION_POINT Vec1, PCALIBRATION_POINT Vec2)
{
	A[Index] = Vec2->y - Vec1->y;
	B[Index] = Vec1->x - Vec2->x;
	C[Index] = -A[Index] * Vec1->x - B[Index] * Vec1->y;
}

__declspec(noinline) VOID DoFloatingPointCalculationForCalibrationNine(PINPUT_TOUCH_PACKET Buffer, PDEVICE_EXTENSION Pdx)
{
	PCALIBRATION_POINT Prp = ((PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension)->CPoints;
	PINPUT_TOUCH_DATA Touch;
	DOUBLE A[8] = { 0 };
	DOUBLE B[8] = { 0 };
	DOUBLE C[8] = { 0 };
	DOUBLE x, y, rx1, rx2, rx3;

	DoCalcCoeff(A, B, C, 0, Prp, Prp + 4); // Left vertical
	DoCalcCoeff(A, B, C, 1, Prp + 1, Prp + 3); // Right vertical
	DoCalcCoeff(A, B, C, 2, Prp, Prp + 1); // Top horizontal
	DoCalcCoeff(A, B, C, 3, Prp + 3, Prp + 4); // Bottom horizontal
	DoCalcCoeff(A, B, C, 4, Prp + 5, Prp + 6); // 1 centr horizontal
	DoCalcCoeff(A, B, C, 5, Prp + 6, Prp + 7); // 2 centr horizontal
	DoCalcCoeff(A, B, C, 6, Prp + 7, Prp + 8); // 3 centr horizontal
	DoCalcCoeff(A, B, C, 7, Prp + 8, Prp + 2); // 4 centr horizontal

	rx1 = (Prp + 6)->x;
	rx2 = (Prp + 7)->x;
	rx3 = (Prp + 8)->x;

	if (Buffer) {
		for (UINT16 index = 0; index < SIX_POINTS_IN_PACKET; ++index) {
			Touch = &(Buffer->touches[index]);
			x = Touch->x;
			y = Touch->y;
			if (Touch->id == 0xFF) { // there is no point in this package 
				continue;
			}
			else if (!Pdx->ActiveLargeTouches && (Touch->width > MAX_WIDTH || Touch->height > MAX_HEIGHT)) {
				Touch->unknownFlag = 4;
			}
			else if ((rx3 <= x) && (Fx(A[1], B[1], C[1], y) >= x) && (Fy(A[2], B[2], C[2], x) <= y) && (Fy(A[7], B[7], C[7], x) >= y)) { // First quadrant
				Touch->x = (MAX_VALUE * 3 / 4) + (x - rx3) / (Fx(A[1], B[1], C[1], y) - rx3) * (MAX_VALUE / 4);
				Touch->y = (y - Fy(A[2], B[2], C[2], x)) / (Fy(A[7], B[7], C[7], x) - Fy(A[2], B[2], C[2], x)) * (MAX_VALUE / 2);
			}
			else if ((rx2 <= x) && (rx3 >= x) && (Fy(A[2], B[2], C[2], x) <= y) && (Fy(A[6], B[6], C[6], x) >= y)) { // Second quadrant
				Touch->x = (MAX_VALUE / 2) + (x - rx2) / (rx3 - rx2) * (MAX_VALUE / 4);
				Touch->y = (y - Fy(A[2], B[2], C[2], x)) / (Fy(A[6], B[6], C[6], x) - Fy(A[2], B[2], C[2], x)) * (MAX_VALUE / 2);
			}
			else if ((rx1 <= x) && (rx2 >= x) && (Fy(A[2], B[2], C[2], x) <= y) && (Fy(A[5], B[5], C[5], x) >= y)) { // Third quadrant
				Touch->x = (MAX_VALUE / 4) + (x - rx1) / (rx2 - rx1) * (MAX_VALUE / 4);
				Touch->y = (y - Fy(A[2], B[2], C[2], x)) / (Fy(A[5], B[5], C[5], x) - Fy(A[2], B[2], C[2], x)) * (MAX_VALUE / 2);
			}
			else if ((Fx(A[0], B[0], C[0], y) <= x) && (rx1 >= x) && (Fy(A[2], B[2], C[2], x) <= y) && (Fy(A[4], B[4], C[4], x) >= y)) { // Fourth quadrant
				Touch->x = (x - Fx(A[0], B[0], C[0], y)) / (rx1 - Fx(A[0], B[0], C[0], y)) * (MAX_VALUE / 4);
				Touch->y = (y - Fy(A[2], B[2], C[2], x)) / (Fy(A[4], B[4], C[4], x) - Fy(A[2], B[2], C[2], x)) * (MAX_VALUE / 2);
			}
			else if ((Fx(A[0], B[0], C[0], y) <= x) && (rx1 >= x) && (Fy(A[4], B[4], C[4], x) <= y) && (Fy(A[3], B[3], C[3], x) >= y)) { // Five quadrant
				Touch->x = (x - Fx(A[0], B[0], C[0], y)) / (rx1 - Fx(A[0], B[0], C[0], y)) * (MAX_VALUE / 4);
				Touch->y = (MAX_VALUE / 2) + (y - Fy(A[4], B[4], C[4], x)) / (Fy(A[3], B[3], C[3], x) - Fy(A[4], B[4], C[4], x)) * (MAX_VALUE / 2);
			}
			else if ((rx1 <= x) && (rx2 >= x) && (Fy(A[5], B[5], C[5], x) <= y) && (Fy(A[3], B[3], C[3], x) >= y)) { // Six quadrant
				Touch->x = (MAX_VALUE / 4) + (x - rx1) / (rx2 - rx1) * (MAX_VALUE / 4);
				Touch->y = (MAX_VALUE / 2) + (y - Fy(A[5], B[5], C[5], x)) / (Fy(A[3], B[3], C[3], x)- Fy(A[5], B[5], C[5], x)) * (MAX_VALUE / 2);
			}
			else if ((rx2 <= x) && (rx3 >= x) && (Fy(A[6], B[6], C[6], x) <= y) && (Fy(A[3], B[3], C[3], x) >= y)) { // Seven quadrant
				Touch->x = (MAX_VALUE / 2) + (x - rx2) / (rx3 - rx2) * (MAX_VALUE / 4);
				Touch->y = (MAX_VALUE / 2) + (y - Fy(A[6], B[6], C[6], x)) / (Fy(A[3], B[3], C[3], x) - Fy(A[6], B[6], C[6], x)) * (MAX_VALUE / 2);
			}
			else if ((rx3 <= x) && (Fx(A[1], B[1], C[1], y) >= x) && (Fy(A[7], B[7], C[7], x) <= y) && (Fy(A[3], B[3], C[3], x) >= y)) { // Eight quadrant
				Touch->x = (MAX_VALUE * 3 / 4) + (x - rx3) / (Fx(A[1], B[1], C[1], y) - rx3) * (MAX_VALUE / 4);
				Touch->y = (MAX_VALUE / 2) + (y - Fy(A[7], B[7], C[7], x)) / (Fy(A[3], B[3], C[3], x) - Fy(A[7], B[7], C[7], x)) * (MAX_VALUE / 2);
			}
			else {
				Touch->unknownFlag = 4;
			}
		}
	}
}

__declspec(noinline) VOID DoFloatingPointCalculationForCalibrationFive(PINPUT_TOUCH_PACKET Buffer, PDEVICE_EXTENSION Pdx)
{
	PCALIBRATION_POINT Prp = ((PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension)->CPoints;
	PINPUT_TOUCH_DATA Touch;
	DOUBLE A1, A2, A3, A4; // Equations of straight lines of the form Ax+By+C=0
	DOUBLE B1, B2, B3, B4;
	DOUBLE C1, C2, C3, C4;
	DOUBLE x, y, rx, ry;

	A1 = (Prp + 3)->y - (Prp)->y; // Left vertical
	B1 = (Prp)->x - (Prp + 3)->x;
	C1 = -A1 * (Prp)->x - B1 * (Prp)->y;

	A2 = (Prp + 2)->y - (Prp + 1)->y; // Right vertical
	B2 = (Prp + 1)->x - (Prp + 2)->x;
	C2 = -A2 * (Prp + 1)->x - B2 * (Prp + 1)->y;

	A3 = (Prp + 1)->y - (Prp)->y; // Top horizontal
	B3 = (Prp)->x - (Prp + 1)->x;
	C3 = -A3 * (Prp)->x - B3 * (Prp)->y;

	A4 = (Prp + 3)->y - (Prp + 2)->y; // Bottom horizontal
	B4 = (Prp + 2)->x - (Prp + 3)->x;
	C4 = -A4 * (Prp + 2)->x - B4 * (Prp + 2)->y;

	rx = (Prp + 4)->x;
	ry = (Prp + 4)->y;
	
	if (Buffer) { 
		for (UINT16 index = 0; index < SIX_POINTS_IN_PACKET; ++index) {
			Touch = &(Buffer->touches[index]);
			x = Touch->x;
			y = Touch->y;
			if (Touch->id == 0xFF) { // there is no point in this package 
				continue;
			}
			else if (!Pdx->ActiveLargeTouches && (Touch->width > MAX_WIDTH || Touch->height > MAX_HEIGHT)) {
				Touch->unknownFlag = 4;
			}
			else if ((rx <= x) && (-(B2 * y + C2) / A2 >= x) && (-(A3 * x + C3) / B3 <= y) && (ry >= y)) { // First quadrant
				Touch->x = (DOUBLE)(MAX_VALUE / 2) + ((x - rx) / (-(B2 * y + C2) / A2 - rx)) * (MAX_VALUE / 2);
				Touch->y = ((y - (-(A3 * x + C3) / B3)) / (ry - (-(A3 * x + C3) / B3))) * (MAX_VALUE / 2);
			}
			else if ((-(B1 * y + C1) / A1 <= x) && (rx >= x) && (-(A3 * x + C3) / B3 <= y) && ((ry >= y))) { // Second quadrant
				Touch->x = ((x - (-(B1 * y + C1) / A1)) / (rx - (-(B1 * y + C1) / A1))) * (MAX_VALUE / 2);
				Touch->y = ((y - (-(A3 * x + C3) / B3)) / (ry - (-(A3 * x + C3) / B3))) * (MAX_VALUE / 2);
			}
			else if ((-(B1 * y + C1) / A1 <= x) && (rx >= x) && (ry <= y) && (-(A4 * x + C4) / B4 >= y)) { // Third quadrant
				Touch->x = ((x - (-(B1 * y + C1) / A1)) / (rx - (-(B1 * y + C1) / A1))) * (MAX_VALUE / 2);
				Touch->y = (DOUBLE)(MAX_VALUE / 2) + ((y - ry) / (-(A4 * x + C4) / B4 - ry)) * (MAX_VALUE / 2);
			}
			else if ((rx <= x) && (-(B2 * y + C2) / A2 >= x) && (ry <= y) && (-(A4 * x + C4) / B4 >= y)) { // Fourth quadrant
				Touch->x = (DOUBLE)(MAX_VALUE / 2) + ((x - rx) / (-(B2 * y + C2) / A2 - rx)) * (MAX_VALUE / 2);
				Touch->y = (DOUBLE)(MAX_VALUE / 2) + ((y - ry) / (-(A4 * x + C4) / B4 - ry)) * (MAX_VALUE / 2);
			}
			else {
				Touch->unknownFlag = 4;
			}
		}
	}
}

VOID ResetInputData(PINPUT_TOUCH_PACKET Buffer)
{
	PINPUT_TOUCH_DATA Touch;

	if (Buffer) {
		for (UINT16 index = 0; index < SIX_POINTS_IN_PACKET; ++index) {
			Touch = &(Buffer->touches[index]);
			if (Touch->id == 0xFF) { // there is no point in this package 
				continue;
			}
			else {
				Touch->unknownFlag = 4;
			}
		}
	}
}

NTSTATUS ReadCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	NTSTATUS Status;
	PINPUT_TOUCH_PACKET Buffer = NULL;
	XSTATE_SAVE SaveState;
	PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;

	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}

	if (!NT_SUCCESS(Irp->IoStatus.Status)) {
		IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);
		return Irp->IoStatus.Status;
	}

	MmProbeAndLockPages(Irp->MdlAddress, KernelMode, IoModifyAccess);

	Buffer = (PINPUT_TOUCH_PACKET)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);

	if (Buffer) {
		if (Pdx->DeviceActive) {
			Status = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
			if (NT_SUCCESS(Status)) {
				__try {
					if (Edx->NumberPoints == FIVE_CALIBRATION_POINTS) {
						DoFloatingPointCalculationForCalibrationFive(Buffer, Pdx);
					}
					else if (Edx->NumberPoints == NINE_CALIBRATION_POINTS) {
						DoFloatingPointCalculationForCalibrationNine(Buffer, Pdx);
					}
				}
				__finally {
					KeRestoreExtendedProcessorState(&SaveState);
				}
			}
		}
		else {
			ResetInputData(Buffer);
		}
	}

	MmUnlockPages(Irp->MdlAddress);

	IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);

	return Irp->IoStatus.Status;
}

NTSTATUS GetAndDeleteRMSZValueFromRegistry()
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	UNICODE_STRING ValueName = RTL_CONSTANT_STRING(L"UpperFilters");
	ULONG Size = 0, Offset = 0, OffsetNew = 0;
	size_t StrLength;
	BOOLEAN EqStr = TRUE;

	InitializeObjectAttributes(&ObjAttr, (PUNICODE_STRING)&RegistryPathClass, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_ALL_ACCESS, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	Status = ZwQueryValueKey(Hkey, &ValueName, KeyValuePartialInformation, NULL, 0, &Size);
	if (Status == STATUS_OBJECT_NAME_NOT_FOUND || Size == 0) {
		ZwClose(Hkey);
		return Status;
	}

	Size = min(Size, PAGE_SIZE);

	PKEY_VALUE_PARTIAL_INFORMATION PartialInf = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, Size, 'key');

	if (!PartialInf) {
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = ZwQueryValueKey(Hkey, &ValueName, KeyValuePartialInformation, (PVOID)PartialInf, Size, &Size);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		return Status;
	}

	if (PartialInf->Type != REG_MULTI_SZ) {
		ZwClose(Hkey);
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		return STATUS_INVALID_VARIANT;
	}

	PWCHAR NewStr = (PWCHAR)ExAllocatePool2(POOL_FLAG_PAGED, PartialInf->DataLength, 'pmt');
	PWCHAR TmpStr = (PWCHAR)ExAllocatePool2(POOL_FLAG_PAGED, PartialInf->DataLength, 'pmt');
	if (!NewStr || !TmpStr) {
		if (NewStr) {
			ExFreePoolWithTag((PVOID)NewStr, 'pmt');
		}
		if (TmpStr) {
			ExFreePoolWithTag((PVOID)TmpStr, 'pmt');
		}
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	while (TRUE) { // Removes the value defined in MyDriverName from REG_MULTI_SZ value
		if (Offset + sizeof(WCHAR) == PartialInf->DataLength) {
			break;
		}
		RtlStringCbCopyW((NTSTRSAFE_PWSTR)(TmpStr), PartialInf->DataLength, (NTSTRSAFE_PCWSTR)(PartialInf->Data + Offset));
		RtlStringCbLengthW((STRSAFE_PCNZWCH)(PartialInf->Data + Offset), NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &StrLength);
		if (StrLength != MyDriverName.Length) {
			RtlStringCbCopyW((NTSTRSAFE_PWSTR)(NewStr + OffsetNew / sizeof(WCHAR)), PartialInf->DataLength - OffsetNew, (NTSTRSAFE_PCWSTR)TmpStr);
			Offset += StrLength + sizeof(WCHAR);
			OffsetNew += StrLength + sizeof(WCHAR);
			continue;
		}
		for (ULONG i = 0; i < StrLength / sizeof(WCHAR); ++i) {
			EqStr = EqStr && (((PWCHAR)(PartialInf->Data + Offset))[i] == MyDriverName.Buffer[i]);

		}
		if (!EqStr) {
			RtlStringCbCopyW((NTSTRSAFE_PWSTR)(NewStr + OffsetNew / sizeof(WCHAR)), PartialInf->DataLength - OffsetNew, (NTSTRSAFE_PCWSTR)TmpStr);
			OffsetNew += StrLength + sizeof(WCHAR);
		}
		Offset += StrLength + sizeof(WCHAR);
		EqStr = TRUE;
	}

	if (OffsetNew == 0) {
		ZwDeleteValueKey(Hkey, &ValueName);
	}
	else {
		ZwSetValueKey(Hkey, &ValueName, 0, REG_MULTI_SZ, (PVOID)NewStr, OffsetNew + sizeof(WCHAR));
	}
	
	ZwClose(Hkey);
	ExFreePoolWithTag((PVOID)NewStr, 'pmt');
	ExFreePoolWithTag((PVOID)TmpStr, 'pmt');
	ExFreePoolWithTag((PVOID)PartialInf, 'key');

	return Status;
}

NTSTATUS GetDwordValueFromRegistry(PULONG Value, PCWSTR PValueName)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	UNICODE_STRING ValueName;
	ULONG Size = 0;

	RtlInitUnicodeString(&ValueName, PValueName);
	InitializeObjectAttributes(&ObjAttr, &RegistryPathService, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_QUERY_VALUE, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	Status = ZwQueryValueKey(Hkey, &ValueName, KeyValuePartialInformation, NULL, 0, &Size);
	if (Status == STATUS_OBJECT_NAME_NOT_FOUND || Size == 0) {
		ZwClose(Hkey);
		return Status;
	}

	Size = min(Size, PAGE_SIZE);

	PKEY_VALUE_PARTIAL_INFORMATION PartialInf = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, Size, 'key');

	if (!PartialInf) {
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = ZwQueryValueKey(Hkey, &ValueName, KeyValuePartialInformation, (PVOID)PartialInf, Size, &Size);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		return Status;
	}

	if (PartialInf->Type != REG_DWORD) {
		ZwClose(Hkey);
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		return STATUS_INVALID_VARIANT;
	}

	*Value = *(PULONG)PartialInf->Data;

	ZwClose(Hkey);
	ExFreePoolWithTag((PVOID)PartialInf, 'key');

	return Status;
}

NTSTATUS SetDwordValueInRegistry(ULONG Value, PCWSTR PValueName)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = {0};
	HANDLE Hkey;
	UNICODE_STRING ValueName;

	RtlInitUnicodeString(&ValueName, PValueName);

	InitializeObjectAttributes(&ObjAttr, &RegistryPathService, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_SET_VALUE, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	Status = ZwSetValueKey(Hkey, &ValueName, 0, REG_DWORD, &Value, sizeof(Value));
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	ZwClose(Hkey);

	return Status;
}

NTSTATUS DispatchRead(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS Status;
	PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	LARGE_INTEGER Timeout;

	Status = IntendedForFiDO(DeviceObject, Irp);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	Status = IoAcquireRemoveLock(&Pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(Status)) {
		return CompleteRequest(Irp, Status, 0);
	}

	Timeout.QuadPart = 0;
	Status = KeWaitForSingleObject((PVOID)&Edx->SyncEvent, Executive, KernelMode, FALSE, &Timeout);
	if (Status == STATUS_SUCCESS) {
		Status = SetDwordValueInRegistry(Pdx->IdInstance, L"TargetSectionDriver");
		Pdx->DeviceTarget = TRUE;
		Edx->TargetDevice = DeviceObject;
	}

	if (Pdx->DeviceTarget && (!Pdx->DeviceActive || (Edx->Start && Edx->CheckPoints))) {
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutineEx(DeviceObject, Irp, (PIO_COMPLETION_ROUTINE)ReadCompletionRoutine, NULL, TRUE, TRUE, TRUE);
		Status = IoCallDriver(Pdx->LowerDevice, Irp);
	}
	else {
		IoSkipCurrentIrpStackLocation(Irp);
		Status = IoCallDriver(Pdx->LowerDevice, Irp);
		IoReleaseRemoveLock(&Pdx->RemoveLock, Irp);
	}
	
	return Status;
}

__declspec(noinline) NTSTATUS CheckPointsLayoutNine(CALIBRATION_POINT Prp[])
{
	DOUBLE A1, A2, A3, A4; // Equations of straight lines of the form Ax+By+C=0
	DOUBLE B1, B2, B3, B4;
	DOUBLE C1, C2, C3, C4;
	ULONG x, y;

	if ((Prp)->x >= (Prp + 1)->x) { // 1 and 2
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp)->y >= (Prp + 5)->y) { // 1 and 6
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 2)->y <= (Prp + 1)->y) { // 3 and 2
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 2)->y >= (Prp + 3)->y) { // 3 and 4
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 4)->x >= (Prp + 3)->x) { // 5 and 4
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 4)->y <= (Prp + 5)->y) { // 5 and 6
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 6)->x <= (Prp + 5)->x) { // 7 and 6
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 6)->x >= (Prp + 7)->x) { // 7 and 8
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 8)->x <= (Prp + 7)->x) { // 9 and 8
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 8)->x >= (Prp + 2)->x) { // 9 and 3
		return STATUS_INVALID_PARAMETER;
	}

	A1 = (Prp + 4)->y - (Prp)->y; // left vertical
	B1 = (Prp)->x - (Prp + 4)->x;
	C1 = -A1 * (Prp)->x - B1 * (Prp)->y;

	A2 = (Prp + 3)->y - (Prp + 1)->y; // Right vertical
	B2 = (Prp + 1)->x - (Prp + 3)->x;
	C2 = -A2 * (Prp + 1)->x - B2 * (Prp + 1)->y;

	A3 = (Prp + 1)->y - (Prp)->y; // Top horizontal
	B3 = (Prp)->x - (Prp + 1)->x;
	C3 = -A3 * (Prp)->x - B3 * (Prp)->y;

	A4 = (Prp + 4)->y - (Prp + 3)->y; // Bottom horizontal
	B4 = (Prp + 3)->x - (Prp + 4)->x;
	C4 = -A4 * (Prp + 3)->x - B4 * (Prp + 3)->y;

	x = (Prp + 6)->x;
	y = (Prp + 6)->y;

	if ((-(B1 * y + C1) / A1 >= x) || (-(B2 * y + C2) / A2 <= x)) {
		return STATUS_INVALID_PARAMETER;
	}

	if ((-(A3 * x + C3) / B3 >= y) || (-(A4 * x + C4) / B4 <= y)) {
		return STATUS_INVALID_PARAMETER;
	}

	x = (Prp + 7)->x;
	y = (Prp + 7)->y;

	if ((-(B1 * y + C1) / A1 >= x) || (-(B2 * y + C2) / A2 <= x)) {
		return STATUS_INVALID_PARAMETER;
	}

	if ((-(A3 * x + C3) / B3 >= y) || (-(A4 * x + C4) / B4 <= y)) {
		return STATUS_INVALID_PARAMETER;
	}

	x = (Prp + 8)->x;
	y = (Prp + 8)->y;

	if ((-(B1 * y + C1) / A1 >= x) || (-(B2 * y + C2) / A2 <= x)) {
		return STATUS_INVALID_PARAMETER;
	}

	if ((-(A3 * x + C3) / B3 >= y) || (-(A4 * x + C4) / B4 <= y)) {
		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

__declspec(noinline) NTSTATUS CheckPointsLayoutFive(CALIBRATION_POINT Prp[])
{	
	DOUBLE A1, A2, A3, A4; // Equations of straight lines of the form Ax+By+C=0
	DOUBLE B1, B2, B3, B4;
	DOUBLE C1, C2, C3, C4;
	ULONG x, y;

	if ((Prp)->x >= (Prp + 1)->x) { // 1 and 2
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp)->y >= (Prp + 3)->y) { // 1 and 4
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 2)->y <= (Prp + 1)->y) { // 3 and 2
		return STATUS_INVALID_PARAMETER;
	}

	if ((Prp + 2)->x <= (Prp + 3)->x) { // 3 and 4
		return STATUS_INVALID_PARAMETER;
	}

	A1 = (Prp + 3)->y - (Prp)->y; // Left vertical
	B1 = (Prp)->x - (Prp + 3)->x;
	C1 = -A1 * (Prp)->x - B1 * (Prp)->y;

	A2 = (Prp + 2)->y - (Prp + 1)->y; // Right vertical
	B2 = (Prp + 1)->x - (Prp + 2)->x;
	C2 = -A2 * (Prp + 1)->x - B2 * (Prp + 1)->y;

	A3 = (Prp + 1)->y - (Prp)->y; // Top horizontal
	B3 = (Prp)->x - (Prp + 1)->x;
	C3 = -A3 * (Prp)->x - B3 * (Prp)->y;

	A4 = (Prp + 3)->y - (Prp + 2)->y; // Bottom horizontal
	B4 = (Prp + 2)->x - (Prp + 3)->x;
	C4 = -A4 * (Prp + 2)->x - B4 * (Prp + 2)->y;

	x = (Prp + 4)->x;
	y = (Prp + 4)->y;

	if ((-(B1 * y + C1) / A1 >= x) || (-(B2 * y + C2) / A2 <= x)) { 
		return STATUS_INVALID_PARAMETER;
	}

	if ((-(A3 * x + C3) / B3 >= y) || (-(A4 * x + C4) / B4 <= y)) {
		return STATUS_INVALID_PARAMETER;
	}

	return STATUS_SUCCESS;
}

NTSTATUS GetAndCheckCalibrationPoints(PVOID Buffer, ULONG InLength, ULONG NumberPoints)
{
	NTSTATUS Status;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	XSTATE_SAVE SaveState;

	if (!Buffer) {
		return STATUS_INVALID_ADDRESS;
	}

	if (sizeof(Edx->CPoints[0]) * NumberPoints != InLength) {
		return STATUS_INVALID_BUFFER_SIZE;
	}

	for (ULONG i = 0; i < NumberPoints; ++i) {
		Edx->CPoints[i] = *((PCALIBRATION_POINT)Buffer + i);
		if ((Edx->CPoints[i].x > 32767) || (Edx->CPoints[i].y > 32767)) {
			return STATUS_INVALID_PARAMETER;
		}
	}

	Status = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
	if (NT_SUCCESS(Status)) {
		Status = STATUS_FLOAT_INVALID_OPERATION;
		__try {
			if (NumberPoints == FIVE_CALIBRATION_POINTS) {
				Status = CheckPointsLayoutFive(Edx->CPoints);
			}
			else if (NumberPoints == NINE_CALIBRATION_POINTS) {
				Status = CheckPointsLayoutNine(Edx->CPoints);
			}
		}
		__finally {
			KeRestoreExtendedProcessorState(&SaveState);
		}
		if (!NT_SUCCESS(Status)) {
			return Status;
		}
	}

	return Status;
}

VOID SetAllDeviceActive()
{
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	PDEVICE_OBJECT CurrentDevice = Edx->DriverObject->DeviceObject;

	while (CurrentDevice) {
		PCOMMON_DEVICE_EXTENSION Cdx = (PCOMMON_DEVICE_EXTENSION)CurrentDevice->DeviceExtension;
		if (Cdx->flag == FIDO_EXTENSION) {
			PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)CurrentDevice->DeviceExtension;
			Pdx->DeviceActive = TRUE;
		}
		CurrentDevice = CurrentDevice->NextDevice;
	}
}

VOID ClearAllDeviceTargeting()
{
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	PDEVICE_OBJECT CurrentDevice = Edx->DriverObject->DeviceObject;

	while (CurrentDevice) {
		PCOMMON_DEVICE_EXTENSION Cdx = (PCOMMON_DEVICE_EXTENSION)CurrentDevice->DeviceExtension;
		if (Cdx->flag == FIDO_EXTENSION) {
			PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)CurrentDevice->DeviceExtension;
			Pdx->DeviceTarget = FALSE;
		}
		CurrentDevice = CurrentDevice->NextDevice;
	}
}

NTSTATUS SetSingleDeviceTarget()
{
	NTSTATUS Status = STATUS_INVALID_DEVICE_STATE;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	PDEVICE_OBJECT CurrentDevice = Edx->DriverObject->DeviceObject;

	while (CurrentDevice) {
		PCOMMON_DEVICE_EXTENSION Cdx = (PCOMMON_DEVICE_EXTENSION)CurrentDevice->DeviceExtension;
		if (Cdx->flag == FIDO_EXTENSION) {
			PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)CurrentDevice->DeviceExtension;
			Pdx->DeviceTarget = TRUE;
			Edx->TargetDevice = CurrentDevice;
			Status = SetDwordValueInRegistry(Pdx->IdInstance, L"TargetSectionDriver");
			break;
		}
		CurrentDevice = CurrentDevice->NextDevice;
	}

	return Status;
}

NTSTATUS DeleteKeyValueInRegistry(PUNICODE_STRING PRegistryPath, PCWSTR PValueName)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	UNICODE_STRING ValueName;

	RtlInitUnicodeString(&ValueName, PValueName);
	InitializeObjectAttributes(&ObjAttr, PRegistryPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_ALL_ACCESS, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}
	
	Status = ZwDeleteValueKey(Hkey, &ValueName);
	if (Status == STATUS_OBJECT_NAME_NOT_FOUND) {
		Status = STATUS_SUCCESS;
	}

	ZwClose(Hkey);

	return Status;
}

NTSTATUS RetrievePointsFromRegistry(PCWSTR PValueName, ULONG NumberPoints)
{
	NTSTATUS Status;
	XSTATE_SAVE SaveState;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	UNICODE_STRING ValueName;
	ULONG Size = 0;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;

	RtlInitUnicodeString(&ValueName, PValueName);
	InitializeObjectAttributes(&ObjAttr, &RegistryPathService, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_QUERY_VALUE, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	Status = ZwQueryValueKey(Hkey, &ValueName, KeyValuePartialInformation, NULL, 0, &Size);
	if (Status == STATUS_OBJECT_NAME_NOT_FOUND || Size == 0) {
		ZwClose(Hkey);
		return Status;
	}

	Size = min(Size, PAGE_SIZE);

	PKEY_VALUE_PARTIAL_INFORMATION PartialInf = (PKEY_VALUE_PARTIAL_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, Size, 'key');

	if (!PartialInf) {
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = ZwQueryValueKey(Hkey, &ValueName, KeyValuePartialInformation, (PVOID)PartialInf, Size, &Size);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		return Status;
	}

	if (PartialInf->Type != REG_BINARY) {
		ZwClose(Hkey);
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		return STATUS_INVALID_VARIANT;
	}

	if (Size / sizeof(Edx->CPoints[0]) < Edx->NumberPoints) {
		ZwClose(Hkey);
		ExFreePoolWithTag((PVOID)PartialInf, 'key');
		return STATUS_INVALID_VARIANT;
	}

	for (ULONG i = 0; i < Edx->NumberPoints; ++i) { 
		Edx->CPoints[i] = *((PCALIBRATION_POINT)PartialInf->Data + i);
	}

	Status = KeSaveExtendedProcessorState(XSTATE_MASK_LEGACY, &SaveState);
	if (NT_SUCCESS(Status)) {
		Status = STATUS_FLOAT_INVALID_OPERATION;
		__try {
			if (Edx->NumberPoints == FIVE_CALIBRATION_POINTS) {
				Status = CheckPointsLayoutFive(Edx->CPoints);
				if (!NT_SUCCESS(Status)) {
					SetDwordValueInRegistry(FALSE, L"PointsAvailable");
				}
			}
			else if (Edx->NumberPoints == NINE_CALIBRATION_POINTS) {
				Status = CheckPointsLayoutNine(Edx->CPoints);
				if (!NT_SUCCESS(Status)) {
					SetDwordValueInRegistry(FALSE, L"PointsAvailable");
				}
			}
			else {
				Status = STATUS_INVALID_PARAMETER;
			}
		}
		__finally {
			KeRestoreExtendedProcessorState(&SaveState);
		}
	}
	
	ZwClose(Hkey);
	ExFreePoolWithTag((PVOID)PartialInf, 'key');

	return Status;
}

NTSTATUS RecordPointsInRegistry(PCWSTR PValueName, ULONG NumberPoints)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	UNICODE_STRING ValueName;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;

	RtlInitUnicodeString(&ValueName, PValueName);
	InitializeObjectAttributes(&ObjAttr, &RegistryPathService, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_SET_VALUE, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	Status = ZwSetValueKey(Hkey, &ValueName, 0, REG_BINARY, (PVOID)Edx->CPoints, sizeof(Edx->CPoints[0]) * NumberPoints);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	ZwClose(Hkey);

	return Status;
}

NTSTATUS GetInformationFromRegistry(HANDLE Hkey, KEY_INFORMATION_CLASS KeyInfo, ULONG Tag, PVOID *PInf)
{
	NTSTATUS Status;
	ULONG Size;

	ZwQueryKey(Hkey, KeyInfo, NULL, 0, &Size);

	Size = min(Size, PAGE_SIZE);

	*PInf = ExAllocatePool2(POOL_FLAG_PAGED, Size, Tag);
	if (!(*PInf)) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	Status = ZwQueryKey(Hkey, KeyInfo, *PInf, Size, &Size);
	if (!NT_SUCCESS(Status)) {
		ExFreePoolWithTag(*PInf, Tag);
		return Status;
	}

	return Status;
}

BOOLEAN StrInStr(PWCHAR SubStr, SIZE_T SubSize, PWCHAR Str, SIZE_T Size)
{
	BOOLEAN Check = TRUE;

	if (SubSize > Size) {
		Check = FALSE;
		return Check;
	}

	for (ULONG i = 0; i < SubSize / sizeof(WCHAR); ++i) {
		Check = Check && (SubStr[i] == Str[i]);
	}
	
	return Check;
}

NTSTATUS DeleteTabCalValue(PWCHAR HidDeviceName, BOOLEAN NeedDelete)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	UNICODE_STRING DeviceParameters;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	ULONG Size;

	RtlInitUnicodeString(&DeviceParameters, HidDeviceName);
	InitializeObjectAttributes(&ObjAttr, &DeviceParameters, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_ALL_ACCESS, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	if (NeedDelete) {
		Status = ZwDeleteValueKey(Hkey, (PUNICODE_STRING)&TabCalValue);
		if (Status == STATUS_OBJECT_NAME_NOT_FOUND) {
			Status = STATUS_SUCCESS;
		}
		Edx->TabCalValuesInReg = FALSE;
	}
	else {
		Status = ZwQueryValueKey(Hkey, (PUNICODE_STRING)&TabCalValue, KeyValuePartialInformation, NULL, 0, &Size);
		if (Status != STATUS_OBJECT_NAME_NOT_FOUND && Size != 0) {
			Edx->TabCalValuesInReg = TRUE;
		}
	}

	ZwClose(Hkey);

	return Status;
}

NTSTATUS EnumerateIdDevices(PWCHAR HidDeviceName, BOOLEAN NeedDelete)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	UNICODE_STRING DeviceName;
	ULONG Tag = 'fnI', MaxLength;
	PVOID PInf = NULL;
	SIZE_T LengthDevName;

	RtlInitUnicodeString(&DeviceName, HidDeviceName);
	InitializeObjectAttributes(&ObjAttr, &DeviceName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_ALL_ACCESS, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}

	Status = GetInformationFromRegistry(Hkey, KeyFullInformation, Tag, &PInf); // After use PInf, you need to free up memory
	if ((!NT_SUCCESS(Status)) || (!PInf)) {
		ZwClose(Hkey);
		return Status;
	}

	MaxLength = ((PKEY_FULL_INFORMATION)PInf)->MaxNameLen + sizeof(KEY_BASIC_INFORMATION);
	MaxLength = min(MaxLength, PAGE_SIZE);

	PKEY_BASIC_INFORMATION PBasicInf = (PKEY_BASIC_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, MaxLength, Tag);
	if (!PBasicInf) {
		ExFreePoolWithTag(PInf, Tag);
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PWCHAR HidDeviceName2 = (PWCHAR)ExAllocatePool2(POOL_FLAG_PAGED, TMP_BUFFER_SIZE, Tag);
	if (!HidDeviceName2) {
		ExFreePoolWithTag((PVOID)PBasicInf, Tag);
		ExFreePoolWithTag(PInf, Tag);
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlStringCbCopyW((NTSTRSAFE_PWSTR)HidDeviceName2, TMP_BUFFER_SIZE, (NTSTRSAFE_PCWSTR)HidDeviceName);
	RtlStringCbCatW((NTSTRSAFE_PWSTR)HidDeviceName2, TMP_BUFFER_SIZE, L"\\");
	RtlStringCbLengthW((STRSAFE_PCNZWCH)HidDeviceName2, TMP_BUFFER_SIZE, &LengthDevName);

	for (ULONG i = 0; i < ((PKEY_FULL_INFORMATION)PInf)->SubKeys; ++i) {
		ULONG Size;
		ZwEnumerateKey(Hkey, i, KeyBasicInformation, PBasicInf, MaxLength, &Size);
		RtlStringCbCopyW((NTSTRSAFE_PWSTR)(HidDeviceName2 + LengthDevName / sizeof(WCHAR)), TMP_BUFFER_SIZE - LengthDevName, PBasicInf->Name);
		RtlStringCbCopyW((NTSTRSAFE_PWSTR)(HidDeviceName2 + (LengthDevName + PBasicInf->NameLength) / sizeof(WCHAR)), TMP_BUFFER_SIZE - (LengthDevName + PBasicInf->NameLength), L"\\Device Parameters");
		DeleteTabCalValue(HidDeviceName2, NeedDelete);
	}

	ZwClose(Hkey);
	ExFreePoolWithTag((PVOID)HidDeviceName2, Tag);
	ExFreePoolWithTag(PInf, Tag);

	return Status;
}

NTSTATUS EnumerateAndDeleteTabCalValues(BOOLEAN NeedDelete)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES ObjAttr = { 0 };
	HANDLE Hkey;
	ULONG Tag = 'fnI', MaxLength;
	PVOID PInf = NULL;

	InitializeObjectAttributes(&ObjAttr, (PUNICODE_STRING)&EnumHIDPath, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	Status = ZwOpenKey(&Hkey, KEY_ALL_ACCESS, &ObjAttr);
	if (!NT_SUCCESS(Status)) {
		ZwClose(Hkey);
		return Status;
	}
	
	Status = GetInformationFromRegistry(Hkey, KeyFullInformation, Tag, &PInf); // After use PInf, you need to free up memory
	if ((!NT_SUCCESS(Status)) || (!PInf)) {
		ZwClose(Hkey);
		return Status;
	}

	MaxLength = ((PKEY_FULL_INFORMATION)PInf)->MaxNameLen + sizeof(KEY_BASIC_INFORMATION);
	MaxLength = min(MaxLength, PAGE_SIZE);

	PKEY_BASIC_INFORMATION PBasicInf = (PKEY_BASIC_INFORMATION)ExAllocatePool2(POOL_FLAG_PAGED, MaxLength, Tag);
	if (!PBasicInf) {
		ExFreePoolWithTag(PInf, Tag);
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	PWCHAR HidDeviceName = (PWCHAR)ExAllocatePool2(POOL_FLAG_PAGED, TMP_BUFFER_SIZE, Tag);
	if (!HidDeviceName) {
		ExFreePoolWithTag((PVOID)PBasicInf, Tag);
		ExFreePoolWithTag(PInf, Tag);
		ZwClose(Hkey);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	
	for (ULONG i = 0; i < ((PKEY_FULL_INFORMATION)PInf)->SubKeys; ++i) {
		ULONG Size;
		ZwEnumerateKey(Hkey, i, KeyBasicInformation, PBasicInf, MaxLength, &Size);
		RtlStringCbCopyW((NTSTRSAFE_PWSTR)HidDeviceName, EnumHIDPath.Length + sizeof(WCHAR), (NTSTRSAFE_PCWSTR)EnumHIDPath.Buffer);
		if (StrInStr((PWCHAR)EnumId.Buffer, EnumId.Length, PBasicInf->Name, PBasicInf->NameLength)) {
			RtlStringCbCatW((NTSTRSAFE_PWSTR)HidDeviceName, (UINT64)EnumHIDPath.Length + 2 * sizeof(WCHAR), L"\\");
			RtlStringCbCatW((NTSTRSAFE_PWSTR)HidDeviceName, PBasicInf->NameLength + (UINT64)EnumHIDPath.Length + 2 * sizeof(WCHAR), (NTSTRSAFE_PCWSTR)PBasicInf->Name);
			EnumerateIdDevices(HidDeviceName, NeedDelete);
		}
	}

	ZwClose(Hkey);
	ExFreePoolWithTag(PInf, Tag);
	ExFreePoolWithTag((PVOID)PBasicInf, Tag);
	ExFreePoolWithTag((PVOID)HidDeviceName, Tag);

	return Status;
}

NTSTATUS SendInformation(PWCHAR Data, PVOID Buffer, ULONG OutLength, PULONG Info)
{
	SIZE_T Length;

	RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Length);

	if (OutLength < Length + sizeof(WCHAR)) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	RtlStringCbCopyW((NTSTRSAFE_PWSTR)Buffer, Length + sizeof(WCHAR), (NTSTRSAFE_PCWSTR)Data);
	*Info = Length + sizeof(WCHAR);

	return STATUS_SUCCESS;
}

NTSTATUS AdhereNewDataToData(PWCHAR Data, ULONG BufSize, NTSTRSAFE_PCWSTR NewData)
{
	NTSTATUS Status;
	SIZE_T CurrentStrSize, SizeNewData;

	Status = RtlStringCbLengthW(NewData, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &SizeNewData); // SizeNewData without size the terminating null character
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	Status = RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &CurrentStrSize); // CurrentSize without size the terminating null character
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	if ((BufSize - CurrentStrSize - sizeof(WCHAR)) >= SizeNewData) {
		Status = RtlStringCbCatW((NTSTRSAFE_PWSTR)Data, BufSize, NewData);
	}

	return Status;
}

NTSTATUS DispatchDevCTL(PDEVICE_OBJECT DeviceObject, PIRP Irp) 
{
	NTSTATUS Status;
	PCOMMON_DEVICE_EXTENSION Cdx = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PVOID Buffer = Irp->AssociatedIrp.SystemBuffer;
	ULONG Info = 0; // How many bytes have we recorded in Buffer
	PIO_STACK_LOCATION Stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG InLength = Stack->Parameters.DeviceIoControl.InputBufferLength; 
	ULONG OutLength = Stack->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG Code = Stack->Parameters.DeviceIoControl.IoControlCode;

	LARGE_INTEGER Interval;
	PWCHAR Data = NULL;
	PWCHAR TmpStr = NULL;
	SIZE_T Size;

	if (Cdx->flag == FIDO_EXTENSION) {
		return DispatchAny(DeviceObject, Irp);
	}

	Status = IoAcquireRemoveLock(&Edx->RemoveLock, Irp);
	if (!NT_SUCCESS(Status)) {
		return CompleteRequest(Irp, Status, 0);
	}

	OutLength = min(OutLength, NTSTRSAFE_MAX_CCH * sizeof(WCHAR));

	Data = (PWCHAR)ExAllocatePool2(POOL_FLAG_PAGED, OutLength, 'data');
	TmpStr = (PWCHAR)ExAllocatePool2(POOL_FLAG_PAGED, TMP_BUFFER_SIZE, 'tmp');
	if (Data && TmpStr) {
		if (Basis.Length + sizeof(WCHAR) <= OutLength) {
			RtlStringCbCopyW((NTSTRSAFE_PWSTR)Data, Basis.Length + sizeof(WCHAR), Basis.Buffer);
		}	
	}
	
	PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)Edx->TargetDevice->DeviceExtension;

	switch (Code)
	{
	case IOCTL_SEND_CALIBRATION_POINTS:
		Edx->NumberPoints = FIVE_CALIBRATION_POINTS;
		Status = GetAndCheckCalibrationPoints(Buffer, InLength, Edx->NumberPoints);
		if (NT_SUCCESS(Status)) {
			Edx->CheckPoints = TRUE;
			Status = RecordPointsInRegistry(L"CalibrationPoints", Edx->NumberPoints);
			if (NT_SUCCESS(Status)) {
				Status = SetDwordValueInRegistry(TRUE, L"PointsAvailable");
				if (!NT_SUCCESS(Status) && Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"SetDwordValueInRegistry(TRUE, L\"PointsAvailable\") is failed in IOCTL_SEND_CALIBRATION_POINTS with error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
				Status = SetDwordValueInRegistry(Edx->NumberPoints, L"NumberPoints");
				if (!NT_SUCCESS(Status) && Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"SetDwordValueInRegistry(FIVE_CALIBRATION_POINTS, L\"NumberPoints\") is failed in IOCTL_SEND_CALIBRATION_POINTS with error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
			}
			else {
				if (Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"RecordPointsInRegistry(L\"CalibrationPoints\", Edx->NumberPoints) is failed in IOCTL_SEND_CALIBRATION_POINTS with error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
			}
		}
		else {
			if (Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"GetAndCheckCalibrationPoints(Buffer, InLength, Edx->NumberPoints) is failed in IOCTL_SEND_CALIBRATION_POINTS with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			Edx->CheckPoints = FALSE;
			Status = SetDwordValueInRegistry(FALSE, L"PointsAvailable");
			if (!NT_SUCCESS(Status) && Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"SetDwordValueInRegistry(FALSE, L\"RestrictionPoints\") is failed in IOCTL_SEND_CALIBRATION_POINTS with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_IDENTIFY_DEVICE:
		ClearAllDeviceTargeting();
		if (NumberDevices == 1) {
			Status = SetSingleDeviceTarget();
			if (!NT_SUCCESS(Status) && Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"SetSingleDeviceTarget() is failed in IOCTL_IDENTIFY_DEVICE with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		else if (NumberDevices > 1) {
			Interval.QuadPart = -1000 * 1000; // 1 decisecond
			KeSetEvent(&Edx->SyncEvent, IO_NO_INCREMENT, FALSE); // See DispatchRead
			for (ULONG i = 0; i < 10 * WAITING_TIME; ++i) {
				if (KeReadStateEvent(&Edx->SyncEvent)) {
					KeDelayExecutionThread(KernelMode, FALSE, &Interval);
				}
				else {
					break;
				}
			}
			Interval.QuadPart = 0;
			Status = KeWaitForSingleObject((PVOID)&Edx->SyncEvent, Executive, KernelMode, FALSE, &Interval);
			if (Status == STATUS_SUCCESS) {
				Status = DeleteKeyValueInRegistry(&RegistryPathService, L"TargetSectionDriver");
				if (!NT_SUCCESS(Status) && Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"DeleteKeyValueInRegistry(&RegistryPathService, L\"TargetSectionDriver\") is failed in IOCTL_IDENTIFY_DEVICE with error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
				Edx->TargetDevice = NULL;
				Status = STATUS_DEVICE_CONFIGURATION_ERROR;
				if (Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"Device not selected in IOCTL_IDENTIFY_DEVICE error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
			}
			else {
				Status = STATUS_SUCCESS;
			}
		}
		else {
			Status = STATUS_INVALID_DEVICE_STATE;
			if (Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"Undefined state in IOCTL_IDENTIFY_DEVICE error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_STOP_CALIBRATION:
		Edx->Start = FALSE;
		Status = SetDwordValueInRegistry(FALSE, L"StartFlag");
		if (!NT_SUCCESS(Status) && Data && TmpStr) {
			RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
				L"SetDwordValueInRegistry(FALSE, L\"StartFlag\") is failed in IOCTL_STOP_CALIBRATION with error code: 0x%X; ", Status);
			AdhereNewDataToData(Data, OutLength, TmpStr);
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_START_CALIBRATION:
		Edx->Start = TRUE;
		Status = SetDwordValueInRegistry(TRUE, L"StartFlag");
		if (!NT_SUCCESS(Status) && Data && TmpStr) {
			RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
				L"SetDwordValueInRegistry(TRUE, L\"StartFlag\") is failed in IOCTL_START_CALIBRATION with error code: 0x%X; ", Status);
			AdhereNewDataToData(Data, OutLength, TmpStr);
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_REMOVE_DRIVER:
		ClearAllDeviceTargeting();
		Status = GetAndDeleteRMSZValueFromRegistry();
		if (!NT_SUCCESS(Status) && Data && TmpStr) {
			RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
				L"GetAndDeleteRMSZValueFromRegistry() is failed in IOCTL_REMOVE_DRIVER with error code: 0x%X; ", Status);
			AdhereNewDataToData(Data, OutLength, TmpStr);
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		break;
	case IOCTL_GET_STATE_INFORMATION:
		EnumerateAndDeleteTabCalValues(FALSE);
		if (TmpStr) {
			Status = RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
				(NTSTRSAFE_PCWSTR)StateInformation.Buffer, NumberDevices, Edx->Start,
				Edx->CheckPoints, Edx->NumberPoints, Pdx->DeviceActive, Pdx->DeviceTarget,
				Pdx->ActiveLargeTouches, Edx->TabCalValuesInReg
				);
			SendInformation((PWCHAR)TmpStr, Buffer, OutLength, &Info);
		}
		else {
			SendInformation(L"There are not enough resources to allocate memory for a row with information", Buffer, OutLength, &Info);
			Status = STATUS_SUCCESS;
		}
		break;
	case IOCTL_DISABLE_DEVICE:
		if (Edx->TargetDevice) {
			SetAllDeviceActive();
			Pdx->DeviceActive = FALSE;
			Status = SetDwordValueInRegistry(Pdx->IdInstance, L"DisableDeviceId");
			if (!NT_SUCCESS(Status) && Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"SetDwordValueInRegistry(Pdx->IdInstance, L\"DisableDeviceId\") is failed in IOCTL_DISABLE_DEVICE with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		else {
			Status = STATUS_INVALID_DEVICE_REQUEST;
			if (Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"The device to be disconnected is not selected in IOCTL_DISABLE_DEVICE with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_ACTIVATE_DEVICE:
		SetAllDeviceActive();
		Status = DeleteKeyValueInRegistry(&RegistryPathService, L"DisableDeviceId");
		if (!NT_SUCCESS(Status) && Data && TmpStr) {
			RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
				L"DeleteKeyValueInRegistry(&RegistryPathService, L\"DisableDeviceId\") is failed in IOCTL_ACTIVATE_DEVICE with error code: 0x%X; ", Status);
			AdhereNewDataToData(Data, OutLength, TmpStr);
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_RESET_TABCAL:
		Status = EnumerateAndDeleteTabCalValues(TRUE);
		if (!NT_SUCCESS(Status) && Data && TmpStr) {
			RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
				L"EnumerateAndDeleteTabCalValues() is failed in IOCTL_RESET_TABCAL with error code: 0x%X; ", Status);
			AdhereNewDataToData(Data, OutLength, TmpStr);
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_SEND_CALIBRATION_POINTS_HQ:
		Edx->NumberPoints = NINE_CALIBRATION_POINTS;
		Status = GetAndCheckCalibrationPoints(Buffer, InLength, Edx->NumberPoints);
		if (NT_SUCCESS(Status)) {
			Edx->CheckPoints = TRUE;
			Status = RecordPointsInRegistry(L"CalibrationPoints", Edx->NumberPoints);
			if (NT_SUCCESS(Status)) {
				Status = SetDwordValueInRegistry(TRUE, L"PointsAvailable");
				if (!NT_SUCCESS(Status) && Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"SetDwordValueInRegistry(TRUE, L\"PointsAvailable\") is failed in IOCTL_SEND_CALIBRATION_POINTS_HQ with error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
				Status = SetDwordValueInRegistry(Edx->NumberPoints, L"NumberPoints");
				if (!NT_SUCCESS(Status) && Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"SetDwordValueInRegistry(FIVE_CALIBRATION_POINTS, L\"NumberPoints\") is failed in IOCTL_SEND_CALIBRATION_POINTS_HQ with error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
			}
			else {
				if (Data && TmpStr) {
					RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
						L"RecordPointsInRegistry(L\"CalibrationPoints\", Edx->NumberPoints) is failed in IOCTL_SEND_CALIBRATION_POINTS_HQ with error code: 0x%X; ", Status);
					AdhereNewDataToData(Data, OutLength, TmpStr);
				}
			}
		}
		else {
			if (Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"GetAndCheckCalibrationPoints(Buffer, InLength, Edx->NumberPoints) is failed in IOCTL_SEND_CALIBRATION_POINTS_HQ with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			Edx->CheckPoints = FALSE;
			Status = SetDwordValueInRegistry(FALSE, L"PointsAvailable");
			if (!NT_SUCCESS(Status) && Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"SetDwordValueInRegistry(FALSE, L\"RestrictionPoints\") is failed in IOCTL_SEND_CALIBRATION_POINTS_HQ with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}

		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_DISABLE_LARGE_TOUCHES:
		if (Edx->TargetDevice) {
			PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)Edx->TargetDevice->DeviceExtension;
			Pdx->ActiveLargeTouches = FALSE;
			RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"LargeTouches %u", Pdx->IdInstance);
			Status = SetDwordValueInRegistry(FALSE, (PCWSTR)TmpStr);
			if (!NT_SUCCESS(Status) && Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"SetDwordValueInRegistry(FALSE, (PCWSTR)TmpStr) is failed in IOCTL_DISABLE_LARGE_TOUCHES with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		else {
			Status = STATUS_INVALID_DEVICE_REQUEST;
			if (Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"The device that you need to disable large touches is not selected in IOCTL_DISABLE_LARGE_TOUCHES with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	case IOCTL_ACTIVATE_LARGE_TOUCHES:
		if (Edx->TargetDevice) {
			PDEVICE_EXTENSION Pdx = (PDEVICE_EXTENSION)Edx->TargetDevice->DeviceExtension;
			Pdx->ActiveLargeTouches = TRUE;
			RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"LargeTouches %u", Pdx->IdInstance);
			Status = SetDwordValueInRegistry(TRUE, (PCWSTR)TmpStr);
			if (!NT_SUCCESS(Status) && Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"SetDwordValueInRegistry(TRUE, (PCWSTR)TmpStr) is failed in IOCTL_ACTIVATE_LARGE_TOUCHES with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		else {
			Status = STATUS_INVALID_DEVICE_REQUEST;
			if (Data && TmpStr) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE,
					L"The device that you need to activate large touches is not selected in IOCTL_ACTIVATE_LARGE_TOUCHES with error code: 0x%X; ", Status);
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
		}
		if (Data && TmpStr) {
			RtlStringCbLengthW((STRSAFE_PCNZWCH)Data, NTSTRSAFE_MAX_CCH * sizeof(WCHAR), &Size);
			if (Size == Basis.Length) {
				RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"Operation is successful");
				AdhereNewDataToData(Data, OutLength, TmpStr);
			}
			SendInformation(Data, Buffer, OutLength, &Info);
		}
		Status = STATUS_SUCCESS;
		break;
	default:
		Status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	if (Data) {
		ExFreePoolWithTag((PVOID)Data, 'data');
	}

	if (TmpStr) {
		ExFreePoolWithTag((PVOID)TmpStr, 'tmp');
	}
	
	IoReleaseRemoveLock(&Edx->RemoveLock, Irp);

	return CompleteRequest(Irp, Status, Info);
}

NTSTATUS DispatchCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp) 
{
	NTSTATUS Status;
	PCOMMON_DEVICE_EXTENSION Cdx = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	LARGE_INTEGER Timeout;

	if (Cdx->flag == EDO_EXTENSION) {
		Status = IoAcquireRemoveLock(&Edx->RemoveLock, Irp);
		if (!NT_SUCCESS(Status)) {
			return CompleteRequest(Irp, Status, 0);
		}
		Timeout.QuadPart = 0;
		Status = KeWaitForSingleObject((PVOID)&Edx->SyncCloseCreateEvent, Executive, KernelMode, FALSE, &Timeout);
		if (Status == STATUS_TIMEOUT) {
			Status = STATUS_DELETE_PENDING;
		}
		CompleteRequest(Irp, Status, 0);
		IoReleaseRemoveLock(&Edx->RemoveLock, Irp);
		return Status;
	}

	return DispatchAny(DeviceObject, Irp);
}

NTSTATUS DispatchClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	NTSTATUS Status;
	PCOMMON_DEVICE_EXTENSION Cdx = (PCOMMON_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
	PEXTRA_DEVICE_EXTENSION Edx = (PEXTRA_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

	if (Cdx->flag == EDO_EXTENSION) {
		KeSetEvent(&Edx->SyncCloseCreateEvent, IO_NO_INCREMENT, FALSE);
		Status = STATUS_SUCCESS;
		CompleteRequest(Irp, Status, 0);
		return Status;
	}

	return DispatchAny(DeviceObject, Irp);
}

BOOLEAN CheckDeviceProperty(PDEVICE_OBJECT Pdo) 
{
	NTSTATUS Status;
	BOOLEAN check = TRUE;
	UNICODE_STRING probeHardwareID;
	UNICODE_STRING probeDeviceDescription;
	UINT8 Buffer[0x400];
	ULONG ReturnLength;

	*(WCHAR*)Buffer = 0;
	Status = IoGetDeviceProperty(Pdo, DevicePropertyHardwareID, sizeof(Buffer), &Buffer, &ReturnLength);
	if (NT_SUCCESS(Status)) {
		RtlInitUnicodeString(&probeHardwareID, (WCHAR*)Buffer);
		if (probeHardwareID.Length < SoughtHardwareID.Length) {
			check = FALSE;
			return check;
		}

		for (UINT32 i = 0; i < SoughtHardwareID.Length / sizeof(WCHAR); ++i) {
			check = check && (*(probeHardwareID.Buffer + i) == *(SoughtHardwareID.Buffer + i));
			if (!check) {
				return check;
			}
		}
	}
	else {
		check = FALSE;
		return check;
	}
	
	*(WCHAR*)Buffer = 0;
	Status = IoGetDeviceProperty(Pdo, DevicePropertyDeviceDescription, sizeof(Buffer), &Buffer, &ReturnLength);
	if (NT_SUCCESS(Status)) {
		RtlInitUnicodeString(&probeDeviceDescription, (WCHAR*)Buffer);
		if (probeDeviceDescription.Length < SoughtDeviceDescription.Length) {
			check = FALSE;
			return check;
		}

		for (UINT32 i = 0; i < SoughtDeviceDescription.Length / sizeof(WCHAR); ++i) {
			check = check && (*(probeDeviceDescription.Buffer + i) == *(SoughtDeviceDescription.Buffer + i));
			if (!check) {
				return check;
			}
		}
	}
	else {
		check = FALSE;
		return check;
	}

	return check;
}

ULONG GetDeviceTypeToUse(PDEVICE_OBJECT Pdo)
{
	PDEVICE_OBJECT Tdo = IoGetAttachedDeviceReference(Pdo);
	ULONG DeviceType;

	if (!Tdo)
		return FILE_DEVICE_UNKNOWN;
	DeviceType = Tdo->DeviceType;
	ObDereferenceObject(Tdo);
	return DeviceType;
}

NTSTATUS CreateEDO(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS Status;
	PEXTRA_DEVICE_EXTENSION Edx = NULL;
	UNICODE_STRING EDOName = RTL_CONSTANT_STRING(L"\\Device\\TouchscreenFilterConfigDevice");
	ULONG Start = 0, CheckPoints = 0, NumberPoints = 0;

	Status = IoCreateDeviceSecure(DriverObject, sizeof(EXTRA_DEVICE_EXTENSION), 
		&EDOName, FILE_DEVICE_UNKNOWN, 0, TRUE, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL, &EdoGuid, &Edo);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}
	
	RtlZeroMemory(Edo->DeviceExtension, sizeof(EXTRA_DEVICE_EXTENSION));
	Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	RtlInitUnicodeString(&Edx->EDOSymLinkName, L"\\??\\TouchscreenFilterConfigDeviceLink");
	
	Status = IoCreateSymbolicLink(&Edx->EDOSymLinkName, &EDOName);
	if (!NT_SUCCESS(Status)) {
		IoDeleteDevice(Edo);
		return Status;
	}

	IoInitializeRemoveLock(&Edx->RemoveLock, 'VZ', 0, 0);
	Status = IoAcquireRemoveLock(&Edx->RemoveLock, NULL);
	if (!NT_SUCCESS(Status)) {
		IoDeleteSymbolicLink(&Edx->EDOSymLinkName);
		IoDeleteDevice(Edo);
		return Status;
	}
	KeInitializeEvent(&Edx->SyncEvent, SynchronizationEvent, FALSE);
	KeInitializeEvent(&Edx->SyncCloseCreateEvent, SynchronizationEvent, TRUE);
	Edx->flag = EDO_EXTENSION;
	Edx->DriverObject = DriverObject;
	Edx->CheckPoints = FALSE; 
	Edx->Start = FALSE;
	Edx->TargetDevice = NULL;
	Edx->NumberPoints = 0;
	Edx->TabCalValuesInReg = FALSE;

	Status = GetDwordValueFromRegistry(&NumberPoints, L"NumberPoints");
	if (NT_SUCCESS(Status)) {
		Edx->NumberPoints = NumberPoints;
	}

	Status = GetDwordValueFromRegistry(&CheckPoints, L"PointsAvailable");
	if (NT_SUCCESS(Status)) {
		Edx->CheckPoints = CheckPoints;
	}

	if (Edx->CheckPoints && Edx->NumberPoints) {
		Status = RetrievePointsFromRegistry(L"CalibrationPoints", Edx->NumberPoints);
		if (!NT_SUCCESS(Status)) {
			Edx->CheckPoints = FALSE;
		}
	}

	Status = GetDwordValueFromRegistry(&Start, L"StartFlag");
	if (NT_SUCCESS(Status)) {
		Edx->Start = Start;
	}

	Edo->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

NTSTATUS GetIdInstance(PDEVICE_OBJECT Pdo, PULONG IdInstance)
{
	NTSTATUS Status;
	UINT8 Buffer[0x400];
	ULONG ReturnLength;
	UNICODE_STRING IdStr;

	*(WCHAR*)Buffer = 0;
	Status = IoGetDeviceProperty(Pdo, DevicePropertyDriverKeyName, sizeof(Buffer), &Buffer, &ReturnLength);
	if (!NT_SUCCESS(Status)) {
		return Status;
	}

	RtlInitUnicodeString(&IdStr, (PCWSTR)(Buffer + ReturnLength - 10));
	RtlUnicodeStringToInteger(&IdStr, 0, IdInstance);

	return Status;
}

NTSTATUS AddDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT Pdo)
{
	NTSTATUS Status;
	PDEVICE_OBJECT Fido = NULL;
	PDEVICE_EXTENSION Pdx = NULL;
	PEXTRA_DEVICE_EXTENSION Edx = NULL;
	ULONG TmpValue = 0;
	PWCHAR TmpStr = NULL;

	if (!CheckDeviceProperty(Pdo)) {
		return STATUS_OBJECT_TYPE_MISMATCH;
	}

	if (!Edo) {
		Status = CreateEDO(DriverObject);
		if (!NT_SUCCESS(Status)) {
			return Status;
		}
		if (!Edo) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	}
	
	Edx = (PEXTRA_DEVICE_EXTENSION)Edo->DeviceExtension;
	
	Status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), NULL, GetDeviceTypeToUse(Pdo), 0, FALSE, &Fido);
	if (!NT_SUCCESS(Status)) {
		IoReleaseRemoveLock(&Edx->RemoveLock, NULL);
		IoDeleteSymbolicLink(&Edx->EDOSymLinkName);
		IoDeleteDevice(Edo);
		return Status;
	}
	InterlockedIncrement(&NumberDevices); 

	RtlZeroMemory(Fido->DeviceExtension, sizeof(DEVICE_EXTENSION));
	Pdx = (PDEVICE_EXTENSION)Fido->DeviceExtension;
	IoInitializeRemoveLock(&Pdx->RemoveLock, 'ZV', 0, 0);

	Pdx->DriverObject = DriverObject;
	Pdx->Fido = Fido;
	Pdx->Pdo = Pdo;
	Pdx->flag = FIDO_EXTENSION;
	Pdx->DeviceTarget = FALSE;
	Pdx->DeviceActive = TRUE;
	Pdx->ActiveLargeTouches = TRUE;
	Status = GetIdInstance(Pdx->Pdo, &Pdx->IdInstance);
	if (!NT_SUCCESS(Status)) {
		IoReleaseRemoveLock(&Edx->RemoveLock, NULL);
		IoDeleteSymbolicLink(&Edx->EDOSymLinkName);
		IoDeleteDevice(Edo);
		IoDeleteDevice(Fido);
		InterlockedDecrement(&NumberDevices);
		return Status;
	}

	TmpStr = (PWCHAR)ExAllocatePool2(POOL_FLAG_PAGED, TMP_BUFFER_SIZE, 'tmp');
	if (TmpStr) {
		RtlStringCbPrintfW((NTSTRSAFE_PWSTR)TmpStr, TMP_BUFFER_SIZE, L"LargeTouches %u", Pdx->IdInstance);
		Status = GetDwordValueFromRegistry(&TmpValue, (PCWSTR)TmpStr);
		if (NT_SUCCESS(Status)) {
			Pdx->ActiveLargeTouches = TmpValue;
		}
		ExFreePoolWithTag((PVOID)TmpStr, 'tmp');
	}
	
	Status = GetDwordValueFromRegistry(&TmpValue, L"DisableDeviceId");
	if (NT_SUCCESS(Status) && Pdx->IdInstance == TmpValue) {
		Pdx->DeviceActive = FALSE;
	}

	Status = GetDwordValueFromRegistry(&TmpValue, L"TargetSectionDriver");
	if (NT_SUCCESS(Status) && Pdx->IdInstance == TmpValue) {
		Pdx->DeviceTarget = TRUE;
		Edx->TargetDevice = Fido;
	}

	Status = IoAttachDeviceToDeviceStackSafe(Fido, Pdo, &Pdx->LowerDevice);
	if (!NT_SUCCESS(Status)) {
		IoReleaseRemoveLock(&Edx->RemoveLock, NULL);
		IoDeleteSymbolicLink(&Edx->EDOSymLinkName);
		IoDeleteDevice(Edo);
		IoDeleteDevice(Fido);
		InterlockedDecrement(&NumberDevices);
		return Status;
	}

	Fido->Flags |= Pdx->LowerDevice->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO | DO_POWER_PAGABLE);
	Fido->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS; 
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	PVOID Buffer = ExAllocatePool2(POOL_FLAG_NON_PAGED, RegistryPath->Length + sizeof(WCHAR), 'ger'); // See DriverUnload

	if (Buffer) {
		RtlStringCbCopyW((NTSTRSAFE_PWSTR)Buffer, RegistryPath->Length + sizeof(WCHAR), (NTSTRSAFE_PCWSTR)RegistryPath->Buffer);
		RtlInitUnicodeString(&RegistryPathService, (PCWSTR)Buffer);
	}
	else {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	for (UINT32 i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; ++i) {
		DriverObject->MajorFunction[i] = DispatchAny;
	}
	DriverObject->DriverUnload = DriverUnload;
	DriverObject->DriverExtension->AddDevice = AddDevice;
	DriverObject->MajorFunction[IRP_MJ_READ] = DispatchRead;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DispatchCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DispatchClose;
	DriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDevCTL;

	return STATUS_SUCCESS;
}
