// Starting the game's helper programs: the suspects' brain (llama-server), the speech server (sherpa-onnx) and the
// local voices (Tools\VoiceServer.bat -> Python -> a second llama-server).
//
// They're tied to the game through a Windows job object: when the game exits - normally, by crashing, or killed from
// Task Manager - Windows ends every one of them and anything they started, so nothing is left holding the graphics
// card or a port. Each program is created suspended and put in the job before it runs a single instruction, so even
// children it starts straight away (cmd -> python) are inside.
#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

namespace HUProcess
{
	// Exe and Args as for FPlatformProcess::CreateProc; hidden, no console window. Invalid handle if it couldn't start.
	HOMICIDEUNIT_API FProcHandle Launch(const FString& Exe, const FString& Args, const FString& WorkDir);
}
