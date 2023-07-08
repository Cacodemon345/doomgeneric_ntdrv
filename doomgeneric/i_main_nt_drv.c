#include <ntddk.h>
#include <wdm.h>

#define NT_DEVICE_NAME      L"\\Device\\DoomGeneric"

static PDEVICE_OBJECT deviceObject = NULL;
static HANDLE threadHandle = NULL;

extern int __cdecl main(int argc, char **argv, char** envp, unsigned long debflag);

__drv_dispatchType(IRP_MJ_CREATE)
__drv_dispatchType(IRP_MJ_CLOSE)
DRIVER_DISPATCH DoomGenericCreateClose;

DRIVER_INITIALIZE DriverEntry;

__drv_dispatchType(IRP_MJ_DEVICE_CONTROL)
DRIVER_DISPATCH DoomGenericDeviceControl;

DRIVER_UNLOAD DoomGenericUnloadDriver;

#define DOOMGENERIC_TYPE 40002

#define IOCTL_DOOMGENERIC_INIT_DOOM \
	CTL_CODE( DOOMGENERIC_TYPE, 0x850, METHOD_NEITHER, FILE_ANY_ACCESS )
	
#define IOCTL_DOOMGENERIC_WAIT_DOOM \
	CTL_CODE( DOOMGENERIC_TYPE, 0x851, METHOD_NEITHER, FILE_ANY_ACCESS )


void DoomGenericUnloadDriver(PDRIVER_OBJECT pDriverObject)
{
	if (deviceObject) IoDeleteDevice(deviceObject);
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath)
{	NTSTATUS Status;
    ULONG BytesReturned;
	UNICODE_STRING ntUnicodeString;
	
	RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);
	Status = IoCreateDevice(pDriverObject, 0ul, &ntUnicodeString, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
	if (!NT_SUCCESS(Status))
	{
		return Status;
	}
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = DoomGenericCreateClose;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = DoomGenericCreateClose;
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DoomGenericDeviceControl;
	pDriverObject->DriverUnload = DoomGenericUnloadDriver;
    return STATUS_SUCCESS;
}

NTSTATUS DoomGenericCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest( Irp, IO_NO_INCREMENT );

    return STATUS_SUCCESS;
}

char* static_args[] = { "doomgeneric", "-iwad", "\\??\\C:\\Windows\\doom2.wad", "-cdrom", NULL };

void DoomGenericThread(void* parm)
{
    main(4, static_args, NULL, 0);
}

NTSTATUS DoomGenericDeviceControl(PDEVICE_OBJECT devObj, PIRP Irp)
{
	PIO_STACK_LOCATION irpSp;
	ULONG inputBufferLength;
	PCHAR inputBuffer;
	PULONG inputBufferLong;
	NTSTATUS Status = STATUS_SUCCESS;
	UINT32 i;
	
	UNREFERENCED_PARAMETER(devObj);
	
	irpSp = IoGetCurrentIrpStackLocation(Irp);
	
	if (!irpSp)
	{
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto End;
	}

	switch(irpSp->Parameters.DeviceIoControl.IoControlCode)
	{
        case IOCTL_DOOMGENERIC_INIT_DOOM:
        {
            OBJECT_ATTRIBUTES ObjectAttributes;
            InitializeObjectAttributes(&ObjectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
            Status = PsCreateSystemThread(&threadHandle, GENERIC_READ | GENERIC_WRITE | SYNCHRONIZE, &ObjectAttributes, NULL, NULL, DoomGenericThread, NULL);
            break;
        }
        case IOCTL_DOOMGENERIC_WAIT_DOOM:
        {
            break;
        }
        default:
        {
            Status = STATUS_INVALID_DEVICE_REQUEST;
            break;
        }
    }

End:
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}