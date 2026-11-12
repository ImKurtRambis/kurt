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

SPEAK QUESTIONS (optional, Settings > Speak questions): Moonshine tiny English (sherpa-onnx ASR model) in AI\stt\ (*.onnx + tokens.txt). Uses AI\voice\bin\sherpa-onnx-offline.exe.
