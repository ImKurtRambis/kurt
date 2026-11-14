# Homicide Unit - system requirements

Everything runs on the player's own PC: the game, the suspects' brain (a local language model), the voices and
speech recognition. No internet and no fees. The game looks at the PC and picks where each part runs:

| The PC has | Suspects' brain | Voices | Hearing the detective |
|---|---|---|---|
| An NVIDIA card with 12 GB or more | the card | Orpheus + CSM, on the card | the processor |
| Any other card: AMD, Intel, a smaller NVIDIA card | the card (through Vulkan) | Supertonic, on the processor | the processor |
| No card the model can use | the processor | Supertonic, on the processor | the processor |

Every player gets the same suspects: one model, one set of rules. What changes with the PC is how fast they
answer and which voices speak.

## Recommended (the PC it was measured on)

| Part | Recommended |
|---|---|
| Graphics card | NVIDIA RTX with 12 GB or more for the big voices (measured: RTX 5070 Ti 16 GB) |
| Memory | 32 GB |
| Processor | 8 cores (measured: Ryzen 7 7800X3D) |
| Storage | 20 GB free, SSD |

## Minimum (worked out, not yet tested on such a PC)

| Part | Minimum |
|---|---|
| Graphics card | Any maker's card with 6 GB of video memory: the brain takes about 3.3 GB beside the game, the small voice none |
| Memory | 16 GB |
| Processor | 6 cores (for example a Ryzen 5 5600 or Core i5-12400): it speaks the small voice and hears the detective |
| Storage | 20 GB free, SSD |
| System | Windows 10/11, 64-bit, a graphics driver with Vulkan (every current NVIDIA, AMD and Intel driver has it) |

On a card too small for the brain the game notices it writing slowly and moves it to the processor, where it
answers in about 5 seconds instead of a quarter of a second.

## What was measured (12 Nov 2026): memory and frame rate

Measured at the crime scene, 1600x900 window, editor build (a packaged build will be a little lighter).
Video memory was measured as the whole card's use minus what Windows was already using.

| What's running | Video memory | System memory | Frame rate |
|---|---|---|---|
| The game alone | about 0.6 GB | 2.1 GB | about 400 fps |
| + the suspects' brain | about 3.9 GB | 5.2 GB | about 275 fps while it writes answers, 420 fps after |
| + the big voices (everything) | **about 10.2 GB** | **8.3 GB** | about 225-290 fps while it writes, 400 fps after |

- Planning the case (the offender's brain): 300 cases in about 26 ms. It never adds to load times.
- Prewritten answers: all 48 (8 suspects x 6 questions) were ready about 27 seconds after the model loaded.
  A card half as fast would take about a minute, still well before anyone reaches the interview room.
- Frames over 33 ms (a visible stutter): none while answers were being written. There's one hitch of
  0.3-0.5 seconds when the model and voices first load at the start of a case.
- Storage: the AI files are about 15.5 GB (14 Nov, with the accurate speech model and the small voice); the game's own content is small.

## What was measured (14 Nov 2026): speed on every path

Same PC. Each path the game can pick was run by forcing it, in the game without a window. The AMD and Intel
paths are this same code: the brain through Vulkan is what the NVIDIA card itself uses, and the small voice
never touches the card.

| Path | Forced with | Result |
|---|---|---|
| Brain on the card | (the game's own choice) | ready 4.9 s after it's asked for; 151 answers, half under 0.24 s; writes 191 tokens a second |
| Brain on the processor | `-HUBrain=cpu` | ready in 2.7 s; 151 answers, half under 5.0 s, the slowest 13.9 s; writes 17 tokens a second |
| The card fails, the processor takes over | `-HUBrainBreakCard` | handed over by itself; answering 4.8 s after it was asked for |
| The card is too slow, the processor takes over | `-HUBrainMinSpeed=9999` | handed over by itself; answering 4.9 s after it was asked for |
| Big voices on the card | (the game's own choice) | 911 lines (Orpheus): 2.2 s a line. Suspects (CSM): 7 s a line, 1.7 s of work per second of speech |
| Small voice on the processor | `-HUVoices=cpu` | 1.4 s a line for 911 lines and suspects alike, 0.4 s of work per second of speech |
| Orpheus without CUDA (card through Vulkan, its decoder on the processor) | `HU_VOICES=orpheus` | 3.7 s a 911 line, 0.8 s of work per second of speech |
| Hearing the detective | (always the processor) | 43 s of speech read in 4.0 s on two threads |

- llama.cpp's CUDA build was tried for the brain: 175 tokens a second against Vulkan's 190, and 1.2 GB against
  100 MB. So one Vulkan build serves every card.
- The 5 seconds on the processor is mostly re-reading: the last six exchanges slide along every turn, and a
  processor reads about 130 tokens a second where the card reads 5000.

## Not measured yet

- **Any AMD or Intel card, and any PC without a 12 GB NVIDIA card.** Everything above ran on one RTX 5070 Ti.
- Video memory with the Lumen renderer (the 12 Nov numbers are older than it), and while a voice is speaking.
- A 12 GB NVIDIA card: the big voices are picked from 11.5 GB up, which is the measured 10.2 GB plus room for
  Windows. On less the small voice speaks.
- A processor slower than the Ryzen 7 7800X3D running the brain.
- A packaged (shipping) build on a PC with nothing installed.

## Measure it again

Development builds have these switches and a console command:

- `-HUNoBrain` starts the game without the suspects' brain; `-HUNoVoice` starts it without the voices.
- `-HUBrain=card` / `-HUBrain=cpu` and `-HUVoices=cuda` / `-HUVoices=cpu` force where the brain and the voices run
  (these two work in a shipping build too, as launch options).
- `-HUInterviewBench -HUBenchCases=1` asks the suspects 151 questions; the log has a line for each answer
  (`HomicideUnit brain: answered in 0.24 s (36 tokens at 191 a second, on the graphics card)`).
- `-HUVoiceTest` speaks a 911 call and four suspect lines twice and logs what each took.
- `HU.Perf` in the console logs the frame times every 5 seconds, plus how much of that time the brain was writing.
