#pragma once

#define SYSTEM_MODULE_INFORMATION 11

DWORD64 FindTargetModule(const char* inputName)
{
	void* buffer = ExAllocatePool(NonPagedPool, 8);
	ULONG bufferSize = 0;

	NTSTATUS status = ZwQuerySystemInformation(SYSTEM_MODULE_INFORMATION, buffer, bufferSize, &bufferSize);

	while (status == STATUS_INFO_LENGTH_MISMATCH)
	{
		ExFreePool(buffer);

		buffer = ExAllocatePool(NonPagedPool, bufferSize);
		status = ZwQuerySystemInformation(SYSTEM_MODULE_INFORMATION, buffer, bufferSize, &bufferSize);
	}

	if (!NT_SUCCESS(status))
	{
		ExFreePool(buffer);
		return 0;
	}

	PRTL_PROCESS_MODULES modules = (PRTL_PROCESS_MODULES)buffer;

	for (ULONG i = 0; i < modules->NumberOfModules; ++i)
	{
		const char* moduleName = (char*)modules->Modules[i].FullPathName + modules->Modules[i].OffsetToFileName;

		if (strcmp(moduleName, inputName) == 0)
		{
			DWORD64 result = (DWORD64)modules->Modules[i].ImageBase;

			ExFreePool(buffer);
			return result;
		}
	}

	ExFreePool(buffer);
	return 0;
}

BOOL CheckMask(PCHAR base, PCHAR pattern, PCHAR mask)
{
	for (; *mask; ++base, ++pattern, ++mask)
	{
		if (*mask == 'x' && *base != *pattern)
		{
			return FALSE;
		}
	}

	return TRUE;
}

PVOID FindPattern(PCHAR base, DWORD length, PCHAR pattern, PCHAR mask)
{
	length -= (DWORD)strlen(mask);
	for (DWORD i = 0; i <= length; ++i)
	{
		PVOID addr = &base[i];
		if (CheckMask((PCHAR)addr, pattern, mask))
		{
			return addr;
		}
	}

	return 0;
}

PVOID FindPatternImage(PCHAR base, PCHAR pattern, PCHAR mask)
{
	PVOID match = 0;

	PIMAGE_NT_HEADERS headers = (PIMAGE_NT_HEADERS)(base + ((PIMAGE_DOS_HEADER)base)->e_lfanew);
	PIMAGE_SECTION_HEADER sections = IMAGE_FIRST_SECTION(headers);
	for (DWORD i = 0; i < headers->FileHeader.NumberOfSections; ++i)
	{
		PIMAGE_SECTION_HEADER section = &sections[i];
		if (*(PINT)section->Name == 'EGAP' || memcmp(section->Name, ".text", 5) == 0)
		{
			match = FindPattern(base + section->VirtualAddress, section->Misc.VirtualSize, pattern, mask);
			if (match)
			{
				break;
			}
		}
	}

	return match;
}