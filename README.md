# LINKED//SPIRITS

*By: Claude Opus 5.5 // Prompts by: DarkCenobyte*

A 7'34" audiovisual demo for Windows x64: one executable, OpenGL 4.6, no
files, no external assets. Every image is raymarched or generated from vertex
IDs. The soundtrack is synthesised while the thin green line of the loading
screen crosses the black: an original modern-epic chiptune score and three
synthetic female voices that sing fully intelligible lyrics.

An android wakes up in a clinical white laboratory. A voice she has never heard
calls her through the walls. Behind the last doors, in a cathedral of cables, a
cyborg is held prisoner by the machine. When their hands meet, the world comes
apart into light, and two bodies made of particles cross planets, rings and an
iris that becomes a planetary system. Then comes the return to the white room,
the same framing, and three blinks. The person who opens her eyes is not quite
the same.

## Running it

* Windows 10/11 x64, a GPU with OpenGL 4.6 (a recent discrete GPU is
  recommended; the scenes are heavy raymarching).
* Prebuilt binaries are in [`release/`](release/), all the same film:
  * `linked_spirits_64k.exe`: **the 64k edition**, 65 536 bytes (32-bit,
    kkrunchy, a leaner voice bank);
  * `linked_spirits32_kkrunchy.exe`: the smallest with the full voice bank
    (32-bit, kkrunchy);
  * `linked_spirits_squishy.exe`: the smallest 64-bit (squishy);
  * `linked_spirits_upx.exe`: 64-bit, UPX;
  * `linked_spirits.exe`: 64-bit, unpacked.

  They run fullscreen at the desktop resolution, 2.35:1 letterbox. Packed
  executables (kkrunchy and squishy especially) are sometimes flagged by
  antivirus heuristics; the unpacked one never needs unpacking.
* **Escape quits immediately**, during the loading line and during the film.
* Loading takes around ten seconds: worker threads render the whole soundtrack
  (music + voices) into memory, and a short benchmark of the heaviest shots
  picks the internal resolution. Slower GPUs render fewer pixels (down to
  half resolution) instead of dropping frames; fast ones render more
  (1.25× or 1.5× per axis) and the film is supersampled.
* With no audio device the film still runs, timed on the system clock.

## Size

| file | packer | bytes |
|---|---|---|
| `linked_spirits_64k.exe` | kkrunchy_k7 0.23a4 `--best` (32-bit), 64k voice bank | **65 536** (64 KiB) |
| `linked_spirits32_kkrunchy.exe` | kkrunchy_k7 0.23a4 `--best` (32-bit) | **75 776** (74 KiB) |
| `linked_spirits_squishy.exe` | squishy 0.2.0 (64-bit) | **79 872** (78 KiB) |
| `linked_spirits_upx.exe` | UPX 4.2.2 `--best --ultra-brute --lzma` (64-bit) | 98 816 |
| `linked_spirits.exe` | none (64-bit, CRT-free) | 226 304 |

Every packer that was tried, on the same program (compatible pairs only: each
packer takes the plain executable or, for Crinkler, the object file):

| target | packer | bytes | notes |
|---|---|---|---|
| 32-bit | kkrunchy_k7 `--best` | **75 776** | runs; unpacks in about a second |
| 32-bit | Crinkler 2.3 `/COMPMODE:SLOW` | 79 840 | compressing linker made for 4k-8k; even a 495-byte Crinkler test does not start under Wine, so it could not be verified; Crinkler 3.0b refuses a code part above 64 KB |
| 64-bit | squishy 0.2.0 | **79 872** | runs; x86-64 only since 0.2.0 |
| 32-bit | UPX `--ultra-brute` | 93 184 | |
| 64-bit | UPX `--ultra-brute` | 98 816 | `--best --lzma` alone: 99 840 |

The compiler settings matter as much as the packer, and differently for each:
x87 floating point instead of SSE makes denser code (-1 KB with UPX, -0.6 KB
with squishy, -1 KB with kkrunchy), and plain i386 code instead of Nehalem
saves another 1 KB with kkrunchy. The x87 soundtrack was checked against the
SSE one: same loudness to 0.06 dB on average, and the sung lyrics come back as
well (word error rate 6.5% vs 7.2%). 32-bit code is denser than x86-64 (no REX
prefixes, 4-byte pointers), which is why the smallest build is 32-bit.
Fatpack was not tried: it is an LZMA packer for x64 with a larger loader,
built for compatibility (TLS, Rust or Delphi executables) rather than size, so
it could at best match UPX. Importing DLL functions by hash is already what
kkrunchy, squishy and Crinkler do; with UPX it would save a few hundred bytes at most (the OpenGL functions
have to be looked up by name anyway).

Artistic quality came first. At 77.5 KiB, going under 64 KiB would have
meant cutting the voice bank and with it the intelligibility of the singing,
so 128 KiB was taken as the ceiling instead, and the extra room was spent
where it shows most:

* **the voices** (+20 KB): finer spectral keyframes and quantisation, a 10 ms
  energy track, and a source speaker of her own for each character. The word
  error rate of the sung lyrics fell from 14.7% to the figures below;
* **the astral world**: a galaxy of a quarter of a million points, discs of
  light, a sky with a galactic band, nebulae and diffraction spikes, planets
  with sunsets and rings of fire;
* **the lyrics on screen**, an anamorphic streak on the brightest lights, a
  furnished medical corridor, the twisting corridor behind her, and the run on
  a planetary ring.

### The 64k edition

Inside the kkrunchy build (73.6 KB of packed data), the voice bank weighs
37.1 KB, the shaders 14.6 KB, the lyrics 0.7 KB and the code with the rest of
the data 21.2 KB. The code and shaders were already minified to the point
where further tricks (macros for GLSL built-ins, other compiler switches) gain
a few dozen bytes, and a dedicated arithmetic coder for the voice coefficients
would not beat kkrunchy's own context mixing (they are close to random at this
precision). So the 64k edition changes only how the voice bank spends its
bits, measured by rate and distortion rather than guessed:

* fewer spectral keyframes where the voice is quiet (breaths, closures, the
  tails of notes, which the music also masks): the keyframe threshold grows
  with 1.3 × (1 - √(energy / peak energy));
* loudness in 1.75 dB steps instead of 1.5 dB (still every 10 ms).

Everything else is identical: both speakers, every word, every frame of the
film, the same score and code. The bank shrinks from 39.6 to 28.3 KB (LZMA) and
its energy-weighted spectral error grows from 1.26 to 1.63 dB, mostly in the
quiet frames. The sung lyrics stay about as intelligible: Whisper word error
rates of 7.5-9.2% across the candidates tried, against 7.2% for the full bank,
differences that are within the test's noise. The setting kept is the one
closest to the full bank that still fits in 65 536 bytes.

Before that, the bytes above 64 KiB had gone into the cyborg (her body, the
machine and the cable dome of the cathedral) and the dissolution of the
world.

Lossless savings (the image and the sound are unchanged):

* the voice bank stores deltas only for its first three PCA components, which
  move smoothly; the others are noise-like and pack better as plain values
  (≈ 1 KB; the rendered soundtrack is bit-identical);
* the release shaders leave out the look-development test mode;
* the exe has a fixed base and no relocation table;
* x87 floating point instead of SSE (see above).

Where the packed bytes go (LZMA estimates):

| part | raw | compressed |
|---|---|---|
| x86-64 code (director, synth, voice, platform) | 63 KB | ≈ 31 KB |
| singing-voice bank (54 sung lines, 2 speakers) + lyrics text | 101 KB | ≈ 43 KB |
| GLSL (minified, 8 sources, 7 programs) | 50.9 KB | ≈ 18 KB |
| score, tables, GL names, constants | ≈ 6 KB | ≈ 4 KB |

## Storyboard

One bar = 2.4 s (100 BPM in 4/4); 189 bars.

| time | scene |
|---|---|
| 0:00 | Straight after loading: an extreme close-up of her closed eye in the dark, while her systems wake one by one. A scan line crosses the face; the eye opens (reactive pupil, micro-saccades). A long pull-back reveals the white laboratory and the cradle. |
| 0:38 | **Song 1 (android, voice A)**: she rises and walks through the lab and along the glass partition. Racks of green lights, the halo above the pod. |
| 1:26 | She stops in a clean, aseptic medical corridor (a waiting bench, a stretcher and its IV pole, a cart of drawers, a monitor tracing a pulse, a red extinguisher), before a sealed access. **A distant voice (cyborg, voice B)** calls her, *Follow the signal*. It sounds far away, down a corridor, and each time it sings, violet light leaks through the seams of the access. |
| 1:50 | She answers (*I know that tone*); the dialogue at a distance. |
| 2:10 | **The search**: the access slides open on the impact. Beyond it, a ribbed passage lined with cables grows vast and strange. She comes towards us, and far behind her the long corridor twists about its axis, while she stays untouched. *Why do I hear you in my thoughts?* |
| 2:48 | She pushes the doors; white light floods in. |
| 2:53 | **The cathedral**: a nave of vertebral ribs and cables. At its end, a gigantic dome of woven cables with a face at its apex. Extreme close-up: the left eye opens **blue**, then the right one **red**, on an impact. |
| 3:07 | **The sung dialogue**. The cyborg's worn face is set into the mass of cables. Below it hangs a body of worn plates over exposed muscle, arms ending in torn wires, hung from the vault and sunk into a machine. |
| 3:50 | The android reaches up and takes the torn stump of her arm. A blue point of light appears under her finger, then a red one; a pulse runs up her arm, and **the contact wave**, a red/blue interference front, spreads through the cathedral. Then **the world dissolves**: every visible surface (the cables, the cyborg, the android) turns into points of light in its own colours, released by a front sweeping out from the touch, rising like embers into the dark. |
| 4:10 | **Astral bodies**: the screen stays black, then the astral world fades in: a blue body of particles (android) with a green gorget and pauldrons; a taller red body (cyborg) with vivid blue ones. Her right hand on the android's shoulder. Below them, a spiral galaxy of a quarter of a million points of light, leaning towards the viewer; above, the band of another galaxy with its dust lanes, veils of nebulae and bright stars with diffraction spikes. |
| 4:24 | **The journey**: they face the flight and lean into it, arms swept back, legs trailing, each body shedding sparks of its own colour behind it. Acceleration: the universe streams towards them, streaks of stars, the edges of the frame pull everything out of it. An Earth-like world with a sunset line and a dreaming night side, a desert world, then a gas giant: its ring comes up under their feet, they land and run on its ice and rock in long strides, arms driving, the giant looming beside them, and leap back into flight. An ocean world. Whoever sings reaches out towards the other. |
| 4:34 | **Duo** (voices A and B together). |
| 5:22 | *Am I becoming you?* Push into her iris: the iris becomes a planetary system. The pupil ignites into a star, the fibres settle into dust lanes, worlds appear on their orbits. |
| 5:43 | The final quatrain, hand in hand, above a disc where worlds are born: dust of light to the horizon, with gaps opened by young worlds and a young star at its centre that backlights everything (a planet eclipsing into a ring of fire). |
| 6:02 | **Climax**: arms open, heads thrown back, the two bodies interpenetrate, and a third, ambiguous white silhouette appears between them. |
| 6:12 | **The dream**: face to face, turning slowly, among drifting motes of light, above an ocean planet, under a giant star. |
| 6:41 | **The return**: the same white room, the same framing as the opening. Three blinks; on the last one, the eyes open in **heterochromia** (left blue, right red) and glow. The title *LINKED//SPIRITS* (7:12). **Voice C**, the merged voice, sings the last two lines: *I wake with memories not my own, and hear a name they never knew.* |

## How it is made

### Images: `src/shaders/`

* **One fragment shader per world** (lab, cathedral, space) compiled from the
  same source with `#define S_LAB/S_CATH/S_SPACE`. The scene pass is sphere
  tracing into an MRT target (RGBA16F colour + R32F depth) at an internal
  1920×816 (2.35:1) or lower.
* **Shading**: soft shadows (restricted to the characters where large
  occluders would only add noise), ambient occlusion, Blinn-Phong specular with
  Fresnel, environment reflections, a floor reflection pass, a glass partition, and
  emissive systems (screens, ceiling strips, the voice-driven light).
* **Particles**: a vertex shader turns `gl_VertexID` (hashed with an integer
  hash) into motes of light, bodies of light, star fields, ring fragments,
  braided wakes, and one great disc: a two-armed spiral galaxy (arms, old disc,
  bulge, pink knots), a disc of newborn worlds, or a planetary ring lit and
  shadowed by its planet. Worlds hide the particles behind them.
* **The dissolution of the world** is built from a snapshot:
  * the frame at the chosen moment is rendered once more (the director is a
    pure function of time) into a colour + ray-distance target;
  * a quarter of a million particles rebuild every visible surface from it,
    each carrying the energy of the pixels it replaces;
  * a front from the touch releases them, and each surface of the live render
    lets go on the same clock.
  Streaks are a second `GL_LINES` pass. Sprites are depth-tested against the
  scene depth and get bokeh discs from the circle of confusion.
* **Post**: a 6-level bloom pyramid, a 40-tap gather depth of field weighted by
  each sample's circle of confusion, subtle chromatic fringe, ACES, a cool
  "material world" grade vs an astral grade, vignette, grain, and the title.
* **Anti-aliasing**: supersampling on fast GPUs; on every GPU an FXAA-style
  pass smooths edges along their own direction, and fine procedural patterns
  (wall seams, floor tiles) are filtered by the pixel's footprint so they fade
  into their average at a distance instead of shimmering. (There are no
  textures seen at grazing angles, so anisotropic filtering would not change
  anything.)
* **Lyrics**: every sung line is drawn once at start-up into a text atlas (GDI
  on Windows). The post pass outlines the glyphs in the colour of the singer
  (green: the android, red: the cyborg, light grey: the duo and the merged
  voice after the awakening), lets each line form from left to right behind a
  glowing edge, and dissolves it grain by grain as it rises to make room for
  the next one.

### Characters: `src/shaders/char.glsl`, `src/director.c`

* **The android**: a helmet-like cranium and an oval face mask tapering to a V
  jaw, with no nose, thin lips and shallow cheek hollows. The neck is slimmer
  than the head and set under the skull; a composite body has visible metal
  joints, hands whose palms rest towards the thighs, and sculpted feet without
  toes (heel, high instep, ball, rounded toe box, inner arch, flat sole).
  * **Real jaw**: the lower face rotates about a hinge under the ears. The
    angular gap between the lips is folded onto the lip line and carved out as
    the mouth, over teeth plates and a dark cavity; the cheeks stretch
    smoothly.
  * **Lip-sync**: mouth opening and rounding come from the synthesised voice
    itself (an F1 estimate per 10 ms).
* **Eyes**: real eyeballs with a refracting cornea (IOR 1.376) and a procedural
  iris: fibres, collarette, crypts, furrows and a hidden nine-bladed
  diaphragm. The pupil reacts to light; micro-saccades and blinks drive
  almond-shaped eyelid shells.
* **The cyborg**: the same head size, older and worn (patina, scratches,
  dried streaks, light in the seams), with heterochromia (left blue, right
  red). Her face emerges from a dome of cables woven over and under each
  other. Below it:
  * worn plates over exposed dark-red muscle, a sternum column with ribs, and
    a ringed throat;
  * an open waist sunk into a dark machine, with clamps holding her arm
    stumps and torn wires spilling from them;
  * taut cables hanging her from the vault.
* **Animation**: CPU forward kinematics (20 joints), two-bone arm IK, a walk
  cycle along a path, head/gaze/blink/pupil animation.

### Voices: `tools/voicebank.py`, `src/voice.c`

Three synthetic female voices sing 54 lines of lyrics:

* **Voice A**, the android: clear, intimate, airy.
* **Voice B**, the cyborg: lower and ampler; the room takes part.
* **Voice C**, the two merged: a new quality.

At build time, each line is spoken by a neural TTS (Kokoro, via sherpa-onnx)
and aligned by DTW to an MBROLA rendering of the same phonemes (which gives
phoneme boundaries). The two characters have two different source speakers:
the android's words come from one voice (`bf_isabella`), the cyborg's (her
lines and her part of every duet) from a darker one (`bf_emma`), so they
differ in their very timbre, not only by a formant shift, and still blend in
the duets. The android's speaker was chosen among six Kokoro voices by the
intelligibility check below; her upper formants also sit lower than those of
the previous voice, which sounded too shrill. Each rendering is analysed
into 16th-order LPC → line spectral frequencies. Variable-frame-rate
keyframes are least-squares fitted and projected on 16 PCA components with
uniform quantisation, plus a 10 ms energy track. That is 95 KB (≈ 41 KB
packed) for the whole libretto, both speakers.

At run time the voice is re-synthesised:

* **Excitation**: band-limited pulses (BLIT) plus noise, mixed MELP-style.
* **Filter**: LSF → LPC direct form, in double precision.
* **Timing**: each syllable is time-warped onto the notes of the score, with
  vowel morphing towards canonical targets and emphasised consonants.
* **Voice character**: formants are scaled through the internal sample rate,
  per voice.
* **Singing, not speaking**:
  * notes join through smooth glides that anticipate the next note, and are
    approached from slightly below after a rest;
  * the vibrato starts late and never quite repeats, and a slow pitch drift
    plus tiny jitter and shimmer keep the voice alive;
  * the loudness follows the sung phrase rather than spoken stress;
  * on the highest notes the first formant follows the pitch, as a soprano
    opens her jaw, and the resonances widen slightly so they never whistle.
* **The distant cyborg** sounds down a corridor: a darker, quieter direct
  voice, flutter echoes and much more room.

The music never masks the words. The voices duck the instruments, a
dynamic EQ carves room for them, and a presence boost is applied. The
reverb is kept low on the voices, except for the distant cyborg.

**Intelligibility check**: Whisper via sherpa-onnx, run on the soundtrack
rendered by the final build (music and voices together). Word error rate
of the transcript against the lyrics, over all 54 sung lines:

| test | one speaker, coarser bank | + cyborg `bf_emma` | + android `bf_isabella` (now) |
|---|---|---|---|
| full mix, lines in pairs, Whisper `small.en` | 14.7% | 9.6% | **7.2%** |
| full mix, each line alone, `small.en` | 15.7% | 13.3% | 12.3% |
| full mix, lines in pairs, `base.en` | 20.1% | 15.4% | 17.1% |

Most remaining errors are near-homophones or single phonemes: "*I know that
tune*" for "*I know that tone*", "*the whisper in the wild*" for "*…in the
wire*", "*and keep the name they never knew*" for "*and hear a name…*". In
her solo lines the cyborg's "*voice*", which the single-speaker bank turned
into "*force*", is now heard right; in one duet ("*Between your voice and
mine*") it still becomes "*the two forces*". The smaller `base.en` model does
slightly worse with the new android voice than with the previous one.

### Music: `tools/score.py`, `src/synth.c`

An original score (D minor, 100 BPM) in 16 sections, written as data:
* chords per bar;
* per-bar arrangement flags (drums, bass, arpeggio, pads, lead, granular
  shimmer, drone, dark bed, breath);
* a lead line;
* the vocal lines, one note per syllable;
* sync events.

The synth renders each track into float buffers on worker threads:
* chip voices (band-limited pulses, a 4-bit triangle, noise) beside modern
  ones (ten detuned saws through state-variable filters, tuned granular
  particles, drones);
* FDN reverb, ping-pong delay, sidechain and a limiter.

Sync points (the blue/red eye impact, the doors, the contact wave, the
acceleration, the climax, the three blinks) come from the same timeline
that drives the director.

### Size techniques

* No C runtime: `-nostdlib`, custom entry point, x87 inline-assembly math
  (`src/mathx.h`), `-Oz`, `--gc-sections`, no unwind tables.
* All GL functions loaded by name from one packed string table; uniforms are
  a single `vec4 U[64]` at an explicit location, shared by every program.
* GLSL minifier (`tools/minify_glsl.py`): strips comments and whitespace,
  shortens float literals and renames identifiers by frequency.
* Voice bank stored as planar streams (phoneme ids, durations, gaps, energy,
  PCA coefficients) so that LZMA sees homogeneous data.
* UPX `--ultra-brute --lzma`.

## Building

Requirements: `x86_64-w64-mingw32-gcc` (GCC 13), `upx`, `python3`.

```sh
./build.sh            # build/linked_spirits.exe and build/linked_spirits_upx.exe
tools/pack.sh         # build/linked_spirits_squishy.exe, build/linked_spirits32_kkrunchy.exe, build/linked_spirits_64k.exe
./build.sh debug      # build/linked_spirits_debug.exe, logs to debug.log
./build.sh data       # regenerate src/gen_score.h and src/gen_voice.h
```

`tools/pack.sh` also needs `i686-w64-mingw32-gcc`, Wine with 32-bit support,
squishy (`SQUISHY=.../squishy-x64.exe`, from logicoma) and kkrunchy
(`KKRUNCHY=.../kkrunchy_k7.exe`). kkrunchy has no binary release; it was
written for Visual Studio, and `tools/kkrunchy_gcc.py <fr_public/kkrunchy_k7>
<dir>` builds it with MinGW-w64 instead (replacing its MSVC inline assembly and
its PDB reader; it needs NASM 2.10, as the RDF output of later versions is
broken).

`./build.sh data` needs the voice toolchain: `sherpa-onnx` (Python) with the
Kokoro `kokoro-en-v0_19` model (`KOKORO_DIR`), `espeak-ng`, and `mbrola` with
the `us1` voice. The generated headers are committed, so a normal build needs
none of it.

### Linux preview harness

`./build.sh preview` builds `build/preview` (EGL, Mesa; works on llvmpipe):

```sh
build/preview frames T0 T1 STEP OUTDIR [SCALE]   # PPM frames of the film
build/preview audio out.wav [stems|vox]          # the soundtrack
U7="0.6 0 0 0" build/preview frames 50 50 1 out  # override a uniform (look-dev)
CAMH="0 0 .8 30" build/preview frames 50 50 1 out   # camera relative to her head
```

## Repository layout

| path | contents |
|---|---|
| `src/main_win.c` | Windows platform: window, GL 4.6 core context, loading line, benchmark, audio, main loop |
| `src/main_lin.c` | Linux/EGL preview harness |
| `src/demo.c` | render passes and targets |
| `src/director.c` | the film: shots, cameras, poses, animation, uniform layout |
| `src/synth.c`, `src/voice.c` | music synthesis and singing-voice synthesis |
| `src/shaders/*.glsl` | shaders (embedded by `tools/embed_shaders.py`) |
| `src/gen_score.h`, `src/gen_voice.h`, `src/gen_voice_64k.h` | generated data: score, voice bank, the 64k edition's voice bank (`src/gen_shaders.h` is written by every `build.sh` run) |
| `tools/` | score, voice bank builder, minifier, Whisper tests, packing (`pack.sh`, `kkrunchy_gcc.py`) |
