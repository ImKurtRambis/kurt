# Homicide Unit - the local voices. Free, offline, no keys or tokens. Started by the game (UHUVoiceSubsystem).
#
# WHICH VOICES RUN is decided here, once, from the graphics card (see pick_voices):
#   "cuda" - an NVIDIA card with 12 GB or more: the big voices, on the card.
#       Orpheus 3B (Apache-2.0) - the 911 caller and tape audio. Run by llama-server (AI\llama) on port 8090,
#                                 its audio codes turned into sound by SNAC (MIT, AI\snac).
#       Sesame CSM-1B (Apache-2.0) - suspects and everyone else. Run by PyTorch + CUDA (AI\csm\model).
#   "cpu"  - every other PC (AMD, Intel, a smaller NVIDIA card, no card): the small voice, on the processor.
#       Supertonic 3 (OpenRAIL-M) - ten voices from AI\voice\supertonic, run by sherpa-onnx (AI\voice\bin).
#                                   No graphics card and no PyTorch: about 1.5 s for a 3-second line.
#     It also stands in on an NVIDIA PC for a big voice that fails to load, or to speak a line.
#   HU_VOICES=cuda|cpu forces one (the game passes -HUVoices=...). HU_VOICES=orpheus is "cpu" plus Orpheus for phone
#   lines on whatever card llama-server finds, its codes decoded on the processor: for trying AMD and Intel cards.
# The game talks to this on http://127.0.0.1:8005 either way:
#   GET  /health -> {"orpheus": bool, "csm": bool, "voices": "cuda"|"cpu", "names": {...}, "loading": bool}
#                   "orpheus": phone lines can be spoken; "csm": everyone else can - whichever voice does it
#   POST /tts {"engine": "orpheus"|"csm", "text": "...", "voice": 3, "background": false} -> 24 kHz 16-bit mono .wav
# Run with Tools\VoiceServer.bat (it loads Visual Studio's compiler, which makes CSM about 4x faster).
import ctypes, hashlib, io, itertools, json, os, queue, re, subprocess, sys, tempfile, threading, time, urllib.request, wave, atexit
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "AI"))
# the ports can be moved (HU_VOICE_PORT, HU_ORPHEUS_PORT) to test this beside a running game
PORT, LLAMA_PORT, RATE = int(os.environ.get("HU_VOICE_PORT", 8005)), int(os.environ.get("HU_ORPHEUS_PORT", 8090)), 24000
MIN_CUDA_GB = 11.5  # the big voices + the brain + the game measured 10.2 GB: a "12 GB" card, not an 11 GB one
voices = "cpu"                             # "cuda" or "cpu": set by pick_voices() at start
loaded = {"orpheus": False, "csm": False}  # the big voice itself is loaded
failed = {"orpheus": False, "csm": False}  # it won't load on this PC: the small voice speaks its lines
small = False                              # Supertonic works here (it spoke a test line)
loading = True                             # still deciding and loading: the game keeps asking /health
jobs = queue.Queue()                       # lines waiting for the one thread that speaks (see voice_thread)
llama = None
snac_model, snac_dev = None, "cuda"
csm_model = csm_proc = None
csm_seeds = {}  # voice number -> (text, audio): the first line in that voice, fed back so the voice stays the same

import shutil
# Visual Studio's C++ compiler: on the developer's PC (VoiceServer.bat loads it), almost never on a player's
HAS_CL = shutil.which("cl") is not None
HAS_TRITON = csm_compiled = False  # set by setup_compile(); csm_compiled is switched off for good if compiling ever fails

def log(*a):
    print("[voice]", *a, flush=True)

def setup_compile():
    """PyTorch's compiler settings for CSM. Only the big voices use PyTorch: the small one never even imports it."""
    global HAS_TRITON, csm_compiled
    try:
        import triton  # noqa: F401  (lets torch compile CSM: much faster)
        import torch._inductor.config as _ind
        import torch._inductor.cudagraph_trees  # noqa: F401  (its CUDA graphs belong to the thread that imports it: the voice thread)
        _ind.triton.descriptive_names = False  # short kernel file names: Windows' 260-character path limit
        _ind.use_static_cuda_launcher = False  # its launcher overflows a Windows 32-bit long
        if not HAS_CL:
            # Without cl, two torch steps that build C++ on the CPU throw ("Compiler: cl is not found") and take every
            # CSM line down with them: the probe for CPU vector instructions, and the shape guards (which also come in
            # pure Python). CSM itself runs on the GPU, where Triton compiles with its own bundled tcc.
            import torch._dynamo.config as _dyn
            _ind.cpp.vec_isa_ok = False
            _dyn.enable_cpp_symbolic_shape_guards = False
        HAS_TRITON = True
    except Exception:
        HAS_TRITON = False
    csm_compiled = HAS_TRITON

# ---------------- which voices this PC gets ----------------
def nvidia_gb():
    """Video memory of the biggest NVIDIA card in GB, or 0 without one. Asks NVIDIA's driver itself (no PyTorch)."""
    try:
        cu = ctypes.WinDLL("nvcuda.dll")  # comes with the NVIDIA driver: not there on AMD and Intel PCs
        n = ctypes.c_int(0)
        if cu.cuInit(0) != 0 or cu.cuDeviceGetCount(ctypes.byref(n)) != 0: return 0.0
        best = 0
        for i in range(n.value):
            dev, size = ctypes.c_int(0), ctypes.c_size_t(0)
            if cu.cuDeviceGet(ctypes.byref(dev), i) == 0 and cu.cuDeviceTotalMem_v2(ctypes.byref(size), dev) == 0:
                best = max(best, size.value)
        return best / 2 ** 30
    except Exception:
        return 0.0

def pick_voices():
    have_big = os.path.exists(os.path.join(ROOT, "orpheus", "orpheus-3b-q4_k_m.gguf")) or os.path.exists(os.path.join(ROOT, "csm", "model", "model.safetensors"))
    want = os.environ.get("HU_VOICES", "").strip().lower()
    if want in ("cuda", "cpu", "orpheus"):
        log(f"voices forced to {want} (HU_VOICES)")
        return "cuda" if want == "cuda" else "cpu"
    gb = nvidia_gb()
    if gb >= MIN_CUDA_GB and have_big:
        log(f"NVIDIA card with {gb:.1f} GB: the big voices (Orpheus + CSM) on the card")
        return "cuda"
    why = "the big voices aren't installed" if not have_big else f"the NVIDIA card has only {gb:.1f} GB" if gb > 0 else "no NVIDIA card"
    log(f"{why}: the small voice (Supertonic) on the processor")
    return "cpu"

# ---------------- Orpheus ----------------
ORPHEUS_VOICES = ["tara", "leo", "leah", "dan", "jess", "zac", "mia", "zoe"]

def llama_post(path, body, timeout=120):
    req = urllib.request.Request(f"http://127.0.0.1:{LLAMA_PORT}{path}", json.dumps(body).encode(), {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())

def load_orpheus():
    global llama, snac_model, snac_dev
    try:
        import torch
        from snac import SNAC
        snac_dev = "cuda" if torch.cuda.is_available() else "cpu"  # SNAC is small: a processor decodes it faster than it plays
        exe = os.path.join(ROOT, "llama", "llama-server.exe")
        llama = subprocess.Popen([exe, "-m", os.path.join(ROOT, "orpheus", "orpheus-3b-q4_k_m.gguf"), "--port", str(LLAMA_PORT),
                                  "--host", "127.0.0.1", "-ngl", "99", "-c", "4096", "--log-disable"],
                                 cwd=os.path.dirname(exe), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        snac_model = SNAC.from_pretrained(os.path.join(ROOT, "snac")).eval().to(snac_dev)
        for _ in range(240):
            if llama.poll() is not None: raise RuntimeError(f"llama-server stopped (exit code {llama.returncode})")
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{LLAMA_PORT}/health", timeout=2) as r:
                    if r.status == 200: break
            except Exception:
                time.sleep(1)
        else:
            raise RuntimeError("llama-server never answered")
        loaded["orpheus"] = True
        log(f"Orpheus ready (its sound decoder on {snac_dev})")
    except Exception as e:
        failed["orpheus"] = True
        log("Orpheus failed:", e)

def orpheus(text, voice):
    import torch
    name = ORPHEUS_VOICES[abs(voice) % len(ORPHEUS_VOICES)]
    toks = llama_post("/tokenize", {"content": f"{name}: {text}", "add_special": False})["tokens"]
    # start of human, BOS, the line, end of text, end of human, start of AI, start of speech
    prompt = [128259, 128000] + toks + [128009, 128260, 128261, 128257]
    out = llama_post("/completion", {"prompt": prompt, "n_predict": min(3500, 400 + len(text) * 10), "temperature": 0.6,
                                     "top_p": 0.9, "repeat_penalty": 1.1, "return_tokens": True, "cache_prompt": False})
    codes = []
    for t in out.get("tokens", []):
        if t == 128258 or t == 128009: break  # end of speech
        if t < 128266: continue
        codes.append(t - 128266 - (len(codes) % 7) * 4096)
    n = len(codes) // 7
    if n < 2: raise RuntimeError("Orpheus gave no audio")
    c = codes[: n * 7]
    l1, l2, l3 = [], [], []
    for i in range(n):
        f = c[i * 7: i * 7 + 7]
        l1 += [f[0]]; l2 += [f[1], f[4]]; l3 += [f[2], f[3], f[5], f[6]]
    clamp = lambda xs: torch.tensor([[min(max(x, 0), 4095) for x in xs]], device=snac_dev)
    with torch.inference_mode():
        audio = snac_model.decode([clamp(l1), clamp(l2), clamp(l3)])
    return audio.squeeze().float().cpu().numpy()

# ---------------- Sesame CSM ----------------
SEED_LINES = ["Yeah, I mean... I don't really know what you want me to say here.",
              "Look, I've been sitting here for an hour. Can we just get this over with?"]

def load_csm():
    global csm_model, csm_proc
    try:
        import torch
        from transformers import CsmForConditionalGeneration, AutoProcessor
        path = os.path.join(ROOT, "csm", "model")
        csm_proc = AutoProcessor.from_pretrained(path)
        csm_model = CsmForConditionalGeneration.from_pretrained(path, device_map="cuda", dtype=torch.bfloat16).eval()
        # fixed-size caches: far less Python work per audio frame (the depth decoder runs 32 times a frame)
        if HAS_TRITON:
            csm_model.generation_config.cache_implementation = "static"
            csm_model.depth_decoder.generation_config.cache_implementation = "static"
        loaded["csm"] = True
        log("CSM ready" + (" (compiled on first use)" if HAS_TRITON else " (not compiled: slower)"))
    except Exception as e:
        failed["csm"] = True
        log("CSM failed:", e)

def csm_generate(conversation, text_len):
    global csm_compiled
    import torch
    inputs = csm_proc.apply_chat_template(conversation, tokenize=True, return_dict=True).to("cuda", dtype=torch.bfloat16)
    def run():
        with torch.inference_mode():
            return csm_model.generate(**inputs, output_audio=True, max_new_tokens=int(min(400, 40 + text_len * 1.4)))
    try:
        audio = run()
    except Exception as e:
        if not csm_compiled: raise
        # the compiled path failed on this PC (no compiler, an odd GPU...): run CSM uncompiled from now on - slower, but it talks
        log("compiling CSM failed - running it uncompiled from now on:", str(e).splitlines()[-1][:200] if str(e) else type(e).__name__)
        csm_compiled = False
        csm_model.generation_config.disable_compile = True
        csm_model.depth_decoder.generation_config.disable_compile = True
        torch._dynamo.reset()
        audio = run()
    return audio[0].float().cpu().numpy()

def csm(text, voice):
    import torch, numpy as np
    text = re.sub(r"<[^>]+>", "", text).strip()  # Orpheus-style <sigh> tags mean nothing to CSM
    if voice not in csm_seeds:
        path = os.path.join(ROOT, "voices", f"csm_{voice}.npy")
        seed_text = SEED_LINES[abs(voice) % len(SEED_LINES)]
        if os.path.exists(path):
            seed = np.load(path)
        else:  # first time this voice speaks: make it once with a fixed seed, and keep it
            torch.manual_seed(1000 + abs(voice))
            seed = csm_generate([{"role": "0", "content": [{"type": "text", "text": seed_text}]}], len(seed_text))
            os.makedirs(os.path.dirname(path), exist_ok=True)
            np.save(path, seed)
        csm_seeds[voice] = (seed_text, seed)
    seed_text, seed = csm_seeds[voice]
    conv = [{"role": "0", "content": [{"type": "text", "text": seed_text}, {"type": "audio", "path": seed}]},
            {"role": "0", "content": [{"type": "text", "text": text}]}]
    return csm_generate(conv, len(text))

# ---------------- Supertonic: the small voice, on the processor ----------------
ST_DIR = os.path.join(ROOT, "voice", "supertonic")
ST_EXE = os.path.join(ROOT, "voice", "bin", "sherpa-onnx-offline-tts.exe")
ST_VOICES = 10  # 0-4 are women, 5-9 men
st_count = itertools.count()
# a Windows command line carries plain ASCII safely, so curly quotes, long dashes and the like are swapped out
ST_PLAIN = str.maketrans({"\u2018": "'", "\u2019": "'", "\u201c": "", "\u201d": "", "\u2013": "-", "\u2014": ", ", "\u2026": "..."})

def resample(audio, src, dst):
    import numpy as np
    if src == dst: return audio
    if dst < src:  # keep only what the lower rate can carry, or the rest folds back as hiss
        cut = 0.45 * dst / src
        n = np.arange(-32, 33)
        h = 2 * cut * np.sinc(2 * cut * n) * np.hamming(65)
        audio = np.convolve(audio, h / h.sum(), "same")
    t = np.arange(int(len(audio) * dst / src)) * (src / dst)
    return np.interp(t, np.arange(len(audio)), audio).astype(np.float32)

def supertonic(text, voice):
    """One sherpa-onnx process per line: the model loads in about half a second, and a crash can't take this server with it."""
    import numpy as np
    text = re.sub(r"<[^>]+>", "", text).translate(ST_PLAIN).encode("ascii", "ignore").decode().strip().lstrip("-").strip()
    if not text: raise RuntimeError("nothing to say")
    out = os.path.join(tempfile.gettempdir(), f"hu_voice_{os.getpid()}_{next(st_count)}.wav")
    m = lambda name: os.path.join(ST_DIR, name)
    cmd = [ST_EXE, "--supertonic-duration-predictor=" + m("duration_predictor.int8.onnx"), "--supertonic-text-encoder=" + m("text_encoder.int8.onnx"),
           "--supertonic-vector-estimator=" + m("vector_estimator.int8.onnx"), "--supertonic-vocoder=" + m("vocoder.int8.onnx"),
           "--supertonic-tts-json=" + m("tts.json"), "--supertonic-unicode-indexer=" + m("unicode_indexer.bin"), "--supertonic-voice-style=" + m("voice.bin"),
           "--lang=en", "--num-steps=8", "--num-threads=2", f"--sid={abs(voice) % ST_VOICES}", "--output-filename=" + out, text]
    try:
        r = subprocess.run(cmd, cwd=os.path.dirname(ST_EXE), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120,
                           creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if r.returncode != 0 or not os.path.exists(out):
            raise RuntimeError(f"sherpa-onnx stopped with code {r.returncode}: {r.stdout.decode('utf-8', 'ignore')[-300:]}")
        with wave.open(out, "rb") as w:
            rate, channels = w.getframerate(), w.getnchannels()
            pcm = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")[::channels]
    finally:
        if os.path.exists(out): os.remove(out)
    return resample(pcm.astype(np.float32) / 32768, rate, RATE)

def load_supertonic():
    """Is the small voice installed, and does it run on this PC? It says a test line to find out."""
    global small
    if not (os.path.exists(ST_EXE) and os.path.exists(os.path.join(ST_DIR, "tts.json"))):
        log("the small voice (AI\\voice\\supertonic) isn't installed")
        return
    try:
        t0 = time.time()
        small = len(supertonic("Testing, one, two.", 0)) > RATE // 4
        log(f"Supertonic ready (a test line took {time.time() - t0:.1f} s)" if small else "Supertonic made no sound")
    except Exception as e:
        log("Supertonic failed:", e)

def can_speak(engine):
    """Can a line for this engine be spoken right now: by the big voice, or by the small one in its place?"""
    return loaded.get(engine, False) or (small and (voices == "cpu" or failed.get(engine, False)))

def speak(engine, text, voice):
    if loaded.get(engine):
        try:
            return (orpheus if engine == "orpheus" else csm)(text, voice), engine
        except Exception as e:
            if not small: raise
            log(f"{engine} couldn't speak this line ({str(e).splitlines()[-1][:160] if str(e) else type(e).__name__}) - the small voice says it")
    return supertonic(text, voice), "supertonic"

def health():
    names = {e: ("Orpheus 3B" if e == "orpheus" else "Sesame CSM-1B") if loaded[e] else "Supertonic 3" if can_speak(e) else "" for e in loaded}
    return {"orpheus": can_speak("orpheus"), "csm": can_speak("csm"), "voices": voices, "names": names, "loading": loading}

# ---------------- one thread speaks ----------------
# PyTorch keeps compiled CSM's CUDA graphs in the thread that set them up, and the web server gives every request a
# thread of its own: the third line threw, and CSM dropped to its uncompiled path for good (20-40 s a line). So every
# line is spoken on this one thread - which also means one line at a time, on the card or the processor.
def voice_thread():
    while True:
        fn, box, done = jobs.get()
        try: box["out"] = fn()
        except BaseException as e: box["error"] = e
        done.set()

def on_voice_thread(fn):
    box, done = {}, threading.Event()
    jobs.put((fn, box, done))
    done.wait()
    if "error" in box: raise box["error"]
    return box["out"]

# ---------------- serving ----------------
urgent, urgent_lock = 0, threading.Lock()  # real answers waiting: fillers hold back while > 0

def trim(audio):
    """Cut the silence the models leave at the end (and most at the start), keeping a short breath."""
    import numpy as np
    loud = np.nonzero(np.abs(audio) > 0.02)[0]
    if len(loud) == 0: return audio
    return audio[max(0, loud[0] - RATE // 20): min(len(audio), loud[-1] + RATE // 6)]

def to_wav(audio):
    import numpy as np
    pcm = (np.clip(audio, -1, 1) * 32767).astype("<i2").tobytes()
    b = io.BytesIO()
    with wave.open(b, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE); w.writeframes(pcm)
    return b.getvalue()

class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def reply(self, code, body, ctype):
        self.send_response(code); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)

    def do_GET(self):
        if self.path.startswith("/health"): self.reply(200, json.dumps(health()).encode(), "application/json")
        else: self.reply(404, b"", "text/plain")

    def do_POST(self):
        try:
            req = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            engine, text, voice = req.get("engine", "csm"), str(req.get("text", "")).strip(), int(req.get("voice", 0))
            if not text: return self.reply(400, b"no text", "text/plain")
            if not can_speak(engine): return self.reply(503, b"not ready", "text/plain")
            # "background" lines (the premade fillers) are kept on disk - made once per voice, ever - and wait
            # behind any real answer, so a suspect never answers late because fillers were recording
            background = bool(req.get("background"))
            # (kept per voice model, so a line the small voice recorded is never played back once the big one speaks)
            kind = engine if loaded.get(engine) else "supertonic"
            path = os.path.join(ROOT, "voices", "cache", f"{kind}_{voice}_{hashlib.md5(text.encode()).hexdigest()}.wav")
            if background and os.path.exists(path):
                with open(path, "rb") as f: return self.reply(200, f.read(), "audio/wav")
            global urgent
            t0 = time.time()
            if not background:
                with urgent_lock: urgent += 1
            try:
                while background and urgent > 0: time.sleep(0.1)
                audio, spoke = on_voice_thread(lambda: speak(engine, text, voice))
            finally:
                if not background:
                    with urgent_lock: urgent -= 1
            audio = trim(audio)
            log(f"{spoke} v{voice}: {len(audio) / RATE:.1f}s of speech in {time.time() - t0:.1f}s")
            wav = to_wav(audio)
            if background and spoke == kind:
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "wb") as f: f.write(wav)
            self.reply(200, wav, "audio/wav")
        except Exception as e:
            import traceback; log("failed:", traceback.format_exc()[-1500:])
            self.reply(500, str(e).encode(), "text/plain")

def shutdown():
    if llama: llama.kill()

def watch_parent(pid):
    """Quit when the game does, even if it crashed or was killed (so nothing is left holding the GPU)."""
    import ctypes
    k = ctypes.windll.kernel32
    h = k.OpenProcess(0x00100000, False, pid)  # SYNCHRONIZE
    if not h: return
    k.WaitForSingleObject(h, 0xFFFFFFFF)
    log("the game closed - stopping")
    shutdown()
    os._exit(0)

if __name__ == "__main__":
    atexit.register(shutdown)
    parent = os.environ.get("HU_PARENT_PID", "")
    if parent.isdigit():
        threading.Thread(target=watch_parent, args=(int(parent),), daemon=True).start()
    def start():
        global voices, loading
        voices = pick_voices()
        work = [threading.Thread(target=load_supertonic, daemon=True)]
        if voices == "cuda":
            on_voice_thread(setup_compile)
            work += [threading.Thread(target=load_orpheus, daemon=True), threading.Thread(target=load_csm, daemon=True)]
        elif os.environ.get("HU_VOICES", "").strip().lower() == "orpheus":
            work.append(threading.Thread(target=load_orpheus, daemon=True))
        for w in work: w.start()
        for w in work: w.join()
        loading = False
        names = health()["names"]
        log("phone lines:", names["orpheus"] or "no voice", "| everyone else:", names["csm"] or "no voice")
    threading.Thread(target=voice_thread, daemon=True).start()
    threading.Thread(target=start, daemon=True).start()
    log(f"listening on {PORT}")
    try:
        ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
    finally:
        shutdown()
