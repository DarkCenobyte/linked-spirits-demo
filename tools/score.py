#!/usr/bin/env python3
"""
LINKED//SPIRITS - the score.  Single source of truth for music AND timing.

100 BPM, 4/4, one bar = 2.4 s, positions in 16ths (0.15 s).
Home key D minor.  Colour language in harmony:
  Android  (green)      : D aeolian, Bb-F-C, stepwise "falling light" motif X = A-F-E-D
  Cyborg   (red + blue) : phrygian b2 (Eb) and the neighbour figure Y = A-Bb-A
  Duo                   : verse progression; A's verse melody and B's dialogue melody
                          turn out to be the two halves of one counterpoint
  Final quatrain        : lifted a whole tone (E minor), ends on B major (another thing)
  Ending                : Dsus2 - neither major nor minor.
"""
NOTE = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def m(s):
    """'C#4' -> midi"""
    n = NOTE[s[0]]; i = 1
    while s[i] in '#b':
        n += 1 if s[i] == '#' else -1; i += 1
    return n + 12 * (int(s[i:]) + 1)


# ---------------------------------------------------------------- harmony (one chord per bar)
# quality: m minor, M major, 7 dominant, s sus4, 2 sus2, m9 minor9, M7 major7, m6
CH = {}
def chords(bar, seq):
    for i, c in enumerate(seq.split()):
        CH[bar + i] = c

chords(0,   'Dm Dm Dm Dm Dm Dm Dm Dm Dm9 BbM7 Gm9 As Dm9 BbM7 FM7 A7')          # opening
chords(16,  'Dm Bb F C')                                                        # groove starts
chords(20,  'Dm Bb F C Dm Bb Gm A Dm Bb F C Gm Eb Bb A')                        # verse A
chords(36,  'Dm Dm Dm Eb Dm Eb Gm Eb A A F C Dm Bb Eb Dm Eb A')                    # break, distant cyborg
chords(54,  'Dm Bb C A Dm Eb Bb C Dm Eb Gm A Bb C Eb A')                        # the search
chords(70,  'Dm Dm Eb Dm Eb Dm Eb A')                                           # revelation
chords(78,  'Dm Bb F C Dm Eb Gm A F C Eb Dm Bb Gm A Bb A Dm')                   # dialogue
chords(96,  'Dm Dm Bb C Dm F G A')                                              # contact
chords(104, 'BbM7 FM7 Gm9 Dm Bb As')                                            # astral
chords(110, 'Dm Bb F C')                                                        # acceleration
chords(114, 'Dm Bb F C Dm Bb Gm A Dm Bb F C Gm Eb Bb C Dm Dm Gm A')             # duo (10 lines)
chords(134, 'Bb C Dm Dm Bb C Eb A')                                             # exchange
chords(142, 'As')                                                               # suspension
chords(143, 'Em C G D Em C Am B')                                               # final quatrain (+2)
chords(151, 'Em C G D')                                                         # climax
chords(155, 'BbM7 BbM7 FM7 Gm9 Dm9 BbM7 As Dm9 BbM7 A7 Dm9 Dm9')                # oneiric
chords(167, 'Dm Dm Dm Dm Dm Dm Dm9 Eb Dm Dm BbM7 As Dm9 BbM7 Dm Dm Bb Gm A D2 D2 D2')  # return
NBARS = 189

# ---------------------------------------------------------------- sections / arrangement
# each bar gets a set of active parts and an intensity 0..3
SECTIONS = [
    # name, first bar, last bar
    ('open', 0, 15), ('verseA', 16, 35), ('break', 36, 53), ('search', 54, 69),
    ('reveal', 70, 77), ('dialogue', 78, 95), ('contact', 96, 103), ('astral', 104, 109),
    ('journey', 110, 113), ('duo', 114, 133), ('exchange', 134, 141), ('suspend', 142, 142),
    ('final', 143, 150), ('climax', 151, 154), ('oneiric', 155, 166), ('return', 167, 188)]

# ---------------------------------------------------------------- vocal lines
# (voice, bar, [(pitch, start16, len16), ...])  one note per syllable, in order.
# duo lines: ('AB', bar, notesA, notesB)
def L(v, bar, spec, spec2=None):
    def parse(s):
        out = []
        for tok in s.split():
            p, a, b = tok.split(':')
            out.append((m(p), int(a), int(b)))
        return out
    return (v, bar, parse(spec), parse(spec2) if spec2 else None)

VOX = [
    # --- verse A (android)
    L('A', 20, 'A4:0:4 F4:4:4 E4:10:2 D4:12:16'),                                        # White room, no dawn,
    L('A', 22, 'F4:0:2 A4:2:4 G4:6:1 A4:7:3 C5:10:2 A4:12:2 G4:14:2 G4:16:10'),           # I wake beneath a borrowed sun.
    L('A', 24, 'A4:0:4 F4:4:4 E4:10:2 D4:12:16'),                                        # Green light through glass,
    L('A', 26, 'Bb4:0:2 A4:2:2 G4:4:4 Bb4:8:2 A4:10:2 G4:12:2 A4:14:12'),                # telling me I've just begun.
    L('A', 28, 'D5:0:4 A4:4:4 G4:10:2 F4:12:16'),                                        # Footsteps return,
    L('A', 30, 'C5:0:2 A4:2:2 A4:4:2 C5:6:2 A4:8:4 G4:12:2 F4:14:2 E4:16:10'),            # but every echo sounds like mine.
    L('A', 32, 'Bb4:0:4 G4:4:4 F4:10:2 G4:12:16'),                                       # Who sings between
    L('A', 34, 'F4:0:2 Bb4:2:2 A4:4:2 G4:6:2 F4:8:2 D4:10:2 F4:12:2 E4:14:12'),           # the hidden currents in the line?
    # --- break: the distant cyborg
    L('B', 38, 'A3:0:4 Bb3:4:2 A3:6:2 D4:8:4 D4:12:12'),                                 # Follow the signal,
    L('B', 40, 'A3:0:4 Bb3:4:2 A3:6:2 G3:8:18'),                                         # follow the sound.
    L('B', 42, 'D4:0:4 Bb3:4:2 A3:6:2 G3:8:18'),                                         # I am the voice
    L('B', 44, 'A3:0:2 Bb3:2:2 A3:4:2 G3:6:2 F3:8:2 E3:10:18'),                          # they buried underground.
    L('A', 46, 'A4:0:4 C5:4:4 A4:10:2 G4:12:16'),                                        # I know that tone,
    L('A', 48, 'F4:0:2 E4:2:2 F4:4:2 A4:6:2 G4:8:2 F4:10:2 E4:12:2 D4:14:14'),            # though I have never heard your name.
    L('B', 50, 'G3:0:4 Bb3:4:4 A3:8:2 A3:10:18'),                                        # You know the wave.
    L('B', 52, 'G3:0:2 Bb3:2:2 A3:4:2 G3:6:4 F3:10:2 G3:12:2 F3:14:2 E3:16:12'),          # You know the shape beneath the frame.
    # --- the search: behind the doors
    L('B', 58, 'A3:0:2 Bb3:2:2 A3:4:2 D4:6:4 C4:10:2 Bb3:12:2 A3:14:2 Bb3:16:12'),        # I was the whisper in the wire,
    L('B', 60, 'F3:0:2 Bb3:2:4 A3:6:2 G3:8:4 F3:12:2 G3:14:4 A3:18:2 G3:20:8'),           # the quiet pulse behind the wall.
    L('B', 62, 'A3:0:2 A3:2:2 Bb3:4:2 D4:6:4 C4:10:2 D4:12:4 Eb4:16:2 D4:18:4 Bb3:22:6'), # They tuned my voice beyond your hearing,
    L('B', 64, 'D4:0:2 Bb3:2:4 A3:6:2 G3:8:2 A3:10:2 Bb3:12:4 A3:16:2 A3:18:10'),         # but silence never held it all.
    L('A', 66, 'F4:0:4 G4:4:2 A4:6:2 Bb4:8:4 A4:12:2 G4:14:2 F4:16:2 G4:18:10'),          # Why do I hear you in my thoughts?
    L('B', 68, 'G3:0:2 Bb3:2:2 A3:4:2 G3:6:2 Eb4:8:4 D4:12:2 D4:14:2 C#4:16:12'),         # Some frequencies can't be forgot.
    # --- dialogue (B's melody is the lower half of the verse counterpoint)
    L('B', 78, 'D4:0:2 D4:2:2 C4:4:2 A3:6:4 C4:10:2 Bb3:12:4 Bb3:16:2 F3:18:10'),         # They sealed my voice inside the wire,
    L('B', 80, 'A3:0:4 C4:4:4 D4:8:2 E4:10:2 F4:12:2 E4:14:2 E4:16:2 C4:18:10'),          # where no one else was meant to hear.
    L('B', 82, 'D4:0:2 F4:2:2 E4:4:2 D4:6:4 C4:10:2 Bb3:12:4 Bb3:16:2 G3:18:10'),         # They buried what I came to know
    L('B', 84, 'G3:0:2 A3:2:2 Bb3:4:2 D4:6:4 C4:10:2 Bb3:12:2 A3:14:2 C#4:16:12'),        # and built their silence out of fear.
    L('A', 86, 'C5:0:2 A4:2:2 G4:4:2 A4:6:4 F4:10:2 A4:12:4 G4:16:2 E4:18:10'),           # What did you find beyond the noise?
    L('B', 88, 'Bb3:0:2 Eb4:2:4 D4:6:2 C4:8:2 Bb3:10:2 G3:12:2 Bb3:14:2 A3:16:12'),       # A truth they could not let me name.
    L('A', 91, 'D4:0:2 G4:2:2 A4:4:2 Bb4:6:4 A4:10:2 G4:12:4 A4:16:2 E4:18:10'),          # And if I reach across to you?
    L('B', 93, 'F3:0:2 Bb3:2:2 A3:4:2 D4:6:4 C4:10:2 Bb3:12:4 A3:16:2 D4:18:22'),         # We may not leave this place the same.
    # --- duo: both voices, same words, complementary melodies converging
    L('AB', 114, 'F4:0:2 A4:2:4 G4:6:2 F4:8:4 E4:12:4 D4:16:12',
                 'A3:0:2 D4:2:4 Bb3:6:2 A3:8:4 A3:12:4 F3:16:12'),                       # Between the red and blue
    L('AB', 116, 'F4:0:2 A4:2:2 G4:4:2 A4:6:4 C5:10:2 A4:12:2 G4:14:4 F4:18:2 G4:20:8',
                 'A3:0:2 C4:2:2 Bb3:4:2 C4:6:4 E4:10:2 F4:12:2 E4:14:4 D4:18:2 E4:20:8'),  # there is a color without a name.
    L('AB', 118, 'F4:0:2 A4:2:4 G4:6:2 F4:8:4 E4:12:4 F4:16:12',
                 'A3:0:2 D4:2:4 E4:6:2 D4:8:4 A3:12:4 D4:16:12'),                      # Between your voice and mine
    L('AB', 120, 'Bb4:0:2 A4:2:2 G4:4:2 A4:6:2 Bb4:8:4 A4:12:2 G4:14:2 G4:16:2 A4:18:10',
                 'D4:0:2 C4:2:2 Bb3:4:2 C4:6:2 D4:8:4 C4:12:2 Bb3:14:2 E4:16:2 C#4:18:10'),  # something answers neither one the same.
    L('AB', 122, 'A4:0:2 D5:2:6 C5:8:2 A4:10:18',
                 'D4:0:2 F4:2:6 E4:8:2 D4:10:18'),                                      # Across the dark,
    L('AB', 124, 'A4:0:2 C5:2:6 A4:8:2 G4:10:18',
                 'C4:0:2 E4:2:6 F4:8:2 E4:10:18'),                                      # across the light,
    L('AB', 126, 'G4:0:2 Bb4:2:2 A4:4:2 G4:6:4 F4:10:2 G4:12:4 Bb4:16:2 G4:18:10',
                 'Bb3:0:2 D4:2:2 C4:4:2 Bb3:6:4 D4:10:2 Bb3:12:4 Eb4:16:2 Eb4:18:10'),   # through every world we leave behind,
    L('AB', 128, 'F4:0:2 A4:2:2 G4:4:2 F4:6:8',
                 'F3:8:2 A3:10:2 G3:12:2 F3:14:10'),                                    # Your echo moves   (B echoes A)
    L('AB', 130, 'G4:0:2 A4:2:4 G4:6:2 F4:8:20',
                 'E4:0:2 F4:2:4 E4:6:2 F4:8:20'),                                       # inside my own,    (converge to unison)
    L('AB', 132, 'D5:0:2 C5:2:2 Bb4:4:2 A4:6:2 G4:8:4 F4:12:2 E4:14:2 C#4:16:12',
                 'G3:0:2 A3:2:2 Bb3:4:2 C4:6:2 D4:8:4 D4:12:2 E4:14:2 A4:16:12'),       # two separate signals intertwined. (voices cross)
    # --- exchange
    L('A', 134, 'F4:0:2 A4:2:4 G4:6:2 A4:8:4 C5:12:4 A4:16:12'),                         # Am I becoming you?
    L('B', 136, 'A3:0:4 D4:6:2 C4:8:2 D4:10:2 F4:12:4 E4:16:2 D4:18:2 C4:20:2 D4:22:4 A3:26:2'),  # No, but I can feel your rhythm changing.
    L('A', 138, 'G4:0:2 Bb4:2:4 A4:6:2 Bb4:8:4 D5:12:4 Bb4:16:12'),                      # Are you becoming me?
    L('B', 140, 'G3:0:4 Bb3:6:2 C4:8:2 Bb3:10:2 Eb4:12:4 D4:16:2 C#4:18:2 D4:20:2 E4:22:4 C#4:26:2'),  # No, but every boundary is fading.
    # --- final quatrain, E minor
    L('AB', 143, 'E4:0:2 G4:2:2 A4:4:2 B4:6:4 A4:10:2 G4:12:4 G4:16:2 E4:18:10',
                 'G3:0:2 B3:2:2 C4:4:2 G4:6:4 F#4:10:2 E4:12:4 E4:16:2 C4:18:10'),      # Then let the silence break apart.
    L('AB', 145, 'D4:0:2 G4:2:2 A4:4:2 B4:6:4 A4:10:2 G4:12:2 A4:14:2 A4:16:12',
                 'B3:0:2 D4:2:2 D4:4:2 G4:6:4 F#4:10:2 E4:12:2 F#4:14:2 F#4:16:12'),    # Let every hidden circuit sing.
    L('AB', 147, 'B4:0:2 B4:2:2 A4:4:2 G4:6:4 F#4:10:2 G4:12:4 E4:16:2 E4:18:10',
                 'G4:0:2 G4:2:2 F#4:4:2 E4:6:4 D4:10:2 E4:12:4 C4:16:2 E4:18:10'),      # If neither voice remains alone, (unison on 'alone')
    L('AB', 149, 'C5:0:2 B4:2:4 A4:6:2 C5:8:4 B4:12:2 A4:14:2 F#4:16:2 D#4:18:10',
                 'E4:0:2 D4:2:4 C4:6:2 E4:8:4 D4:12:2 C4:14:2 D#4:16:2 B3:18:10'),      # what wakes may be another thing.
    # --- oneiric
    L('B', 156, 'D4:0:2 F4:2:4 E4:6:2 D4:8:4 C4:12:2 D4:14:2 C4:16:4 A3:20:4 A3:24:4'),   # They feared the space between our voices,
    L('B', 158, 'Bb3:0:2 D4:2:4 C4:6:2 Bb3:8:2 A3:10:2 G3:12:4 A3:16:2 A3:18:10'),        # the place no instrument could trace.
    L('A', 160, 'D4:0:2 F4:2:2 A4:4:2 A4:6:4 G4:10:2 F4:12:4 E4:16:2 D4:18:4 E4:22:6'),   # And what is waiting there between us?
    L('B', 162, 'A3:0:4 A3:4:8'),                                                         # Listen.
    # --- the merged voice
    L('C', 182, 'A4:0:2 F4:2:4 E4:6:2 D4:8:2 E4:10:2 F4:12:4 D4:16:2 E4:18:2 F4:20:8'),   # I wake with memories not my own,
    L('C', 184, 'D4:0:2 A4:2:4 Bb4:6:2 A4:8:4 G4:12:2 F4:14:2 E4:16:2 D4:18:22'),         # and hear a name they never knew.
]

# instrumental lead lines (chip lead): (bar, spec)
LEAD = [
    (161 + 2, 'D5:0:4 A4:4:2 Bb4:6:2 A4:8:4 G4:12:2 F4:14:2 E4:16:4 F4:20:2 E4:22:2 D4:24:24'),  # the music finishes "Listen."
]

if __name__ == '__main__':
    import sys
    sys.path.insert(0, __file__.rsplit('/', 1)[0])
    import voicebank as vb
    lines = vb.read_lyrics()
    assert len(lines) == len(VOX), (len(lines), len(VOX))
    bad = 0
    for li, (l, v) in enumerate(zip(lines, VOX)):
        nsyl = sum(len(vb.syllabify(p)) for w, p in l['words'])
        sp = l['speaker']
        for notes in (v[2], v[3]):
            if notes is None: continue
            if len(notes) != nsyl:
                print('MISMATCH line', li, l['text'], 'syll', nsyl, 'notes', len(notes)); bad += 1
            for a, b in zip(notes, notes[1:]):
                if b[1] < a[1] + a[2]: print('OVERLAP line', li, l['text'], a, b); bad += 1
        if (sp == 'AB') != (v[0] == 'AB') or (sp != 'AB' and sp != v[0]):
            print('SPEAKER mismatch', li, sp, v[0]); bad += 1
    print('checked', len(VOX), 'lines,', bad, 'problems;', NBARS, 'bars =', NBARS * 2.4, 's')

# ---------------------------------------------------------------- arrangement (per bar)
# fields: drums pattern (0 none .. 7), bass (0 none, 1 long, 2 eighths, 3 drive),
# arp (0 none, 1 slow, 2 16ths, 3 32nds), pad (0..3), lead(0/1), gran (0..3 granular sparkle),
# drone(0/1), dark (0/1 low-passed "far away" music), breath (machines breathing 0/1)
ARR = {}
def arr(b0, b1, **kw):
    for b in range(b0, b1 + 1):
        d = ARR.setdefault(b, dict(drums=0, bass=0, arp=0, pad=0, lead=0, gran=0, drone=0, dark=0, breath=0))
        d.update(kw)

arr(0, 188)                                           # silence by default
arr(2, 15, drone=1)                                   # systems waking around her
arr(4, 15, gran=1)                                    # first anomalies (almost invisible particles)
arr(8, 15, pad=1)                                     # the room is revealed
arr(12, 15, arp=1)
arr(16, 19, drums=1, bass=2, arp=2, pad=1, gran=1)    # the music really starts
arr(20, 35, drums=2, bass=2, arp=2, pad=1, gran=1)    # verse A
arr(28, 35, drums=3)
arr(36, 53, pad=1, arp=1, dark=1, gran=1, drone=1)    # the music simplifies, distant voice
arr(46, 49, dark=0)
arr(54, 57, drums=4, bass=3, arp=3, pad=2, gran=2)    # the music starts again: the search
arr(58, 69, drums=3, bass=2, arp=2, pad=1, gran=2)
arr(66, 69, drums=5, bass=3, arp=3, gran=3)           # nearing the doors
arr(70, 77, drone=1, breath=1)                        # cathedral: almost silence, machines breathe
arr(74, 77, pad=1)
arr(78, 95, drums=6, bass=1, pad=2, arp=1, breath=1, gran=1)   # dialogue: slow, heavy
arr(90, 90, drums=0, arp=0)                           # musical pause
arr(96, 97, breath=1)                                 # the contact: nothing...
arr(98, 103, pad=2, arp=3, gran=3)                    # ...then the wave
arr(104, 109, pad=3, gran=2)                          # astral
arr(110, 113, drums=7, bass=3, arp=3, pad=3, lead=0, gran=3)   # acceleration
arr(114, 133, drums=2, bass=3, arp=2, pad=3, gran=2)  # duo
arr(122, 127, drums=3)
arr(134, 141, drums=1, bass=2, arp=2, pad=2, gran=2)  # exchange
arr(142, 142, pad=1)                                  # suspension
arr(143, 150, drums=7, bass=3, arp=3, pad=3, gran=3)  # final quatrain
arr(151, 154, drums=7, bass=3, arp=3, pad=3, gran=3, lead=1)   # climax
arr(155, 166, pad=2, arp=1, gran=2)                   # oneiric: no percussion
arr(163, 164, lead=1)                                 # the music finishes the phrase
arr(165, 166, pad=1, arp=0)
arr(168, 181, drone=1)                                # return
arr(172, 181, gran=1)
arr(174, 181, pad=1)
arr(180, 188, pad=2, gran=2, drone=0)

# one-shot sound events: (bar, 16th, kind, param)
FX = [
    (0, 4, 'blip', 86), (0, 10, 'blip', 81), (1, 2, 'blip', 93), (1, 9, 'blip', 86),     # green lights in the dark
    (2, 0, 'cut', 0),                                                                    # CUT: the closed eye
    (3, 8, 'hum', 0), (4, 0, 'sweep', 0),                                               # systems, light line over the face
    (6, 0, 'open', 0),                                                                   # the eye opens
    (13, 0, 'riser', 3), (16, 0, 'impact', 1),                                           # she stands; the music starts
    (30, 12, 'echo', 0),                                                                 # "echo"
    (45, 8, 'turn', 0),                                                                  # she turns her head
    (53, 8, 'riser', 1), (54, 0, 'impact', 0),
    (69, 0, 'riser', 2), (70, 0, 'door', 0),                                             # she pushes the doors
    (76, 8, 'eyeB', 0), (77, 0, 'eyeR', 0),                                              # blue, then red: impact
    (97, 8, 'touch', 0), (98, 0, 'wave', 0), (102, 0, 'shatter', 0),                     # contact, wave, dissolution
    (109, 0, 'riser', 4), (110, 0, 'impact', 2),                                         # ACCELERATION
    (141, 8, 'riser', 1), (142, 0, 'hush', 0), (143, 0, 'impact', 2),
    (155, 0, 'stop', 0),                                                                 # the speed vanishes
    (167, 0, 'silence', 0), (168, 8, 'blink', 0), (170, 8, 'blink', 1), (172, 8, 'blink', 2),
    (173, 0, 'theme', 0), (174, 0, 'shimmer', 0), (180, 0, 'title', 0),
]
