Homicide Unit - the suspects' local AI brain (free to play: runs on this PC, no internet, no keys)

Put these here (they're big, so they're not in git):
  AI\llama\llama-server.exe   + its DLLs   (llama.cpp release, MIT license)
  AI\models\suspect.gguf                   (a small instruct model, Apache-2.0)

Without them the game still works: suspects answer typed questions with written lines.
In an interview press T, type, Enter. The model only voices the suspect - the game decides every fact.

Free offline VOICES (no keys, no tokens; needs a strong GPU). The game starts Tools\VoiceServer.bat as it launches:
  Orpheus 3B (Apache-2.0)   - the 911 call and tape audio
      AI\orpheus\orpheus-3b-q4_k_m.gguf   (isaiahbjork/orpheus-3b-0.1-ft-Q4_K_M-GGUF), run by AI\llama\llama-server.exe on port 8090
      AI\snac\config.json + pytorch_model.bin   (hubertsiuzdak/snac_24khz, MIT) turns its codes into sound
  Sesame CSM-1B (Apache-2.0) - suspects and everyone else
      AI\csm\model\   (unsloth/csm-1b: model.safetensors, tokenizer + config files)
  AI\python\  - portable Python 3.10 with torch (CUDA 12.8), transformers 4.57, snac, triton-windows 3.5
      plus Python's include\ and libs\ (python_3.10.11_include_libs.zip from woct0rdho/triton-windows) so Triton can compile.
  With Visual Studio's C++ tools installed, CSM is compiled on first use (about 20 s once) and runs ~4x faster.
  Each voice number's first CSM line is kept in AIoices\, so a suspect keeps the same voice; premade lines are cached there too.
  The server: http://127.0.0.1:8005  (/health, POST /tts). Log it by running Tools\VoiceServer.bat by hand.

HEARING THE DETECTIVE (hold V in an interview, talk, let go). Uses sherpa-onnx (Apache-2.0) from AI\voice\bin\. The best model present is used:
  AI\stt-accurate\  NVIDIA Parakeet TDT 0.6B v2, int8 (encoder/decoder/joiner .int8.onnx + tokens.txt, ~630 MB)
      from github.com/k2-fsa/sherpa-onnx releases "asr-models": sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-int8.tar.bz2
      License: CC-BY-4.0 - credit "Parakeet TDT 0.6B v2 by NVIDIA" in the game's credits.
      Loaded once when an interview opens (sherpa-onnx-offline-websocket-server.exe on 127.0.0.1:8096, ~2 s), then ~0.3 s a question.
  AI\stt\           Moonshine tiny English (*.onnx + tokens.txt) - the small fallback, ~0.6 s a question.
  Tested on 80 accented-English clips (EdAcc, 20 accents) and 61 interview lines, clean / with chatter / with noise:
      words wrong on accents  - Moonshine tiny 38% / 61% / 60%,  Parakeet 0.6B v2 21% / 33% / 32%.
  Test the installed model: UnrealEditor.exe HomicideUnit.uproject -game -HUListenTest=C:\path\question.wav (16-bit .wav; logs what it heard).
