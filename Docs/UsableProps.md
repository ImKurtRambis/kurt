# Usable props — the walk-through (checklist D-44)

Every room, every prop that looks like you could use it. **Before** is what E did before this pass. **Now** is what it
does (✅ done in this pass) or will do (⬜ still to do). Things that already worked are listed once at the end of each
room so nothing is missed.

The system behind it: `Source/HomicideUnit/HUUsables.*`. A prop is *marked* with what it is (drawer, files, locker,
phone, seat, bench, light switch, TV, lamp, water, clock, something to read); E on a marked prop does that thing on
every squad member's machine (the use is replayed like any other, finding the part by a name built from its kind and
place). Real furniture placed with `HUBuild::Prop` is marked where it's placed (`HUUsables::MarkProp`), so a desk
anywhere has drawers and a phone anywhere has a dial tone; blocks are marked by hand with `HUUsables::Mark`, and
`HUUsables::AddSwitch` puts a light switch on a wall for a room. A room's own interaction (the in-tray, the desk
computers, the case board...) always comes first.

Check it: `-HUUseTest` on `L_Precinct` or `L_Lobby` counts what's usable, uses one of each kind and saves a
screenshot of each (`Use_<Kind>`) to `Saved/Screenshots`.

| Kind | E does | Q does |
|---|---|---|
| Drawer | slides a drawer out (a tray with papers in it), a line about what's inside; again shuts it | |
| Files | opens the cabinet's top drawer of hanging folders | pulls an old case file to read |
| Locker | swings the door open (the open locker model) / shut, a line about what's inside | |
| Phone | dial tone, the switchboard, the shared line | |
| Seat / Bench | sit (a bench: where you're standing along it) / stand up | |
| Switch | the room's lights and its light panels off / on | |
| Screen | the TV on (a flickering picture, the late news) / off | |
| Lamp | a warm light on / off | |
| Drink, Clock, Read | a cup of water; the time (your PC's clock); what's written on it | |

## Desk menu (L_Menu)

The menu is the desk, clicked with the mouse: the case files, computer, desk phone, cell phone, manual, paper, name
plate, photo and keys already do things, and the lamp already lights the desk. Real furniture here is left unmarked
on purpose (it's a menu, not a room you walk).

| Prop | Before | Now |
|---|---|---|
| Venetian blinds | nothing | ⬜ click: raise / lower (the streetlight stripes come and go) |
| Desk lamp | lights the desk | ⬜ click: on / off |
| Filing cabinet, coat stand, cold coffee, ashtray, pens | nothing | ⬜ a line each when clicked |

## Roll call (L_Lobby)

| Prop | Before | Now |
|---|---|---|
| 12 lockers (one open) | nothing | ✅ open / shut; a line about what's inside |
| Lectern and microphone | nothing | ✅ tap the mic: the sergeant's notes |
| Two notice boards | nothing | ✅ read the notices |
| Duty roster | nothing | ✅ read it |
| Light switch | there wasn't one | ✅ a switch by the doors: the room's lights off / on |
| Case table | nothing (the laptop and folder on it work) | ✅ the drawer under it |
| Water cooler | nothing | ✅ a cup of water |
| Wall clock | nothing | ✅ the time |

Already working: the chalkboard (copy the code), the laptop and folder (host's case picks), the time clock, the 8
benches (sit), the double doors.

## The precinct (L_Precinct)

`-HUUseTest` counts: 22 drawers, 18 filing cabinets, 28 lockers, 21 phones, 32 seats, 4 benches, 15 light switches,
a TV, a water cooler, a clock (plus, from this pass's last additions, the Captain's couch, bookcase and plaques, and
each office's lamps, couch and filing cabinet when its owner has them).

| Room | Prop | Before | Now |
|---|---|---|---|
| Lobby | Front desk | nothing | ✅ drawers, on the sergeant's side |
| | Desk phone | nothing | ✅ pick up: dial tone, the switchboard |
| | Public benches | nothing | ✅ sit |
| Locker room | 28 lockers | nothing | ✅ open / shut |
| | 2 benches | nothing | ✅ sit |
| | 3 shower heads | nothing | ⬜ run the water |
| Break room | Table and 4 chairs | nothing | ✅ sit |
| | Counter cupboards | nothing | ✅ drawers |
| | TV | nothing | ✅ on / off (the late news) |
| | Microwave, fridge, vending machine, sink | nothing | ⬜ open, buy, run the tap |
| Captain's office | Big chair | nothing | ✅ sit |
| | Couch | nothing | ✅ sit |
| | Bookcase, plaques | nothing | ✅ read |
| | Desk | the in-tray works | ⬜ drawers (the desk is a top and a modesty panel: no drawer to pull yet) |
| | Flags | nothing | — (props, not things you use) |
| Evidence | Shelving and boxes, property lockers | nothing | ⬜ look through a box |
| Interview rooms | 3 chairs each | nothing | ✅ sit |
| | One-way mirror, camera | nothing | ⬜ |
| Bullpen | 16 desks | nothing | ✅ drawers |
| | 16 desk phones | nothing | ✅ pick up |
| | 16 chairs | nothing | ✅ sit |
| | Water cooler | nothing | ✅ a cup of water |
| | Wall clock | nothing | ✅ the time |
| | Printer, whiteboard | nothing | ⬜ |
| Booking | Camera, height chart, fingerprint station | the counter works; these nothing | ⬜ |
| Holding | Benches, toilets | nothing | ⬜ (cell doors: there are none yet) |
| Records | 18 filing cabinets | nothing | ✅ drawers, and pull an old file to read |
| | Copier | nothing | ⬜ |
| Corridors | Notice board | nothing | ⬜ |
| | Doorways | no doors in them | ⬜ door leaves that swing open / shut |
| Every room | Light switch | there weren't any | ✅ a switch inside each door: lobby, locker room, break room, Captain, evidence, both interview rooms, bullpen, booking, holding, records, the 4 offices |
| Personal offices | Desk phone | nothing | ✅ pick up |
| | Desk drawers | nothing | ✅ drawers |
| | Desk lamp, floor lamp (when the owner has them) | nothing | ✅ on / off |
| | Couch (when the owner has one) | nothing | ✅ sit |
| | Filing cabinet (when the owner has one) | nothing | ✅ drawers, pull a file |
| | Fish tank, plant, bookshelf, coat rack, trophy shelf | nothing | ⬜ |

Already working: the desk computers (PrecinctOS), the office chairs at your own desk, the case board, the upgrades
board, the case wall, booking, the Captain's in-tray, the coffee, the motor-pool door, the roll-call doors, each
office's catalogue.

## Briefing room and motor pool (L_Briefing)

| Prop | Before | Now |
|---|---|---|
| Projector screen, projector | the Captain's slides play on their own | ⬜ |
| Doorway to the corridor | no door | ⬜ |
| Light switch | there isn't one | ⬜ |
| Roll-up doors (4 bays) | nothing | ⬜ raise / lower |
| Tool chests | nothing | ⬜ drawers |
| Tires, drums | nothing | — (props, not things you use) |

Already working: the seats (sit), the podium (the Captain), the laptop, the supply locker, the cruiser.

## Crime scenes (L_Scene)

| Prop | Before | Now |
|---|---|---|
| Doorways between rooms | no doors | ⬜ |
| Hotel front desk, key boxes, bar | nothing | ⬜ drawers, the key for a room |
| Hotel beds, nightstands | nothing | ⬜ sit, drawers |
| Laundry machines, boiler | nothing | ⬜ |
| Dumpster | nothing | ⬜ lift the lid |
| Patrol car, streetlights | nothing | — |

Already working: furniture you push and pull, ladders, evidence, suspects, the victim, the officer, your cruiser,
the prisoner van, the coffee by the tape, the security desk.

## Still to do, biggest first

1. **Doors in doorways** (precinct corridors, briefing room, crime scenes): door leaves that swing. Today the
   doorways are open holes; a door needs collision that moves for everyone at once and AI/pathing that can pass it.
2. **Roll-up doors** in the motor pool.
3. **The desk menu's blinds and lamp**: click to raise / lower, on / off.
4. **The break room's appliances**: microwave, fridge, vending machine (spend a dollar), the sink.
5. **Showers** that run, **evidence boxes** to look through, the copier and printer.
6. **Crime-scene furniture**: hotel desk drawers, nightstands, beds, the dumpster lid.
