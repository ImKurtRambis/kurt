# Homicide Unit - system requirements

Everything runs on the player's own PC: the game, the suspects' brain (a local language model) and the
voices (Orpheus and CSM). No internet and no fees, but the graphics card has to hold all of it at once.
There is no low-spec mode: every player gets the same suspects and voices.

## Minimum

| Part | Minimum |
|---|---|
| Graphics card | **NVIDIA RTX with 12 GB of video memory** (for example an RTX 3060 12 GB) |
| Memory | 16 GB |
| Processor | 6 cores (for example a Ryzen 5 5600 or Core i5-12400) |
| Storage | 20 GB free, SSD |
| System | Windows 10/11, 64-bit, a recent NVIDIA driver |

## Recommended (the PC it was measured on)

| Part | Recommended |
|---|---|
| Graphics card | NVIDIA RTX with 16 GB (measured: RTX 5070 Ti 16 GB) |
| Memory | 32 GB |
| Processor | 8 cores (measured: Ryzen 7 7800X3D) |
| Storage | 20 GB free, SSD |

## Why NVIDIA

The suspects' brain (llama.cpp, Vulkan build) runs on any modern card, but the voices run on PyTorch with
CUDA, and CUDA is NVIDIA only.

## What was measured (12 Nov 2026)

Measured at the crime scene, 1600x900 window, editor build (a packaged build will be a little lighter).
Video memory was measured as the whole card's use minus what Windows was already using.

| What's running | Video memory | System memory | Frame rate |
|---|---|---|---|
| The game alone | about 0.6 GB | 2.1 GB | about 400 fps |
| + the suspects' brain | about 3.9 GB | 5.2 GB | about 275 fps while it writes answers, 420 fps after |
| + the voices (everything) | **about 10.2 GB** | **8.3 GB** | about 225-290 fps while it writes, 400 fps after |

- Planning the case (the offender's brain): 300 cases in about 26 ms. It never adds to load times.
- Prewritten answers: all 48 (8 suspects x 6 questions) were ready about 27 seconds after the model loaded.
  A card half as fast would take about a minute, still well before anyone reaches the interview room.
- Frames over 33 ms (a visible stutter): none while answers were being written. There's one hitch of
  0.3-0.5 seconds when the model and voices first load at the start of a case.
- Storage: the AI files are 14.4 GB; the game's own content is small.

## Not measured yet

- Video memory while a voice is actually speaking (the test had nobody talking). Expect a little more than 10.2 GB.
- Any other graphics card. The 12 GB minimum is the measured 10.2 GB plus room for Windows and the voices
  speaking. It still needs testing on a 12 GB card.
- A packaged (shipping) build.

## Measure it again

Development builds have two switches and a console command:

- `-HUNoBrain` starts the game without the suspects' brain; `-HUNoVoice` starts it without the voices.
- `HU.Perf` in the console logs the frame times every 5 seconds, plus how much of that time the brain was writing.
