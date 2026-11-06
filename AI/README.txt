Homicide Unit - the suspects' local AI brain (free to play: runs on this PC, no internet, no keys)

Put these here (they're big, so they're not in git):
  AI\llama\llama-server.exe   + its DLLs   (llama.cpp release, MIT license)
  AI\models\suspect.gguf                   (a small instruct model, Apache-2.0)

Without them the game still works: suspects answer typed questions with written lines.
In an interview press T, type, Enter. The model only voices the suspect - the game decides every fact.

Free offline VOICE (Kokoro via sherpa-onnx, both Apache-2.0):
  AI\voice\bin\sherpa-onnx-offline-tts.exe (+ DLLs)
  AI\voice\kokoro\model.onnx, voices.bin, tokens.txt, espeak-ng-data\
When present it's used first (no key needed); suspects and the 911 caller speak with it.
