# Homicide Unit - the local voices. Free, offline, no keys or tokens. Started by the game (UHUVoiceSubsystem).
#   Orpheus 3B (Apache-2.0) - the 911 caller and tape audio. Run by llama-server (AI\llama) on port 8090,
#                             its audio codes turned into sound by SNAC (MIT, AI\snac).
#   Sesame CSM-1B (Apache-2.0) - suspects and everyone else. Run by transformers on the GPU (AI\csm\model).
# The game talks to this on http://127.0.0.1:8005:
#   GET  /health -> {"orpheus": bool, "csm": bool}
#   POST /tts {"engine": "orpheus"|"csm", "text": "...", "voice": 3, "background": false} -> 24 kHz 16-bit mono .wav
# Run with Tools\VoiceServer.bat (it loads Visual Studio's compiler, which makes CSM about 4x faster).
import hashlib, io, json, os, re, subprocess, sys, threading, time, urllib.request, wave, atexit
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "AI"))
PORT, LLAMA_PORT, RATE = 8005, 8090, 24000
ready = {"orpheus": False, "csm": False}
gpu = threading.Lock()  # one line at a time on the graphics card
llama = None
snac_model = None
csm_model = csm_proc = None
csm_seeds = {}  # voice number -> (text, audio): the first line in that voice, fed back so the voice stays the same

import shutil
# Visual Studio's C++ compiler: on the developer's PC (VoiceServer.bat loads it), almost never on a player's
HAS_CL = shutil.which("cl") is not None
try:
    import triton  # noqa: F401  (lets torch compile CSM: much faster)
    HAS_TRITON = True
    import torch._inductor.config as _ind
    _ind.triton.descriptive_names = False  # short kernel file names: Windows' 260-character path limit
    _ind.use_static_cuda_launcher = False  # its launcher overflows a Windows 32-bit long
    if not HAS_CL:
        # Without cl, two torch steps that build C++ on the CPU throw ("Compiler: cl is not found") and take every
        # CSM line down with them: the probe for CPU vector instructions, and the shape guards (which also come in
        # pure Python). CSM itself runs on the GPU, where Triton compiles with its own bundled tcc.
        import torch._dynamo.config as _dyn
        _ind.cpp.vec_isa_ok = False
        _dyn.enable_cpp_symbolic_shape_guards = False
except Exception:
    HAS_TRITON = False
csm_compiled = HAS_TRITON  # switched off for good if compiling ever fails (see csm_generate)

def log(*a):
    print("[voice]", *a, flush=True)

# ---------------- Orpheus ----------------
ORPHEUS_VOICES = ["tara", "leo", "leah", "dan", "jess", "zac", "mia", "zoe"]

def llama_post(path, body, timeout=120):
    req = urllib.request.Request(f"http://127.0.0.1:{LLAMA_PORT}{path}", json.dumps(body).encode(), {"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return json.loads(r.read())

def load_orpheus():
    global llama, snac_model
    try:
        import torch
        from snac import SNAC
        exe = os.path.join(ROOT, "llama", "llama-server.exe")
        llama = subprocess.Popen([exe, "-m", os.path.join(ROOT, "orpheus", "orpheus-3b-q4_k_m.gguf"), "--port", str(LLAMA_PORT),
                                  "--host", "127.0.0.1", "-ngl", "99", "-c", "4096", "--log-disable"],
                                 cwd=os.path.dirname(exe), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                 creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        snac_model = SNAC.from_pretrained(os.path.join(ROOT, "snac")).eval().to("cuda")
        for _ in range(240):
            try:
                with urllib.request.urlopen(f"http://127.0.0.1:{LLAMA_PORT}/health", timeout=2) as r:
                    if r.status == 200: break
            except Exception:
                time.sleep(1)
        ready["orpheus"] = True
        log("Orpheus ready")
    except Exception as e:
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
    clamp = lambda xs: torch.tensor([[min(max(x, 0), 4095) for x in xs]], device="cuda")
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
        ready["csm"] = True
        log("CSM ready")
    except Exception as e:
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
        if self.path.startswith("/health"): self.reply(200, json.dumps(ready).encode(), "application/json")
        else: self.reply(404, b"", "text/plain")

    def do_POST(self):
        try:
            req = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            engine, text, voice = req.get("engine", "csm"), str(req.get("text", "")).strip(), int(req.get("voice", 0))
            if not text: return self.reply(400, b"no text", "text/plain")
            if not ready.get(engine): return self.reply(503, b"not ready", "text/plain")
            # "background" lines (the premade fillers) are kept on disk - made once per voice, ever - and wait
            # behind any real answer, so a suspect never answers late because fillers were recording
            background = bool(req.get("background"))
            path = os.path.join(ROOT, "voices", "cache", f"{engine}_{voice}_{hashlib.md5(text.encode()).hexdigest()}.wav")
            if background and os.path.exists(path):
                with open(path, "rb") as f: return self.reply(200, f.read(), "audio/wav")
            global urgent
            t0 = time.time()
            if not background:
                with urgent_lock: urgent += 1
            try:
                while background and urgent > 0: time.sleep(0.1)
                with gpu:
                    audio = orpheus(text, voice) if engine == "orpheus" else csm(text, voice)
            finally:
                if not background:
                    with urgent_lock: urgent -= 1
            audio = trim(audio)
            log(f"{engine} v{voice}: {len(audio) / RATE:.1f}s of speech in {time.time() - t0:.1f}s")
            wav = to_wav(audio)
            if background:
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
    threading.Thread(target=load_orpheus, daemon=True).start()
    threading.Thread(target=load_csm, daemon=True).start()
    log(f"listening on {PORT}")
    try:
        ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
    finally:
        shutdown()
