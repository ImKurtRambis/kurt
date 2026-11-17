# Homicide Unit - system requirements

Everything runs on the player's own PC: the game, the suspects' brain (a local language model), their voice and
speech recognition. No internet and no fees.

**One of each, the same on every PC.** One brain, one voice, one pair of ears. Nothing is picked by how big the
graphics card is, so every player's suspects think and sound the same, and the download carries nothing a PC won't
use. It is sized for an 8 GB graphics card: an RTX 2070 Super.

| Part | Runs on | Graphics memory | Download |
|---|---|---|---|
| The game | the graphics card | about 1.5-2.1 GB at 1080p | |
| Suspects' brain: Qwen3 4B Instruct (llama.cpp, Vulkan) | the graphics card - NVIDIA, AMD or Intel; the processor if no card can run it | about 3.1 GB | 2.5 GB |
| Their voice: Supertonic 3 (sherpa-onnx) | the processor, one line at a time | none | 0.17 GB |
| Hearing the detective: Parakeet TDT 0.6B v2 (sherpa-onnx) | the processor | none | 0.63 GB |

The AI is 3.2 GB of the download (`Tools\Package.ps1` copies only these; a developer's `AI\` folder is about 15 GB).

## The PC it's sized for

| Part | |
|---|---|
| Graphics card | NVIDIA RTX 2070 Super 8 GB, or an AMD or Intel card with 8 GB |
| Memory | 16 GB |
| Processor | 6 cores (for example a Ryzen 5 3600 or Core i5-10400): it speaks the voice and hears the detective |
| Storage | 12 GB free, SSD |
| System | Windows 10/11, 64-bit, a graphics driver with Vulkan (every current NVIDIA, AMD and Intel driver has it) |

**No RTX 2070 Super has run the game yet.** Everything below was measured on an RTX 5070 Ti; what a 2070 Super
should do is worked out from it under **On an RTX 2070 Super (worked out)**.

A card smaller than 6 GB, or one the brain can't run on: the game notices and moves the brain to the processor,
where it answers in about 5 seconds instead of half a second. With no AI at all, suspects answer with written lines.

## What was measured (16 Nov 2026)

RTX 5070 Ti 16 GB, Ryzen 7 7800X3D, 32 GB. The crime scene, drawn off screen, editor build (a packaged build is a
little lighter), the scripted talking test running (`-HUTalkTest`: questions asked, answered and spoken).
Graphics memory is the whole card's use minus what Windows was already using. On a 16 GB card the engine may keep
more than it would on an 8 GB one, so these are on the high side.

| What's running | Graphics memory, at the most | once settled |
|---|---|---|
| The game alone, 1920x1080 | 2.1 GB | 1.5 GB |
| + the brain | 5.2 GB | 4.7 GB |
| + the brain and the voice, 1920x1080 | 5.3 GB | 4.9 GB |
| + the brain and the voice, 2560x1440 | 5.3 GB | 4.8 GB |

| | |
|---|---|
| The brain beside the game (60 fps cap) | 102-124 tokens a second: an answer in about half a second (0.47-0.72 s) |
| The brain beside the game (no frame cap, the card 94% busy) | about 90 tokens a second: most answers 0.6 s, the slowest 3 s |
| The voice | a line in 1.2-1.7 s: about a second to start the program, then an eighth of a second for every second of speech. Said again: no wait |
| Answers written ahead | one every 1.5 s or so, resting between them (one a second back to back): all 48 (8 suspects x 6 questions every detective asks) in a little over a minute. Until theirs is ready, a suspect answers those six with written lines |

**Frame rate while the brain writes** (1920x1080, no frame cap; `HU.Perf`, 5-second windows):

| | frames a second |
|---|---|
| No brain (`-HUNoBrain`) | 100-135 |
| The brain writing answers ahead back to back (before 17 Nov) | 102 with the brain writing 46% of the time, 52-65 at 65-82% |
| The brain resting between answers written ahead, as long as each took (`UHUBrainSubsystem::PreRest`) | 111 with the brain writing 33% of the time, 122 at 27%, 89 at 32%: about what the game draws with no brain |

A frame drawn while the brain writes waits its turn on the card, even with power to spare: at a 60 fps cap the
card was 36% busy and still fell to 36-46 fps while the brain wrote back to back. Hence the rest between answers
written ahead (live answers never wait).

The hitches of about a second in these runs come from the test itself (it moves the player 30 m in one frame):
they are there with no brain as well.

## On an RTX 2070 Super (worked out, not measured)

- **Graphics memory:** the game, brain and voice took 4.9-5.3 GB above, so an 8 GB card keeps about 2.5 GB for
  Windows and the rest.
- **The brain** writes at a speed set by the card's memory: a 2070 Super's is half as fast (448 GB/s against
  896), so about 50 tokens a second beside the game - an answer in about a second. Most answers cost no wait at all:
  written by hand, or written ahead (see `AGENTS.md`).
- **The voice and the ears** run on the processor, so the card doesn't change them; a slower processor than the
  7800X3D makes a line take longer (the program's start is most of it).
- **Frame rate:** a 2070 Super draws about 40% as fast as the 5070 Ti, so roughly 45-55 fps at 1080p in this
  scene with no brain. Answers written ahead take about twice as long - all 48 in two to three minutes - with the
  brain writing at most half of that time, so the picture keeps most of its frames.

## The brain's paths (14 Nov 2026)

Same PC. Each path the game can pick was run by forcing it, in the game without a window. The AMD and Intel paths
are this same code: the brain through Vulkan is what the NVIDIA card itself uses.

| Path | Forced with | Result |
|---|---|---|
| Brain on the card | (the game's own choice) | ready 4.9 s after it's asked for; 151 answers, half under 0.24 s; writes 191 tokens a second |
| Brain on the processor | `-HUBrain=cpu` | ready in 2.7 s; 151 answers, half under 5.0 s, the slowest 13.9 s; writes 17 tokens a second |
| The card fails, the processor takes over | `-HUBrainBreakCard` | handed over by itself; answering 4.8 s after it was asked for |
| The card is too slow, the processor takes over | `-HUBrainMinSpeed=9999` | handed over by itself; answering 4.9 s after it was asked for |
| Hearing the detective | (always the processor) | 43 s of speech read in 4.0 s on two threads |

- llama.cpp's CUDA build was tried for the brain: 175 tokens a second against Vulkan's 190, and 1.2 GB against
  100 MB. So one Vulkan build serves every card.
- The 5 seconds on the processor is mostly re-reading: the last six exchanges slide along every turn, and a
  processor reads about 130 tokens a second where the card reads 5000.
- A Development package of 14 Nov ran the same brain paths from its own `HomicideUnit.exe` with the same results,
  and killing it by force left no brain or voice program running.

## Not measured yet

- **An RTX 2070 Super, or any 8 GB card. Any AMD or Intel card.** Everything above ran on one RTX 5070 Ti.
- A processor slower than the Ryzen 7 7800X3D running the voice and the ears.
- A package with only the one brain, voice and ears (`Tools\Package.ps1` copying was tested into a scratch folder:
  3.2 GB), and any package on a PC with nothing installed.

## Measure it again

Development builds have these switches and a console command:

- `-HUNoBrain` starts the game without the suspects' brain; `-HUNoVoice` starts it without the voice.
- `-HUBrain=card` / `-HUBrain=cpu` force where the brain runs (this works in a shipping build too, as a launch option).
- `-HUInterviewBench -HUBenchCases=1` asks the suspects 151 questions; the log has a line for each answer
  (`HomicideUnit brain: answered in 0.24 s (36 tokens at 191 a second, on the graphics card)`).
- `-HUVoiceTest` says six lines, then checks lines kept, made ahead and kept on disk, and logs what each took.
- `HU.Perf` in the console logs the frame times every 5 seconds, plus how much of that time the brain was writing.
