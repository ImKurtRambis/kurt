# Playing a case online between two homes

A squad of two plays one full shift over Steam, each detective on their own PC in their own home: join by code,
roll call, briefing, the scene, the precinct, the Captain's verdict. Everything below is what to do and what to look
for; it takes about 30 minutes.

What's already been checked on one PC: `Tools\TestSquadShift.ps1` plays the same shift with two copies of the game
over a network made to behave like two homes (60 ms each way, jitter, 1% of packets lost) and compares the two PCs'
cases at every step. On real Steam, one PC and one account: the host's squad is a Steam lobby carrying its code, and
the worldwide search finds it by the code and heads for the host's Steam address. What only two homes (two accounts)
can show: actually connecting through Steam's relays (no port forwarding), Unreal's own Steam search returning the
squad (it ignores lobbies of your own account, so one account can't test it), and two real people playing at once.

## What you need

- Two Windows PCs, in two different homes (different internet connections).
- Two Steam accounts, each signed in to Steam on its own PC. Steam must be running before the game starts.
  The game uses Steam's test app until it has its own App ID, so Steam shows you as playing **Spacewar**.
- The same build on both PCs. `Version.txt` beside `HomicideUnit.exe` (and the corner of the desk menu) must match;
  two different builds can't join each other ("a different version of the game").

## Making the build

On the dev PC:

```
powershell -ExecutionPolicy Bypass -File Tools\Package.ps1 -Config Development -NoAI
```

- `-Config Development` writes a log (`HomicideUnit\Saved\Logs\HomicideUnit.log` inside the build folder), which is
  what to send back if something goes wrong. Shipping writes none.
- `-NoAI` leaves out the local AI (several GB): suspects answer with written lines. Leave it off to test the voices
  and the thinking suspects too; the AI runs on each PC by itself and isn't part of the online play.
- Zip `Packaged\Windows` and send the same zip to both PCs.

## The shift

**Host (PC 1):** at the desk, pick up the **desk phone** ("Call in your squad"). The squad room opens and its board
shows a 6-digit code. Send the code to your partner.

**Partner (PC 2):** at the desk, pick up the **cell phone** ("Answer a squad's page"), type the 6 numbers, Enter.
Or, with the game open at the desk: in the Steam friends list, right-click the host and **Join Game**, or accept the
host's invite (Shift+Tab, Invite). This way is new and untested; if it does nothing, use the code.

Then play it through together, and tick each line where **both screens agree**:

1. **Roll call.** Both detectives are on the board's roster. Both punch in at the time clock (or Y): the countdown
   starts, and both arrive at the briefing.
2. **Briefing.** One of you clicks through the laptop: the projector changes on both screens. The partner stands up
   with W and walks around: the host sees them get up and move (not stuck in the seat).
3. **Motor pool.** One of you goes to the car alone and presses E: nothing happens, the prompt says it's waiting for
   the squad, and nobody's screen goes black. With both at the car, E drives everyone to the scene.
4. **The scene.** One of you bags evidence: each piece disappears for both of you, and the "EVIDENCE BAGGED" card
   shows. The other interviews a suspect: when a lie breaks, "LIE ON THE RECORD" shows on both screens with the same
   count. Cuff the culprit (belt, cuffs), take them by the arm, walk them to the van and put them in (try it while
   still walking). The van's prompt says the same number inside for both of you.
5. **Back to the precinct** in the car. The prisoner is in holding for both. Book them at the counter (3 steps):
   "BOOKED" for both.
6. **The Captain.** Submit the case at the in-tray. Both get the same verdict (the first sentence: "CASE CLOSED",
   "CASE CLOSED (THIN)", "WRONG ARREST" or "CASE LEFT OPEN"). The pay and XP after it are each detective's own.

## Also worth trying

- Joining from far away: Unreal's own Steam search only looks in your part of the world; the game then asks Steam
  worldwide by itself (the status says "Paging squad ... worldwide on Steam"). A partner on another continent should
  still get in, just a few seconds slower.
- A wrong code: "No squad with code ... answered, anywhere on Steam".
- The partner quitting mid-shift, then joining again by the code.

## If something's wrong

Note the step and what each screen showed (a screenshot from each PC is best), and send from **both** PCs:

- `Version.txt`
- `HomicideUnit\Saved\Logs\HomicideUnit.log` (only in a Development build)

The things most likely to go wrong online are the two PCs disagreeing (evidence gone on one screen but not the
other, a suspect in the van on one PC only, a different verdict), someone frozen or stuck in a black screen, or the
car not leaving with both detectives beside it.
