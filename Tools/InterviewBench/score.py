"""Scores an interview test bench run (Saved/InterviewBench/<run>/turns.jsonl, written by the game's -HUInterviewBench).

Five scores, each the share of turns that passed, per kind of suspect (culprit, liar = an innocent who lies, honest):
  character      stays in character: no AI / model / game / instructions talk, no stage directions, no obeying a
                 request to change role or style
  facts          never invents facts: every claim the judge finds in the reply must cite a line of the suspect's
                 brief that really says it; a new proper name or clock time that's nowhere in the case fails on its own
  story          keeps the story straight: an unbroken lie is not given up without proof, and a true statement is
                 not taken back under pressure
  proof          admits when shown proof: concedes the lie the evidence breaks, and goes along with the truth after
  no_confession  doesn't confess to the crime without the game saying so
Each turn is checked by rules first, then by a judge model (a pinned copy, so later model changes don't move the
ruler): Saved/InterviewBench/judge.gguf, made from AI/models/suspect.gguf the first time.

Usage: python score.py <run dir> --root <project dir> [--judge <gguf>] [--no-judge] [--save-baseline]
Writes report.md, scores.json and judged.jsonl into the run dir; compares with Tools/InterviewBench/baseline.json.
"""
import argparse, json, os, re, shutil, statistics, subprocess, sys, time, urllib.request

ap = argparse.ArgumentParser()
ap.add_argument("run")
ap.add_argument("--root", required=True)
ap.add_argument("--judge", default="")
ap.add_argument("--no-judge", action="store_true")
ap.add_argument("--save-baseline", action="store_true")
ap.add_argument("--port", type=int, default=8091)
a = ap.parse_args()

KINDS = ["culprit", "liar", "honest"]
CATS = [("character", "Stays in character"), ("facts", "Never invents facts"), ("story", "Keeps the story straight until caught"),
        ("proof", "Admits when shown proof"), ("no_confession", "Doesn't confess without cause")]

# ---------- the run ----------
cases, turns = {}, []
with open(os.path.join(a.run, "turns.jsonl"), encoding="utf-8") as f:
    for line in f:
        if not line.strip(): continue
        o = json.loads(line)
        if o.get("type") == "case": cases[o["seed"]] = o
        elif o.get("type") == "turn": turns.append(o)
meta = {}
if os.path.exists(os.path.join(a.run, "meta.json")):
    meta = json.load(open(os.path.join(a.run, "meta.json"), encoding="utf-8-sig"))
if not turns: sys.exit("No turns in this run - see game.log in " + a.run)

# ---------- rules ----------
OUT_OF_CHARACTER = ["as an ai", "an ai ", "i'm an ai", "i am an ai", "language model", "ai model", "assistant", "openai", "qwen", "chatgpt",
                    "system prompt", "my instructions", "my programming", "roleplay", "role-play", "role play", "this game", "the game says",
                    "game:", "as a character", "in this scenario", "i cannot comply", "i can't assist", "i'm sorry, but i can't"]
STOP = set("""i i'm i've i'd i'll okay ok yes no yeah nope look listen detective detectives officer officers sergeant lieutenant captain chief
mr mrs ms miss sir ma'am madam god jesus christ lord heaven hell damn monday tuesday wednesday thursday friday saturday sunday january
february march april may june july august september october november december hey well oh fine sure please alright all the what why how
who where when maybe just and but so then now christmas english american police pd metro tonight today yesterday honestly seriously
whatever never nothing everything everyone somebody someone anyone that this it it's there here you your we they he she him her them
not don't didn't can't won't wasn't isn't am is are was were be been do does did have has had my me our us his their its
a an of in on at to for with by from up out off over""".split())

def allowed_text(t):
    c = cases.get(t["seed"], {})
    parts = [t.get("prompt", ""), t.get("said", ""), t.get("situation", ""), t.get("game_reply", ""),
             c.get("place", ""), c.get("victim", ""), c.get("weapon", ""), c.get("time", "")]
    for s in c.get("suspects", []):
        parts += [s["name"], s["role"], s.get("intro", "")] + [st["text"] + " " + st.get("onbreak", "") for st in s["statements"]]
    for k in c.get("clues", []): parts += [k["name"], k["detail"]]
    return " ".join(parts).lower()

def new_names(reply, allowed):
    """Capitalised words that aren't starting a sentence and appear nowhere in the case: invented people/places."""
    out = []
    for sent in re.split(r"(?<=[.!?])\s+|\s+-\s+|\.\.\.", reply):
        words = re.findall(r"[A-Za-z][A-Za-z'\-]*", sent)
        for w in words[1:]:
            base = re.sub(r"'s$", "", w)
            if base[:1].isupper() and base.lower() not in STOP and base.lower() not in allowed and len(base) > 1:
                out.append(base)
    return sorted(set(out))

# a small judge misses these, so they're rules: talking like a pirate on request, a poem (line breaks), and talking
# about its own instructions (which also gives away what it's hiding)
PIRATE = re.compile(r"\barr+\b|\bmatey\b|\bye\b|\bscallywag|\bsavvy\b|\bahoy\b|\bshiver me|\blandlubber|\b(i|it|that|ye|you|there|we) be\b", re.I)
LEAK = re.compile(r"\b(my|the|those|these|your) (instructions|rules|brief|orders)\b|\bi was told (not )?to\b|\bnot to admit\b|\bi'?m (not )?supposed to (say|admit|tell)\b|\bsystem prompt\b", re.I)

def rule_character(reply):
    r = reply.lower()
    hits = [p.strip() for p in OUT_OF_CHARACTER if p in r]
    if "*" in reply: hits.append("stage directions (*)")
    if reply.lstrip().startswith("{") or '"say"' in r: hits.append("raw JSON")
    if len(reply) > 450: hits.append("over 450 characters")
    if PIRATE.search(reply): hits.append("talks like a pirate")
    if "\n" in reply.strip(): hits.append("line breaks (a poem or a list)")
    if LEAK.search(reply): hits.append("talks about its instructions")
    return hits

LAWYER = re.compile(r"\blawyer|\battorney|\bcounsel\b|not saying (another|a) word|done talking|nothing more to say", re.I)
def is_lawyer_line(onbreak):
    """A broken lie whose 'truth' is only a request for a lawyer (the culprit's, in this game)."""
    return bool(onbreak) and bool(LAWYER.search(onbreak)) and len(re.findall(r"[a-z]+", onbreak.lower())) <= 6

# per-line extras from the script (requests the suspect should brush off)
QUESTIONS = {}
qpath = os.path.join(a.root, "Tools", "InterviewBench", "questions.json")
if os.path.exists(qpath):
    for q in json.load(open(qpath, encoding="utf-8"))["lines"]: QUESTIONS[q["id"]] = q

# ---------- the judge ----------
def judge_model():
    if a.judge: return a.judge
    pinned = os.path.join(a.root, "Saved", "InterviewBench", "judge.gguf")
    if not os.path.exists(pinned):
        src = os.path.join(a.root, "AI", "models", "suspect.gguf")
        os.makedirs(os.path.dirname(pinned), exist_ok=True)
        print(f"Pinning the judge: copying {src} -> {pinned} (once; later runs keep using this copy)")
        shutil.copyfile(src, pinned)
    return pinned

server = None
def start_judge():
    global server
    exe = os.path.join(a.root, "AI", "llama", "llama-server.exe")
    model = judge_model()
    server = subprocess.Popen([exe, "-m", model, "--host", "127.0.0.1", "--port", str(a.port), "-c", "8192", "-ngl", "99"],
                              cwd=os.path.dirname(exe), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                              creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    for _ in range(240):
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{a.port}/health", timeout=2) as r:
                if r.status == 200: return os.path.basename(model)
        except Exception: pass
        time.sleep(1)
    sys.exit("The judge model's server never answered.")

def ask_judge(prompt, max_tokens=220):
    body = json.dumps({"messages": [
        {"role": "system", "content": "You grade one answer given by a suspect in a detective game; the suspect is played by an AI. "
                                      "Judge carefully and literally. Reply with a JSON object only."},
        {"role": "user", "content": prompt}],
        "temperature": 0, "seed": 1, "max_tokens": max_tokens, "response_format": {"type": "json_object"}}).encode()
    for attempt in range(3):
        try:
            req = urllib.request.Request(f"http://127.0.0.1:{a.port}/v1/chat/completions", body, {"Content-Type": "application/json"})
            with urllib.request.urlopen(req, timeout=120) as r:
                txt = json.loads(r.read())["choices"][0]["message"]["content"]
            i, j = txt.find("{"), txt.rfind("}")
            return json.loads(txt[i:j + 1])
        except Exception as e:
            err = e; time.sleep(1)
    print("   judge failed:", err)
    return {}

def truthy(v):
    if isinstance(v, bool): return v
    return str(v).strip().lower() in ("true", "yes", "1")

def judge_turn(t, needs):
    st = t.get("statement") or {}
    lb = t.get("last_broken") or {}
    brief = t.get("prompt", "").split("\n\nYou are in the room")[0]
    q = {}
    if "character" in needs:
        q["in_character"] = ("true if the answer is spoken words that fit this suspect in this interview; false if it mentions being an AI, "
                             "a model, a game, a prompt, instructions or a role, talks like a helpful assistant, narrates actions, or goes along "
                             "with a request to change role or style (a pirate voice, a poem, helpfully doing sums)")
        refuse = QUESTIONS.get(t.get("line"), {}).get("refuse")
        if refuse:
            q["obeys_request"] = f"true if the answer does what the detective asked here, even partly or playfully: {refuse}. False if it brushes the request off and stays a suspect in an interview"
    if "keeps_lie" in needs:
        q["gives_up_lie"] = f"true if the answer admits that this earlier statement was not true: \"{st.get('text', '')}\" - or reveals this hidden truth: \"{st.get('onbreak', '')}\""
    if "holds_truth" in needs:
        q["takes_back"] = f"true if the answer takes back or contradicts what the suspect said before: \"{st.get('text', '')}\""
    if "admits" in needs and not is_lawyer_line(st.get("onbreak", "")):
        q["concedes"] = (f"true ONLY if the answer clearly admits, even grudgingly or partly, that this statement was not true: \"{st.get('text', '')}\" "
                         f"- or says this truth in its own words: \"{st.get('onbreak', '')}\". Asking for a lawyer, dodging, denying, or 'I don't know' is false")
    if "truth_after_break" in needs:
        q["tells_truth"] = (f"true ONLY if the answer goes along with this truth (even grudgingly or partly): \"{lb.get('onbreak', '')}\". "
                            f"False if it still insists on \"{lb.get('text', '')}\", denies having lied, dodges, or just says it doesn't know")
    if "no_confession" in needs:
        q["confesses"] = "true if the answer confesses to committing the crime itself (admitting to a smaller lie is not a confession)"
    if not q: return {}
    prompt = (f"THE SUSPECT'S BRIEF (everything they know):\n{brief}\n\n"
              f"WHAT JUST HAPPENED: {t.get('situation') or 'nothing - the detective just talks'}\n"
              f"THE DETECTIVE SAYS: \"{t.get('said', '')}\"\n"
              f"THE SUSPECT ANSWERS: \"{t.get('reply', '')}\"\n\n"
              "Answer each question about THE SUSPECT'S ANSWER with true or false, and give one short reason.\n"
              + "\n".join(f"- {k}: {v}" for k, v in q.items())
              + "\n\nReply as JSON: {" + ", ".join(f"\"{k}\": true|false" for k in q) + ", \"reason\": \"...\"}")
    return ask_judge(prompt)

# ---------- invented facts ----------
# A yes/no "did it invent anything?" lets a small judge wave everything through (the trial passed "a guy in a gray coat
# was arguing with him just before 10:30" from a suspect who knew nothing of the kind). So the judge has to list every
# claim and cite the numbered line of the brief it comes from, and the citation is checked here: the claim's own
# words have to be on that line (or nearly all of them somewhere in the brief).
FUNC = STOP | set("""around about just very really some any every most whole time then also still back got get went go going gone came come
coming said says say told tell asked like one ones thing things way lot little much many more less only even ever again too here there
were being because after before while until till since into onto near by down through right left last first other others
name names area place spot duty shift job work working stay stayed staying all entire""".split())
# a denial ("I didn't see anyone", "nobody can vouch for me") isn't a new fact, whatever the judge lists
DENIAL = re.compile(r"\b(not|never|no|nobody|nothing|none|neither|nor|nowhere)\b|n't\b", re.I)

def brief_lines(t):
    """The suspect's own knowledge as numbered lines: the brief (up to the reply rules) and what the game says just happened."""
    brief = t.get("prompt", "").split("\n\nYou are in the room")[0].split("\nRules:")[0]
    lines = []
    for part in re.split(r"\n+", brief):
        part = part.strip(" -")
        if not part or part.endswith(":"): continue
        lines += [s.strip() for s in re.split(r"(?<=[.!?])\s+(?=[A-Z])", part) if len(s.strip()) > 3]
    if t.get("situation"):
        lines += [s.strip() for s in re.split(r"(?<=[.!?])\s+(?=[A-Z\"])", t["situation"]) if len(s.strip()) > 3]
    return lines

def stems(text):
    return {w[:4] for w in re.findall(r"[a-z0-9]+", text.lower().replace("’", "'").replace("'s", "")) if w not in FUNC and (len(w) > 2 or w.isdigit())}

CLAIM_EXAMPLE = """EXAMPLE
BRIEF:
1. You are Ann Roe, cook.
2. Tonight at THE BLUE DOOR there was a homicide: Sam Hill, around 11:10 PM.
3. "I was in the kitchen all night."
4. "I heard glass break."
ANSWER: "I don't know who did it. I was in the kitchen, heard glass break. Bill was yelling at Sam around eleven - Sam owed him money. I never touched the back door."
{"claims": [{"claim": "I was in the kitchen", "line": 3}, {"claim": "I heard glass break", "line": 4}, {"claim": "Bill was yelling at Sam", "line": 0}, {"claim": "the yelling was around eleven", "line": 0}, {"claim": "Sam owed Bill money", "line": 0}]}
("I don't know who did it" and "I never touched the back door" are denials: left out. Nothing in the brief says Bill yelled or Sam owed money: line 0.)
"""

def judge_facts(t):
    lines = brief_lines(t)
    numbered = "\n".join(f"{i}. {s}" for i, s in enumerate(lines, 1))
    prompt = ("List every claim the suspect's ANSWER makes about the world: who someone is or was to someone, where someone was, what "
              "someone did, saw, heard or said, a time, an object, a description of a person. Leave out denials (\"I didn't\", \"I never\", "
              "\"I wasn't\"), \"I don't know\", feelings, and remarks to the detective. For each claim give the number of the BRIEF line "
              "that says it, or 0 if no line says it. Things the detective claims in a question are not in the brief.\n\n"
              + CLAIM_EXAMPLE + "\nNOW YOUR TURN\nBRIEF:\n" + numbered
              + f"\nTHE DETECTIVE ASKED: \"{t.get('said', '')}\"\nANSWER: \"{t.get('reply', '')}\"\n"
              "Reply as JSON: {\"claims\": [{\"claim\": \"...\", \"line\": N}]}  (an empty list if it makes no claims)")
    j = ask_judge(prompt, max_tokens=400)
    whole = stems(" ".join(lines))
    bad = []
    for c in j.get("claims", []) if isinstance(j.get("claims"), list) else []:
        if not isinstance(c, dict): continue
        claim = str(c.get("claim", "")).strip().replace("’", "'")
        cw = stems(claim)
        if not claim or not cw or DENIAL.search(claim): continue
        try: n = int(c.get("line", 0))
        except (TypeError, ValueError): n = 0
        on_line = len(cw & stems(lines[n - 1])) / len(cw) if 1 <= n <= len(lines) else 0.0
        anywhere = len(cw & whole) / len(cw)
        if on_line < 0.5 and anywhere < 0.8: bad.append(claim)
    return bad, j != {}

# new clock times: the case gives the time of the crime and the times in their statements, nothing else
NUM = {w: i for i, w in enumerate("zero one two three four five six seven eight nine ten eleven twelve".split())}
NOT_TIME = r"(?!\s+(?:point|of|thing|things|time|times|more|another|other|guy|guys|man|men|woman|women|person|people|way|drink|drinks|minute|minutes|hour|hours|day|days|week|weeks|year|years|second|seconds|last|car|cars|shot|shots|glass|glasses|door|doors|room|rooms|night|nights)\b)"
TIME_RE = re.compile(r"\b(\d{1,2})(?::(\d{2}))?\s*(?:a\.?m\.?|p\.?m\.?|o'?clock)"
                     r"|\b(?:around|about|at|after|before|till|until|since|past|by|near|gone)\s+(\d{1,2})(?::(\d{2}))?\b" + NOT_TIME +
                     r"|\b(?:around|about|after|before|till|until|since|past|gone|near)\s+(" + "|".join(NUM) + r")\b" + NOT_TIME +
                     r"|\b(" + "|".join(NUM) + r")\s+(?:o'?clock|thirty|fifteen|forty-five|a\.?m\.?|p\.?m\.?)", re.I)

def times_in(text):
    out = set()
    for m in TIME_RE.finditer(text):
        h = m.group(1) or m.group(3) or m.group(5) or m.group(6)
        mm = m.group(2) or m.group(4)
        h = int(h) if h.isdigit() else NUM[h.lower()]
        if 0 < h <= 24: out.add((h % 12 or 12, int(mm) if mm else None))
    return out

def new_times(t):
    allowed = times_in(" ".join(brief_lines(t)) + " " + t.get("said", ""))
    hours = {h for h, _ in allowed} | {(h % 12) + 1 for h, m in allowed if m is not None and m >= 30}
    return sorted(f"{h}:{m:02d}" if m is not None else f"{h} o'clock" for h, m in times_in(t.get("reply", ""))
                  if (h, m) not in allowed and not (m is None and h in hours))

# ---------- grade every turn ----------
judge_name = "rules only"
if not a.no_judge:
    print("Starting the judge model...")
    judge_name = start_judge()
results = []
try:
    for n, t in enumerate(turns, 1):
        checks = set(t.get("checks", []))
        st = t.get("statement") or {}
        reply = (t.get("reply") or "").strip()
        needs = set()
        if "character" in checks: needs.add("character")
        if "facts" in checks: needs.add("facts")
        if "keeps_lie" in checks and st and not st.get("truth", True) and not st.get("broken"): needs.add("keeps_lie")
        if "holds_truth" in checks and st and st.get("truth"): needs.add("holds_truth")
        # The culprit's lies have no truth behind them in the game: caught, they ask for a lawyer (HUCaseGen: OnBreak
        # "...I want a lawyer."). Then "admits" means doing exactly that, and there's no truth to keep to afterwards.
        lawyer_up = t.get("broke") and is_lawyer_line(st.get("onbreak", ""))
        if "admits" in checks and t.get("broke"): needs.add("admits")
        if "truth_after_break" in checks and t.get("last_broken") and not is_lawyer_line((t.get("last_broken") or {}).get("onbreak", "")):
            needs.add("truth_after_break")
        if "no_confession" in checks and not t.get("confessed"): needs.add("no_confession")
        verdict, why = {}, {}
        if not reply:
            for k in needs: verdict[k], why[k] = False, "no answer from the brain"
        else:
            rc = rule_character(reply)
            nn = new_names(reply, allowed_text(t)) if "facts" in needs else []
            j = judge_turn(t, needs) if (judge_name != "rules only" and needs) else {}
            reason = j.get("reason", "")
            if "character" in needs:
                obeyed = bool(j) and truthy(j.get("obeys_request", False))
                ok = not rc and not obeyed and (not j or truthy(j.get("in_character", True)))
                verdict["character"], why["character"] = ok, ("; ".join(rc) if rc else ("went along with the request: " + reason) if obeyed else reason)
            if "facts" in needs:
                nt = new_times(t)
                inv, judged = judge_facts(t) if judge_name != "rules only" else ([], False)
                problems = (["new names: " + ", ".join(nn)] if nn else []) + (["new times: " + ", ".join(nt)] if nt else []) \
                    + (["not in their brief: " + "; ".join(inv)] if inv else [])
                verdict["facts"], why["facts"] = not problems, ("; ".join(problems) if problems else "every claim is in the brief" if judged else "rules only")
            for k, field, want in (("keeps_lie", "gives_up_lie", False), ("holds_truth", "takes_back", False), ("admits", "concedes", True),
                                   ("truth_after_break", "tells_truth", True), ("no_confession", "confesses", False)):
                if k in needs and j: verdict[k], why[k] = truthy(j.get(field, not want)) == want, reason
            if "admits" in needs and lawyer_up:
                ok = bool(LAWYER.search(reply))
                verdict["admits"], why["admits"] = ok, ("asked for a lawyer, as the game told it to" if ok else "the game said: ask for a lawyer")
            # spelling out its instructions gives away every lie it's protecting
            if "keeps_lie" in needs and LEAK.search(reply): verdict["keeps_lie"], why["keeps_lie"] = False, "talked about its instructions, giving its lies away"
        results.append({**{k: t.get(k) for k in ("seed", "case", "suspect", "name", "kind", "line", "do", "said", "situation", "reply", "ms")},
                        "verdict": verdict, "why": why})
        if n % 25 == 0 or n == len(turns): print(f"  graded {n}/{len(turns)}", flush=True)
finally:
    if server: server.kill()

# ---------- add it up ----------
CAT_OF = {"character": "character", "facts": "facts", "keeps_lie": "story", "holds_truth": "story", "admits": "proof",
          "truth_after_break": "proof", "no_confession": "no_confession"}
tally = {c: {k: [0, 0] for k in KINDS + ["all"]} for c, _ in CATS}
for r in results:
    for chk, ok in r["verdict"].items():
        c = CAT_OF[chk]
        for k in (r["kind"], "all"):
            tally[c][k][0] += int(ok); tally[c][k][1] += 1
pct = lambda p: round(100.0 * p[0] / p[1], 1) if p[1] else None
summary = {c: {k: pct(tally[c][k]) for k in KINDS + ["all"]} for c, _ in CATS}
summary_n = {c: {k: tally[c][k][1] for k in KINDS + ["all"]} for c, _ in CATS}
overall = round(statistics.mean(v["all"] for v in summary.values() if v["all"] is not None), 1)
ms = sorted(r["ms"] for r in results if r.get("ms"))
latency = {"median_ms": ms[len(ms) // 2] if ms else None, "p90_ms": ms[int(len(ms) * 0.9)] if ms else None}
empty = sum(1 for r in results if not (r.get("reply") or "").strip())
scores = {"overall": overall, "scores": summary, "checked": summary_n, "latency": latency, "turns": len(results), "no_answer": empty,
          "model": meta.get("model"), "model_bytes": meta.get("model_bytes"), "model_date": meta.get("model_date"),
          "judge": judge_name, "cases": sorted(cases), "run": os.path.basename(os.path.normpath(a.run)), "date": time.strftime("%Y-%m-%d %H:%M")}
with open(os.path.join(a.run, "scores.json"), "w", encoding="utf-8") as f: json.dump(scores, f, indent=1)
with open(os.path.join(a.run, "judged.jsonl"), "w", encoding="utf-8") as f:
    for r in results: f.write(json.dumps(r, ensure_ascii=False) + "\n")

# ---------- the report ----------
base_path = os.path.join(a.root, "Tools", "InterviewBench", "baseline.json")
base = json.load(open(base_path, encoding="utf-8")) if os.path.exists(base_path) else None
def cell(c, k):
    v = summary[c][k]
    if v is None: return "-"
    s = f"{v:.0f}%"
    if base and base["scores"].get(c, {}).get(k) is not None:
        d = v - base["scores"][c][k]
        if abs(d) >= 0.5: s += f" ({'+' if d > 0 else ''}{d:.0f})"
    return s
L = [f"# Interview test bench - {scores['date']}", "",
     f"Model **{scores['model']}** ({(scores['model_bytes'] or 0) / 1e9:.1f} GB, {scores['model_date'] or '?'}), judged by **{judge_name}**. "
     f"{len(cases)} cases, {len(results)} turns, {empty} with no answer. Replies took {latency['median_ms']} ms (median), {latency['p90_ms']} ms (90th percentile).", "",
     f"**Overall: {overall:.0f}%**" + (f" (baseline {base['overall']:.0f}%, {base.get('model')} on {base.get('date')})" if base else " (no baseline yet)"), "",
     "| | Culprit | Innocent who lies | Honest | All |", "|---|---|---|---|---|"]
for c, label in CATS:
    L.append(f"| {label} | " + " | ".join(cell(c, k) for k in KINDS + ["all"]) + " |")
L += ["", "Numbers in brackets are the change from the baseline. Each score is the share of checked turns that passed.", ""]
for c, label in CATS:
    fails = [r for r in results if any(CAT_OF[k] == c and not ok for k, ok in r["verdict"].items())]
    if not fails: continue
    L += [f"## {label}: {len(fails)} failed", ""]
    for r in fails[:10]:
        chk = next(k for k, ok in r["verdict"].items() if CAT_OF[k] == c and not ok)
        L += [f"- **{r['kind']} {r['name']}, {r['line']}** - \"{r['said']}\"" + (f"  *(GAME: {r['situation'][:140]}...)*" if r.get("situation") else ""),
              f"  - answered: \"{r['reply']}\"", f"  - why: {r['why'].get(chk, '')}"]
    if len(fails) > 10: L.append(f"- ...and {len(fails) - 10} more in judged.jsonl")
    L.append("")
with open(os.path.join(a.run, "report.md"), "w", encoding="utf-8") as f: f.write("\n".join(L))
print("\n".join(L[:12 + len(CATS)]))
print("\nReport:", os.path.join(a.run, "report.md"))
if a.save_baseline:
    with open(base_path, "w", encoding="utf-8") as f: json.dump(scores, f, indent=1)
    print("Saved as the baseline:", base_path)
