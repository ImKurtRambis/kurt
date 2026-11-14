Homicide Unit - the local AI: the suspects' brain, the voices, and the detective's ears
(free to play: everything runs on this PC, no internet, no keys)

WHAT RUNS WHERE. The game picks by itself, on any maker's graphics card:
  The brain    the graphics card - NVIDIA, AMD or Intel, through Vulkan. The processor when no card can run it.
  The voices   an NVIDIA card with 12 GB or more: Orpheus + CSM, on the card. Any other PC: Supertonic, on the processor.
  The ears     the processor, always.
  To force one (testing, or a launch option for a player whose card misbehaves):  -HUBrain=card|cpu   -HUVoices=cuda|cpu
  Measured on an RTX 5070 Ti + Ryzen 7 7800X3D, the only PC it has run on (14 Nov 2026):
      brain on the card 0.24 s an answer, on the processor 5 s; voices on the card 2 s a 911 line and 7 s a suspect line,
      the small voice on the processor 1.4 s a line. No AMD or Intel card has run it yet: those paths were forced on the NVIDIA card.

THE BRAIN. Put these here (they're big, so they're not in git):
  AI\llama\llama-server.exe   + its DLLs   llama.cpp's VULKAN build for Windows (MIT), from github.com/ggml-org/llama.cpp releases:
                                           llama-b<number>-bin-win-vulkan-x64.zip (build 12170 is the one installed).
                                           One build for every card. Its CUDA build was measured too: no faster on the RTX 5070 Ti
                                           (175 against 190 tokens a second), 1.2 GB instead of 100 MB, and NVIDIA only.
  AI\models\suspect.gguf                   (a small instruct model, Apache-2.0)

Without them the game still works: suspects answer typed questions with written lines.
In an interview press T, type, Enter. The model only voices the suspect - the game decides every fact.
The game's log says where the model runs and how fast ("HomicideUnit brain: ready on the graphics card ... 193 tokens a second",
then "answered in 0.24 s ..." for every answer).

Free offline VOICES (no keys, no tokens). The game starts Tools\VoiceServer.bat as it launches, and the server picks the voices.
 On an NVIDIA card with 12 GB or more - the big voices, on the card:
  Orpheus 3B (Apache-2.0)   - the 911 call and tape audio
      AI\orpheus\orpheus-3b-q4_k_m.gguf   (isaiahbjork/orpheus-3b-0.1-ft-Q4_K_M-GGUF), run by AI\llama\llama-server.exe on port 8090
      AI\snac\config.json + pytorch_model.bin   (hubertsiuzdak/snac_24khz, MIT) turns its codes into sound
  Sesame CSM-1B (Apache-2.0) - suspects and everyone else
      AI\csm\model\   (unsloth/csm-1b: model.safetensors, tokenizer + config files)
  AI\python\  - portable Python 3.10 with torch (CUDA 12.8), transformers 4.57, snac, triton-windows 3.5
      plus Python's include\ and libs\ (python_3.10.11_include_libs.zip from woct0rdho/triton-windows) so Triton can compile.
  With Visual Studio's C++ tools installed, CSM is compiled on first use (15 to 45 s once) and runs ~4x faster.
  Each voice number's first CSM line is kept in AI\voices\, so a suspect keeps the same voice; premade lines are cached there too.
 On every other PC (AMD, Intel, a smaller NVIDIA card, no card) - the small voice, on the processor:
  Supertonic 3 (code MIT, model BigScience OpenRAIL-M) - ten voices (0-4 women, 5-9 men), for the 911 call and everyone else
      AI\voice\supertonic\   duration_predictor, text_encoder, vector_estimator, vocoder (.int8.onnx), tts.json, unicode_indexer.bin, voice.bin
          from github.com/k2-fsa/sherpa-onnx releases "tts-models": sherpa-onnx-supertonic-3-tts-int8-2026-05-11.tar.bz2 (139 MB)
          Licence: selling the game is allowed, but the model's use rules must travel with it. Keep both LICENSE files in
          that folder, and name them in the game's licence text and credits ("Supertonic 3 by Supertone Inc.").
      Run by AI\voice\bin\sherpa-onnx-offline-tts.exe, one process a line. It needs no graphics card and no PyTorch
      (Python only runs the small server). It also stands in on an NVIDIA PC when a big voice won't load or fails a line.
 The server: http://127.0.0.1:8005  (/health says which voices it picked, POST /tts). Log it by running Tools\VoiceServer.bat by hand;
  set HU_VOICES=cuda or cpu first to force the voices (HU_VOICES=orpheus: the small voice plus Orpheus for 911 lines on any card).
 Test them in the game: UnrealEditor.exe HomicideUnit.uproject -game -nullrhi -unattended -HUNoBrain -HUVoiceTest [-HUVoices=cpu]
  (speaks a 911 call and four suspect lines twice; the log says what each line took).

HEARING THE DETECTIVE (hold V in an interview, talk, let go). Uses sherpa-onnx (Apache-2.0) from AI\voice\bin\, on the processor
(no graphics card needed - "NVIDIA" below is who made the model). The best model present is used:
  AI\stt-accurate\  NVIDIA Parakeet TDT 0.6B v2, int8 (encoder/decoder/joiner .int8.onnx + tokens.txt, ~630 MB)
      from github.com/k2-fsa/sherpa-onnx releases "asr-models": sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8.tar.bz2
      License: CC-BY-4.0 - credit "Parakeet TDT 0.6B v2 by NVIDIA" in the game's credits.
      Loaded once when an interview opens (sherpa-onnx-offline-websocket-server.exe on 127.0.0.1:8096, ~2 s), then ~0.3 s a question.
  AI\stt\           Moonshine tiny English (*.onnx + tokens.txt) - the small fallback, ~0.6 s a question.
  Tested on 80 accented-English clips (EdAcc, 20 accents) and 61 interview lines, clean / with chatter / with noise:
      words wrong on accents  - Moonshine tiny 38% / 61% / 60%,  Parakeet 0.6B v2 21% / 33% / 32%.
  Test the installed model: UnrealEditor.exe HomicideUnit.uproject -game -HUListenTest=C:\path\question.wav (16-bit .wav; logs what it heard).
