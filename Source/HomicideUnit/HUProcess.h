// Starting the game's helper programs: the suspects' brain (llama-server), the speech server (sherpa-onnx) and the
// local voice (sherpa-onnx's speaking program, run once for each line).
//
// They're tied to the game through a Windows job object: when the game exits - normally, by crashing, or killed from
// Task Manager - Windows ends every one of them and anything they started, so nothing is left holding the graphics
// card or a port. Each program is created suspended and put in the job before it runs a single instruction, so even
// children it starts straight away are inside.
#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

namespace HUProcess
{
	// Exe and Args as for FPlatformProcess::CreateProc; hidden, no console window. Invalid handle if it couldn't start.
	// bGiveWay: work that can wait (a voice line made for later) - Windows runs it only on processor time the game
	// isn't using. Safe to call from any thread.
	HOMICIDEUNIT_API FProcHandle Launch(const FString& Exe, const FString& Args, const FString& WorkDir, bool bGiveWay = false);
}
