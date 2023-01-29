/*
	Copyright (c) 2020 Samuel Tulach

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.
 */

#include <ntifs.h>
#include <ntimage.h>
#include <minwindef.h>
#include <intrin.h>

#include "shared.h"
#include "utils.h"
#include "ntos.h"
#include "scan.h"
#include "dispatch.h"

PVOID GetShellCode(DWORD64 hooked)
{
	/*
		var_18= qword ptr -18h
		arg_0= qword ptr  8
		arg_8= qword ptr  10h

		mov     [rsp+arg_8], rdx
		mov     [rsp+arg_0], rcx
		sub     rsp, 38h
		mov     rax, 7FFFFFFFFFFFFFFFh
		mov     [rsp+38h+var_18], rax
		mov     rdx, [rsp+38h+arg_8]
		mov     rcx, [rsp+38h+arg_0]
		call    [rsp+38h+var_18]
		add     rsp, 38h
		retn
	*/
	const char* code = "\x48\x89\x54\x24\x10\x48\x89\x4C\x24\x08\x48\x83\xEC\x38\x48"
					   "\xB8\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F\x48\x89\x44\x24\x20\x48"
					   "\x8B\x54\x24\x48\x48\x8B\x4C\x24\x40\xFF\x54\x24\x20\x48\x83"
					   "\xC4\x38\xC3";

    PVOID codeBuffer = ExAllocatePool(NonPagedPool, 49);
    if (!codeBuffer)
        return 0;

    memcpy(codeBuffer, code, 49);

    *(DWORD64*)((DWORD64)codeBuffer + 16) = hooked;

    return codeBuffer;
}

extern "C" NTSTATUS CustomEntry(void* dummy1, void* dummy2)
{
    UNREFERENCED_PARAMETER(dummy1);
    UNREFERENCED_PARAMETER(dummy2);

	DWORD64 driverBase = FindTargetModule("nsiproxy.sys");
	if (!driverBase)
		return CSTATUS_MODULE_NOT_FOUND;

	PVOID targetFunction = FindPatternImage((PCHAR)driverBase, "\x48\x89\x5C\x24\x00\x48\x89\x74\x24\x00\x55\x57\x41\x56\x48\x8D\x6C\x24\x00\x48\x81\xEC\x00\x00\x00\x00\x48\x8B\x05", "xxxx?xxxx?xxxxxxxx?xxx????xxx");
	if (!targetFunction)
		return CSTATUS_SIG_FAILED;

	UNICODE_STRING driverName = RTL_CONSTANT_STRING(L"\\Driver\\nsiproxy");

	PDRIVER_OBJECT object = 0;
	NTSTATUS status = ObReferenceObjectByName(&driverName, OBJ_CASE_INSENSITIVE, 0, 0, *IoDriverObjectType, KernelMode, 0, (PVOID*)&object);
	if (!NT_SUCCESS(status) || !object)
	{
		return STATUS_NOT_FOUND;
	}

	originalFunction = (void*)object->MajorFunction[IRP_MJ_DEVICE_CONTROL];
	if (INVALID_POINTER(originalFunction))
		return CSTATUS_DRIVER_NOT_FOUND;

	PVOID shellcode = GetShellCode((DWORD64)Hooked);

	_disable();

	unsigned long long cr0 = __readcr0();
	unsigned long long originalCr0 = cr0;
	cr0 &= ~(1UL << 16);
	__writecr0(cr0);

	memcpy(targetFunction, shellcode, 49);

	__writecr0(originalCr0);
	_enable();

	object->MajorFunction[IRP_MJ_DEVICE_CONTROL] = (PDRIVER_DISPATCH)targetFunction;

	ExFreePool(shellcode);
	return STATUS_SUCCESS;
}