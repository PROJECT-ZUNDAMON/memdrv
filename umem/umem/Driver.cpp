#include <Windows.h>
#include "Driver.h"
#include <cstdio>

void Driver::SendCommand(Command* cmd)
{
	DWORD returned = 0;
	bool status = DeviceIoControl(DriverHandle, IOCTL_COMMAND, cmd, sizeof(Command), cmd, sizeof(Command), &returned, nullptr);

	if (!status)
	{
		printf("[BENCHMARK] GetLastError(): %x\n", GetLastError());
	}	
}

void Driver::CopyVirtual(bool read, uint64_t destination, uint64_t source, SIZE_T size)
{	
	Command cmd;
	cmd.Source = read ? TargetProcessPid : CurrentProcessPid;
	cmd.Target = read ? CurrentProcessPid : TargetProcessPid;
	cmd.SourceAddress = source;
	cmd.TargetAddress = destination;
	cmd.Size = size;

	SendCommand(&cmd);
}

bool Driver::Init(int targetPid)
{
	DriverHandle = CreateFileA("\\\\.\\nsi", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, NULL, nullptr);
	if (DriverHandle == INVALID_HANDLE_VALUE)
		return false;

	CurrentProcessPid = GetCurrentProcessId();
	TargetProcessPid = targetPid;

	return true;
}

DWORD64 Driver::GetModuleInfo(PCWCHAR moduleName, ULONG* size)
{
	ModInfo info = { 0 };
	info.Target = TargetProcessPid;
	memcpy(info.Name, moduleName, wcslen(moduleName) * sizeof(wchar_t));

	DWORD returned = 0;
	bool status = DeviceIoControl(DriverHandle, IOCTL_MODINFO, &info, sizeof(ModInfo), &info, sizeof(ModInfo), &returned, nullptr);

	if (!status)
		return 0;

	if (size)
		*size = info.Size;

	return info.BaseAddress;
}

void Driver::Close()
{
	CloseHandle(DriverHandle);
}
