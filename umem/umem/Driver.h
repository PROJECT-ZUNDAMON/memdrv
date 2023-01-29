#pragma once
#include <cstdint>
#include "Shared.h"

class Driver
{
private:
	HANDLE DriverHandle;
	int CurrentProcessPid;
	int TargetProcessPid;
public:
	void SendCommand(Command* cmd);
	void CopyVirtual(bool read, uint64_t destination, uint64_t source, SIZE_T size);
	bool Init(int targetPid);
	DWORD64 GetModuleInfo(PCWCHAR moduleName, ULONG* size);
	void Close();

	template<typename T>
	T Read(uint64_t address)
	{
		T val = T();
		CopyVirtual(true, reinterpret_cast<uint64_t>(&val), address, sizeof(T));
		return val;
	}

	template<typename T>
	void Write(uint64_t address, T val)
	{
		CopyVirtual(false, address, reinterpret_cast<uint64_t>(&val), sizeof(T));
	}
};
