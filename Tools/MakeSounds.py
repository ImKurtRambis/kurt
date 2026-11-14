# Synthesizes the game's first sound set from scratch (no downloads, no licences): footsteps, doors, the cruiser,
# bagging evidence, cuffs, menu clicks, rain, street noise and a slow noir music bed.
# Run: AI\python\python.exe Tools\MakeSounds.py   -> writes Tools\Sounds\*.wav (16-bit mono 44.1 kHz)
# Then import them: UnrealEditor-Cmd HomicideUnit.uproject -run=pythonscript -script="Tools/ImportSounds.py"
import os, numpy as np
from scipy.signal import butter, sosfilt
from scipy.io import wavfile

SR = 44100
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Sounds")
os.makedirs(OUT, exist_ok=True)
rng = np.random.default_rng(1947)

def t(sec): return np.arange(int(sec * SR)) / SR
def noise(sec): return rng.standard_normal(int(sec * SR))
def bp(x, lo, hi, o=2): return sosfilt(butter(o, [lo, hi], "band", fs=SR, output="sos"), x)
def lp(x, f, o=2): return sosfilt(butter(o, f, "low", fs=SR, output="sos"), x)
def hp(x, f, o=2): return sosfilt(butter(o, f, "high", fs=SR, output="sos"), x)
def env(sec, a, d): x = t(sec); return np.minimum(x / max(a, 1e-4), 1) * np.exp(-np.maximum(x - a, 0) / d)
def pad(x, sec): y = np.zeros(int(sec * SR)); y[:len(x)] = x[:len(y)]; return y
def at(dst, src, sec): i = int(sec * SR); n = min(len(src), len(dst) - i); dst[i:i + n] += src[:n]
def save(name, x, peak=0.8):
    x = x / (np.max(np.abs(x)) + 1e-9) * peak
    f = int(0.004 * SR); x[:f] *= np.linspace(0, 1, f); x[-f:] *= np.linspace(1, 0, f)
    wavfile.write(os.path.join(OUT, name + ".wav"), SR, (x * 32767).astype(np.int16)); print(name, round(len(x) / SR, 2), "s")
def loop(x, fade=1.0):  # crossfade the tail into the head so the file loops without a click
    n = int(fade * SR); w = np.linspace(0, 1, n); y = x[n:].copy(); y[-n:] = y[-n:] * (1 - w) + x[:n] * w; return y

# footsteps: heel then toe on a hard floor, four variants so they don't machine-gun
for i in range(4):
    s = 0.32; x = np.zeros(int(s * SR))
    heel = bp(noise(0.06), 90 + 20 * i, 900, 2) * env(0.06, 0.001, 0.012) + np.sin(2 * np.pi * (70 + 8 * i) * t(0.06)) * env(0.06, 0.001, 0.02) * 0.6
    toe = bp(noise(0.05), 400, 3500) * env(0.05, 0.001, 0.008) * 0.35
    at(x, heel, 0.0); at(x, toe, 0.055 + 0.01 * i)
    save("SFX_Step%d" % (i + 1), x, 0.7)

# door: latch click, a creak, then it thuds shut
x = np.zeros(int(1.4 * SR))
at(x, bp(noise(0.03), 1500, 6000) * env(0.03, 0.0005, 0.004), 0.0)
ct = t(0.55); f = 220 + 140 * np.sin(2 * np.pi * 1.3 * ct) + 30 * rng.standard_normal(len(ct)).cumsum() / 400
cr = np.sign(np.sin(2 * np.pi * np.cumsum(f) / SR)) * (0.5 + 0.5 * np.sin(2 * np.pi * 23 * ct))
at(x, bp(cr, 300, 2500) * np.hanning(len(ct)) * 0.25, 0.08)
thud = lp(noise(0.4), 220, 4) * env(0.4, 0.002, 0.08) * 3 + np.sin(2 * np.pi * 55 * t(0.4)) * env(0.4, 0.002, 0.1)
at(x, thud, 0.75); at(x, bp(noise(0.03), 1200, 5000) * env(0.03, 0.0005, 0.005) * 0.5, 0.76)
save("SFX_Door", x)

# car door slam then the engine turning over and pulling away
x = np.zeros(int(4.0 * SR))
at(x, lp(noise(0.3), 300, 4) * env(0.3, 0.001, 0.05) * 3, 0.0); at(x, bp(noise(0.05), 2000, 7000) * env(0.05, 0.0005, 0.006), 0.0)
et = t(3.3); rpm = 12 + 10 * np.clip((et - 0.6) / 1.2, 0, 1) + 6 * np.clip((et - 2.0), 0, 1)
ph = 2 * np.pi * np.cumsum(rpm) / SR
eng = (np.sin(ph) + 0.6 * np.sin(2 * ph) + 0.3 * np.sign(np.sin(4 * ph))) * (0.6 + 0.4 * np.abs(np.sin(ph * 0.5)))
eng = lp(eng + 0.3 * lp(noise(3.3), 400), 600, 3)
crank = np.clip((0.6 - et) / 0.6, 0, 1) * (np.sin(2 * np.pi * 7 * et) > 0)
e = np.minimum(et / 0.1, 1) * np.where(et > 2.6, np.clip((3.3 - et) / 0.7, 0, 1), 1)
at(x, (eng * np.clip(et / 0.6, 0.2, 1) + crank * lp(noise(3.3), 900)) * e * 0.8, 0.5)
save("SFX_CarDepart", x)

# roll-up garage door: chain rattle over a metal rumble
st = t(3.0); x = lp(noise(3.0), 180, 3) * 1.5 + bp(noise(3.0), 800, 3000) * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * 14 * st))) * 0.4
x *= np.minimum(st / 0.2, 1) * np.minimum((3.0 - st) / 0.3, 1)
at(x, lp(noise(0.3), 200, 4) * env(0.3, 0.002, 0.07) * 4, 2.65)
save("SFX_RollDoor", x)

# crowbar: wood and metal crack
x = np.zeros(int(0.8 * SR))
at(x, bp(noise(0.25), 200, 4000) * env(0.25, 0.001, 0.03) * 2, 0)
for k in range(5): at(x, bp(noise(0.02), 1000, 6000) * env(0.02, 0.0005, 0.004), 0.03 + 0.025 * k + 0.01 * rng.random())
at(x, np.sin(2 * np.pi * 1840 * t(0.6)) * env(0.6, 0.001, 0.15) * 0.15, 0.0)
save("SFX_Crowbar", x)

# evidence bag: plastic crinkle, then the zip seal run shut
x = np.zeros(int(1.3 * SR))
for k in range(40):
    at(x, bp(noise(0.015), 2500, 9000) * env(0.015, 0.0005, 0.003) * rng.uniform(0.2, 1), rng.uniform(0, 0.55))
zt = t(0.45); zip_ = bp(noise(0.45), 1500, 7000) * (0.5 + 0.5 * np.sign(np.sin(2 * np.pi * (60 + 80 * zt / 0.45) * zt))) * np.hanning(len(zt))
at(x, zip_ * 0.8, 0.7)
save("SFX_Bag", x, 0.6)

# handcuffs: ratchet clicks closing, twice (one wrist each), with a chain jingle
x = np.zeros(int(1.4 * SR))
def click(a=1.0): return (pad(bp(noise(0.012), 2000, 9000) * env(0.012, 0.0003, 0.002), 0.03) + np.sin(2 * np.pi * 3100 * t(0.03)) * env(0.03, 0.0003, 0.006) * 0.4) * a
for w, start in ((0, 0.0), (1, 0.55)):
    for k in range(7): at(x, click(1 - 0.06 * k), start + 0.035 * k)
for k in range(10): at(x, np.sin(2 * np.pi * rng.uniform(2500, 5000) * t(0.08)) * env(0.08, 0.001, 0.02) * 0.2, rng.uniform(0.25, 1.2))
save("SFX_Cuffs", x, 0.7)

# menu: a short typewriter-ish key click, and a softer hover tick
x = pad(pad(bp(noise(0.02), 1500, 7000) * env(0.02, 0.0003, 0.004), 0.04) + np.sin(2 * np.pi * 900 * t(0.04)) * env(0.04, 0.0005, 0.01) * 0.5, 0.08)
save("SFX_Click", x, 0.5)

# rain loop: wide hiss plus scattered drops on hard surfaces, 20 s, loops cleanly
s = 21.0; x = lp(hp(noise(s), 300), 5000) * 0.35 + lp(noise(s), 400) * 0.25
for k in range(1500):
    at(x, bp(noise(0.02), 1500, 6000) * env(0.02, 0.0005, 0.004) * rng.uniform(0.05, 0.4), rng.uniform(0, s - 0.05))
save("AMB_Rain", loop(x), 0.5)

# street: distant traffic swells, a far-off siren once, hum of the city, 30 s loop
s = 31.0; st = t(s); x = lp(noise(s), 250, 3) * (0.6 + 0.4 * np.sin(2 * np.pi * st / 7.3) * np.sin(2 * np.pi * st / 3.1))
for c in range(5):
    m = rng.uniform(2, s - 6); w = np.exp(-((st - m) / 1.8) ** 2)
    x += lp(noise(s), 700, 3) * w * 0.8
sv = np.exp(-((st - 18) / 3.5) ** 2) * np.sin(2 * np.pi * np.cumsum(700 + 250 * np.sin(2 * np.pi * 0.25 * st)) / SR) * 0.05
x += lp(sv, 2000)
save("AMB_Street", loop(x), 0.45)

# precinct room tone: fluorescent hum with a little air
s = 21.0; st = t(s); x = (np.sin(2 * np.pi * 120 * st) * 0.3 + np.sin(2 * np.pi * 240 * st) * 0.1) * (0.9 + 0.1 * np.sin(2 * np.pi * 0.2 * st)) + lp(noise(s), 500) * 0.3
save("AMB_Precinct", loop(x), 0.3)

# music bed: slow minor progression, soft pad + muted bass walk + brushed hats, 60 BPM, 32 s loop
bpm = 60; beat = 60 / bpm; bars = 8; s = bars * 4 * beat + 2; x = np.zeros(int(s * SR))
def midi(n): return 440 * 2 ** ((n - 69) / 12)
prog = [[57, 60, 64, 67], [53, 57, 60, 64], [50, 53, 57, 60], [52, 56, 59, 62]] * 2   # Am7 Fmaj7 Dm7 E7
bass = [[45, 48, 52, 50], [41, 45, 48, 47], [38, 41, 45, 44], [40, 44, 47, 44]] * 2
for b in range(bars):
    t0 = b * 4 * beat; ct = t(4 * beat + 1.5)
    chord = sum(np.sin(2 * np.pi * midi(n) * ct) + 0.3 * np.sin(2 * np.pi * midi(n) * 2.003 * ct) for n in prog[b])
    at(x, lp(chord, 1800) * np.minimum(ct / 0.8, 1) * np.exp(-np.maximum(ct - 4 * beat, 0) / 0.5) * 0.12, t0)
    for k, n in enumerate(bass[b]):
        bt = t(beat * 1.2); at(x, (np.sin(2 * np.pi * midi(n) * bt) + 0.4 * np.sin(4 * np.pi * midi(n) * bt)) * env(beat * 1.2, 0.01, 0.35) * 0.5, t0 + k * beat)
    for k in range(8):
        at(x, hp(noise(0.12), 5000) * env(0.12, 0.002, 0.04 if k % 2 else 0.07) * (0.05 if k % 2 else 0.08), t0 + k * beat / 2)
    if b % 2 == 1:  # a lonely muted horn line every other bar
        for k, n in enumerate([prog[b][3] + 12, prog[b][2] + 12]):
            ht = t(beat * 1.6); vib = 1 + 0.004 * np.sin(2 * np.pi * 5 * ht)
            at(x, lp(np.sin(2 * np.pi * np.cumsum(midi(n) * vib) / SR) ** 3, 1500) * np.hanning(len(ht)) * 0.12, t0 + (1 + 2 * k) * beat)
save("MUS_Bed", loop(x, 2.0), 0.55)
print("done ->", OUT)
