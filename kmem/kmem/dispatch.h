#pragma once

#define MAX_VIRTUAL_USERMODE 0x7FFFFFFFFFFF
#define MIN_VIRTUAL_USERMODE 0x10000

void* originalFunction = 0;
typedef NTSTATUS(__stdcall* fnOriginal)(PDEVICE_OBJECT device, PIRP irp);

void HandleCommand(Command* cmd)
{
    if (cmd->TargetAddress > MAX_VIRTUAL_USERMODE)
        return;
    if (cmd->TargetAddress < MIN_VIRTUAL_USERMODE)
        return;
    if (cmd->SourceAddress > MAX_VIRTUAL_USERMODE)
        return;
    if (cmd->SourceAddress < MIN_VIRTUAL_USERMODE)
        return;

	PEPROCESS sourceProcess;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)cmd->Source, &sourceProcess);
    if (!NT_SUCCESS(status))
        return;

    PEPROCESS targetProcess;
    status = PsLookupProcessByProcessId((HANDLE)cmd->Target, &targetProcess);
    if (!NT_SUCCESS(status))
        return;    

    SIZE_T dummySize = 0;
    MmCopyVirtualMemory(sourceProcess, (PVOID)cmd->SourceAddress, targetProcess, (PVOID)cmd->TargetAddress, cmd->Size, UserMode, &dummySize);

    ObDereferenceObject(sourceProcess);
    ObDereferenceObject(targetProcess);
}

void HandleModInfo(ModInfo* info)
{
    /*PEPROCESS sourceProcess;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)input->Source, &sourceProcess);
    if (!NT_SUCCESS(status))
        return;*/
    
    PEPROCESS targetProcess;
    NTSTATUS status = PsLookupProcessByProcessId((HANDLE)info->Target, &targetProcess);
    if (!NT_SUCCESS(status))
        return;

    wchar_t moduleName[256];
    memcpy(moduleName, info->Name, 256);

    UNICODE_STRING moduleNameStr = { 0 };
    RtlInitUnicodeString(&moduleNameStr, moduleName);

    DWORD64 dllBaseAddress = 0;
    ULONG dllSize = 0;

    KAPC_STATE state;
    KeStackAttachProcess(targetProcess, &state);

    PPEB peb = PsGetProcessPeb(targetProcess);
    for (PLIST_ENTRY pListEntry = peb->Ldr->InMemoryOrderModuleList.Flink; pListEntry != &peb->Ldr->InMemoryOrderModuleList; pListEntry = pListEntry->Flink)
    {
        PLDR_DATA_TABLE_ENTRY entry = CONTAINING_RECORD(pListEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

        if (RtlCompareUnicodeString(&entry->BaseDllName, &moduleNameStr, TRUE) == 0)
        {
            dllBaseAddress = (DWORD64)entry->DllBase;
            dllSize = entry->SizeOfImage;
            break;
        }
    }
	
    KeUnstackDetachProcess(&state);

    ObDereferenceObject(targetProcess);

    info->BaseAddress = dllBaseAddress;
    info->Size = dllSize;
}

NTSTATUS Hooked(PDEVICE_OBJECT device, PIRP irp)
{
    fnOriginal original = (fnOriginal)originalFunction;

    PIO_STACK_LOCATION ioc = IoGetCurrentIrpStackLocation(irp);
    ULONG code = ioc->Parameters.DeviceIoControl.IoControlCode;

    if (code == IOCTL_COMMAND)
    {
        Command* command = (Command*)irp->AssociatedIrp.SystemBuffer;

        HandleCommand(command);

        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = sizeof(Command);
        IoCompleteRequest(irp, IO_NO_INCREMENT);

        return STATUS_SUCCESS;
    }

	if (code == IOCTL_MODINFO)
	{
        ModInfo* info = (ModInfo*)irp->AssociatedIrp.SystemBuffer;

        HandleModInfo(info);

        irp->IoStatus.Status = STATUS_SUCCESS;
        irp->IoStatus.Information = sizeof(ModInfo);
        IoCompleteRequest(irp, IO_NO_INCREMENT);

        return STATUS_SUCCESS;
	}

    return original(device, irp);
}