Homicide Unit - the local AI: the suspects' brain, their voice, and the detective's ears
(free to play: everything runs on this PC, no internet, no keys)

ONE OF EACH, THE SAME ON EVERY PC. One brain, one voice, one pair of ears. Nothing is picked by how big the graphics
card is, so the game thinks and sounds the same for every player - and it is sized for an 8 GB card (an RTX 2070 Super).
  The brain    one model, on the graphics card - NVIDIA, AMD or Intel, through Vulkan. It takes about 3.1 GB of the
               card's memory; the game beside it takes 1.5 to 2.1 GB at 1080p. On the processor when no card can run it.
  The voice    one model, on the processor. None of the card's memory.
  The ears     one model, on the processor.
  A player's download: 3.2 GB of AI. Tools\Package.ps1 copies only the folders marked SHIPPED below.
  To force where the brain runs (testing, or a launch option for a player whose card misbehaves):  -HUBrain=card|cpu
  Measured on an RTX 5070 Ti + Ryzen 7 7800X3D, the only PC it has run on (16 Nov 2026), drawing the crime scene at
  1920x1080 and 60 frames a second:
      the game, the brain and the voice together use 4.9 GB of the card (5.3 GB at the most);
      the brain writes about 105 tokens a second beside the game: half a second an answer;
      the voice says a line in 1.2 to 1.7 s.
  An RTX 2070 Super has half that card's memory speed, so its brain should take about a second an answer.
  No RTX 2070 Super, and no AMD or Intel card, has run it yet. SYSTEM_REQUIREMENTS.md has the numbers.
  The brain and the picture share the card: answers written ahead rest between them as long as each took, so a game
  that drew 100-135 frames a second kept 89-122 while they were written (52-65 when they were written back to back).

THE BRAIN. SHIPPED: AI\llama, AI\models\suspect.gguf. Put these here (they're big, so they're not in git):
  AI\llama\llama-server.exe   + its DLLs   llama.cpp's VULKAN build for Windows (MIT), from github.com/ggml-org/llama.cpp releases:
                                           llama-b<number>-bin-win-vulkan-x64.zip (build 12170 is the one installed).
                                           One build for every card. Its CUDA build was measured too: no faster on the RTX 5070 Ti
                                           (175 against 190 tokens a second), 1.2 GB instead of 100 MB, and NVIDIA only.
  AI\models\suspect.gguf                   Qwen3 4B Instruct 2507, Q4_K_M (Apache-2.0), 2.4 GB

Without them the game still works: suspects answer typed questions with written lines.
Walk up to a suspect, hold V and talk - no menu (call a name, "Walter, where were you?", to talk to someone you aren't
facing). E is the sit-down interview: there, T types instead. The model only voices the suspect - the game decides every fact.
The game's log says where the model runs and how fast ("HomicideUnit brain: ready on the graphics card ... 193 tokens a second",
then "answered in 0.24 s ..." for every answer).

What a suspect says, cheapest first - most lines should cost no wait:
  1. written by hand   a hello gets their introduction; and the six questions every detective asks (where were you, did
                       you see anything, who can vouch for you, how did you know the victim, who did it, did you do it)
                       are answered from what they've already told the detective whenever the model has nothing ready.
                       This is also the floor for a PC that can't run the model at all: the case still plays.
  2. written ahead     once a case starts, the model writes its own answers to those six for every suspect, in the
                       background, so asking one is instant.
  3. thought up live   everything else.
  The log keeps count: "HomicideUnit brain: answers this case: 3 written by hand, 5 written ahead, 2 thought up live".

THE VOICE (free, offline, no keys). SHIPPED: AI\voice\supertonic, and from AI\voice\bin the three programs the game runs.
  Supertonic 3 (code MIT, model BigScience OpenRAIL-M) - ten voices: 0-4 are women, 5-9 men
      AI\voice\supertonic\   duration_predictor, text_encoder, vector_estimator, vocoder (.int8.onnx), tts.json, unicode_indexer.bin, voice.bin
          from github.com/k2-fsa/sherpa-onnx releases "tts-models": sherpa-onnx-supertonic-3-tts-int8-2026-05-11.tar.bz2 (139 MB)
          Licence: selling the game is allowed, but the model's use rules must travel with it. Keep both LICENSE files in
          that folder, and name them in the game's licence text and credits ("Supertonic 3 by Supertone Inc.").
      AI\voice\bin\sherpa-onnx-offline-tts.exe + onnxruntime*.dll   sherpa-onnx 1.13.8 (Apache-2.0), from its GitHub releases
  The game runs that program itself, once for each line (HUVoiceLocal.cpp). No Python, no server, no port, no graphics
  card. A line takes 1.2 to 1.7 s on a Ryzen 7 7800X3D: about a second for the program to start, then an eighth of a
  second for every second of speech. A line that goes wrong is only a caption; three different lines in a row that
  can't be said, and the voice is off for the session (every line a caption) rather than tried again and again.
  Who has which voice: a man has a man's voice and a woman a woman's, and the people of one case have different ones
  (HUPeople::SuspectVoice). The 911 dispatcher is voice 0; the caller is one of the suspects, and has their voice.
  Who goes first: lines someone is waiting to hear, two at a time at most. Lines for later - a suspect's stalling
  lines, answers said ahead - one at a time, only while nobody is waiting, on processor time the game isn't using.
  Every line said is kept in memory, so the same words in the same voice play at once the second time
  ("HomicideUnit voice: already made, no wait"). Stalling lines are kept on disk too (Saved\VoiceCache): with ten
  voices, after a few sessions none has to be said again. Walking up to a suspect has their introduction and the
  answers ready for them spoken ahead ("HomicideUnit voice: made ahead ...").
  Test it in the game: UnrealEditor.exe HomicideUnit.uproject -game -nullrhi -nosound -unattended -HUNoBrain -HUVoiceTest
  (the log: "HUVoiceTest: ok ..." lines with what each line took, then "all checks passed").
  Where a line is heard from is the game's work (UHUVoiceSpace); the voice only ever makes clean speech:
  A suspect's line comes from their mouth: full across the table, a third as loud across a room (5 m), faint at 10 m,
  half as loud behind glass and a murmur behind a wall. The 911 call plays from the briefing room's lectern, and the
  game makes it a phone call on tape: only the 300-3400 Hz a phone line carries, then the recorder's waver and hiss.
  Check it on any level (no voice needed): UnrealEditor.exe HomicideUnit.uproject /Game/Maps/L_Scene -game -nullrhi -nosound
  -unattended -HUNoBrain -HUNoVoice -HUVoiceSpaceTest   (the log: "HUVoiceSpaceTest: ok ..." lines, then "all checks passed").
  Add -HUVoiceSpaceTape=C:\path\line.wav (24 kHz 16-bit mono) to write line_on_tape.wav beside it: that line as the 911 call plays it.

HEARING THE DETECTIVE (hold V near a suspect, talk, let go). SHIPPED: AI\stt-accurate, and from AI\voice\bin the two
listening programs. Uses sherpa-onnx (Apache-2.0), on the processor (no graphics card needed - "NVIDIA" below is who
made the model).
  AI\stt-accurate\  NVIDIA Parakeet TDT 0.6B v2, int8 (encoder/decoder/joiner .int8.onnx + tokens.txt, ~630 MB)
      from github.com/k2-fsa/sherpa-onnx releases "asr-models": sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8.tar.bz2
      License: CC-BY-4.0 - credit "Parakeet TDT 0.6B v2 by NVIDIA" in the game's credits.
      Loaded once when an interview opens (sherpa-onnx-offline-websocket-server.exe on 127.0.0.1:8096, ~2 s), then ~0.3 s a question.
  AI\stt\           Moonshine tiny English (*.onnx + tokens.txt). NOT shipped: the game loads it only on a PC where
      AI\stt-accurate is missing, which a packaged game never is. ~0.6 s a question.
  Tested on 80 accented-English clips (EdAcc, 20 accents) and 61 interview lines, clean / with chatter / with noise:
      words wrong on accents  - Moonshine tiny 38% / 61% / 60%,  Parakeet 0.6B v2 21% / 33% / 32%.
  Test the installed model: UnrealEditor.exe HomicideUnit.uproject -game -HUListenTest=C:\path\question.wav (16-bit .wav; logs what it heard).
  Test who a spoken line goes to: UnrealEditor.exe HomicideUnit.uproject /Game/Maps/L_Scene -game -nullrhi -nosound -unattended
      -HUNoBrain -HUNoVoice -HUTalkTest   (the log: "HUTalkTest: ok ..." lines - the last three are said out loud by the voice
      and made out by the ears - then "all checks passed").

NO LONGER USED BY THE GAME (16 Nov 2026). Not shipped. Still on a developer's PC until someone deletes them:
  AI\orpheus, AI\snac, AI\csm, AI\voices   the big voices (Orpheus 3B, Sesame CSM-1B) the game used to pick on an NVIDIA card with
                                           12 GB: 6 GB of files and 6 GB of the card's memory, 2 to 7 s a line, and a game
                                           that sounded different from one PC to the next.
  AI\voice\kokoro, AI\chatterbox*          earlier trials.
  Tools\VoiceServer.py, VoiceServer.bat    the server that ran the big voices. The game no longer starts it.
  AI\python                                the game no longer needs Python, and it isn't shipped. KEEP it on a developer's PC:
                                           the project's own tools run on it (Tools\InterviewBench.ps1, the prop and sound scripts).
