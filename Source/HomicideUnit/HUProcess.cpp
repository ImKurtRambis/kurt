#include "HUProcess.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"

// one job for the whole game session; Windows closes our handle to it when the game process ends, however it ends,
// and KILL_ON_JOB_CLOSE then ends everything inside
static HANDLE GameJob()
{
	static HANDLE Job = []() -> HANDLE
	{
		HANDLE J = ::CreateJobObjectW(nullptr, nullptr);
		if (!J) return nullptr;
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION Info = {};
		Info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		if (!::SetInformationJobObject(J, JobObjectExtendedLimitInformation, &Info, sizeof(Info))) { ::CloseHandle(J); return nullptr; }
		return J;
	}();
	return Job;
}
#endif

FProcHandle HUProcess::Launch(const FString& Exe, const FString& Args, const FString& WorkDir)
{
#if PLATFORM_WINDOWS
	FString Cmd = FString::Printf(TEXT("\"%s\" %s"), *Exe, *Args);
	STARTUPINFOW Si = {};
	Si.cb = sizeof(Si);
	Si.dwFlags = STARTF_USESHOWWINDOW;
	Si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION Pi = {};
	// suspended: into the job first, then let it run
	if (!::CreateProcessW(nullptr, Cmd.GetCharArray().GetData(), nullptr, nullptr, 0, CREATE_SUSPENDED | CREATE_NO_WINDOW,
		nullptr, WorkDir.IsEmpty() ? nullptr : *WorkDir, &Si, &Pi))
	{
		UE_LOG(LogTemp, Warning, TEXT("HomicideUnit: couldn't start %s (error %u)"), *FPaths::GetCleanFilename(Exe), ::GetLastError());
		return FProcHandle();
	}
	HANDLE J = GameJob();
	if (!J || !::AssignProcessToJobObject(J, Pi.hProcess))
		UE_LOG(LogTemp, Warning, TEXT("HomicideUnit: %s isn't tied to the game (error %u) - it may outlive a crash"), *FPaths::GetCleanFilename(Exe), ::GetLastError());
	::ResumeThread(Pi.hThread);
	::CloseHandle(Pi.hThread);
	return FProcHandle(Pi.hProcess);
#else
	return FPlatformProcess::CreateProc(*Exe, *Args, false, true, true, nullptr, 0, WorkDir.IsEmpty() ? nullptr : *WorkDir, nullptr);
#endif
}
