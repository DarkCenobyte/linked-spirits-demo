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
* `linked_spirits_upx.exe` (or the unpacked `linked_spirits.exe`): fullscreen at
  the desktop resolution, 2.35:1 letterbox.
* **Escape quits immediately**, during the loading line and during the film.
* Loading takes around ten seconds: worker threads render the whole soundtrack
  (music + voices) into memory, and a short benchmark of the heaviest shots
  picks the internal resolution. Slower GPUs render fewer pixels (down to
  half resolution) instead of dropping frames.
* With no audio device the film still runs, timed on the system clock.

## Size

| file | bytes |
|---|---|
| `linked_spirits.exe` (raw, CRT-free) | 161 280 |
| `linked_spirits_upx.exe` (`upx --best --ultra-brute --lzma`) | **71 168** (69.5 KiB) |

This is above the 64 KiB ideal and well inside the 256 KiB limit. Artistic
quality came first. Most of the bytes above 64 KiB went into the cyborg
(her body, the machine and the cable dome of the cathedral), the dissolution
of the world and the one-piece armour of light of the astral bodies.

Where the packed bytes go (LZMA estimates):

| part | raw | compressed |
|---|---|---|
| x86-64 code (director, synth, voice, platform) | 59.1 KB | ≈ 28.3 KB |
| singing-voice bank (54 sung lines, 3 voices) | 49.3 KB | ≈ 20 KB |
| GLSL (minified, 8 sources, 7 programs) | 40.0 KB | ≈ 14.5 KB |
| score, tables, GL names, constants | ≈ 6 KB | ≈ 4 KB |

## Storyboard

One bar = 2.4 s (100 BPM in 4/4); 189 bars.

| time | scene |
|---|---|
| 0:00 | Darkness. Green lights come on one by one. Extreme close-up of a closed eye; a scan line crosses the face; the eye opens (reactive pupil, micro-saccades). A long pull-back reveals the white laboratory and the cradle. |
| 0:38 | **Song 1 (android, voice A)**: she rises and walks through the lab and along the glass partition. Racks of green lights, the halo above the pod. |
| 1:26 | She stops, tiny in a long white corridor. **A distant voice (cyborg, voice B)** calls her, *Follow the signal*. Each time it sings, violet light leaks through the gap of the doors at the far end. |
| 1:50 | She answers (*I know that tone*); the dialogue at a distance. |
| 2:10 | **The search**: the corridor gets longer, twists and grows vast and strange. *Why do I hear you in my thoughts?* |
| 2:48 | She pushes the doors; white light floods in. |
| 2:53 | **The cathedral**: a nave of vertebral ribs and cables. At its end, a gigantic dome of woven cables with a face at its apex. Extreme close-up: the left eye opens **blue**, then the right one **red**, on an impact. |
| 3:07 | **The sung dialogue**. The cyborg's worn face is set into the mass of cables. Below it hangs a body of worn plates over exposed muscle, arms ending in torn wires, hung from the vault and sunk into a machine. |
| 3:50 | The android reaches up and takes the torn stump of her arm. A blue point of light appears under her finger, then a red one; a pulse runs up her arm, and **the contact wave**, a red/blue interference front, spreads through the cathedral. Then **the world dissolves**: every visible surface (the cables, the cyborg, the android) turns into points of light in its own colours, released by a front sweeping out from the touch, rising like embers into the dark. |
| 4:10 | **Astral bodies**: a blue body of particles (android) with a green gorget and pauldrons; a taller red body (cyborg) with vivid blue ones. Her hand on the android's shoulder. |
| 4:24 | **The journey**: acceleration, streaks of stars; an Earth-like world, a desert world, a gas giant whose rings they fly through (an ocean of fragments), an ocean world. |
| 4:34 | **Duo** (voices A and B together). |
| 5:22 | *Am I becoming you?* Push into her iris: the iris becomes a planetary system. The pupil ignites into a star, the fibres settle into dust lanes, worlds appear on their orbits. |
| 5:43 | The final quatrain, circling them with the worlds behind. |
| 6:02 | **Climax**: the two bodies interpenetrate, and a third, ambiguous white silhouette appears between them. |
| 6:12 | **The dream**: above an ocean planet, under a giant star. |
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
* **Particles**: a vertex shader turns `gl_VertexID` into motes of light,
  bodies of light, star fields, ring fragments and structures of light.
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

### Characters: `src/shaders/char.glsl`, `src/director.c`

* **The android**: a helmet-like cranium and an oval face mask tapering to a V
  jaw, with no nose, thin lips and shallow cheek hollows. The neck is slimmer
  than the head and set under the skull; a composite body has visible metal
  joints.
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
phoneme boundaries). It is then analysed into 16th-order LPC → line spectral
frequencies. Variable-frame-rate keyframes are least-squares fitted and
projected on 16 PCA components with uniform quantisation, plus a 20 ms energy
track. That is 49 KB for the whole libretto.

At run time the voice is re-synthesised:

* **Excitation**: band-limited pulses (BLIT) plus noise, mixed MELP-style.
* **Filter**: LSF → LPC direct form, in double precision.
* **Timing**: each syllable is time-warped onto the notes of the score, with
  vowel morphing towards canonical targets and emphasised consonants.
* **Voice character**: formants are scaled through the internal sample rate,
  per voice.

The music never masks the words. The voices duck the instruments, a
dynamic EQ carves room for them, and a presence boost is applied. The
reverb is kept low on the voices.

**Intelligibility check**: Whisper via sherpa-onnx, run on the soundtrack
rendered by the final build (music and voices together). Word error rate
of the transcript against the lyrics, over all 54 sung lines:

| test | WER |
|---|---|
| full mix, lines in pairs, Whisper `small.en` | **13.0%** |
| full mix, each line alone, `small.en` | 19.8% |
| full mix, lines in pairs, `base.en` | 33.4% |

Most remaining errors are near-homophones ("*I knew that tune*" for "*I know
that tone*"). The final couplet, sung by the merged voice over thinned-out
pads, is transcribed with a single error: "*I wake with memories not my own,
and hear the name they never knew*".

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
./build.sh debug      # build/linked_spirits_debug.exe, logs to debug.log
./build.sh data       # regenerate src/gen_score.h and src/gen_voice.h
```

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
| `src/gen_*.h` | generated data: shaders, score, voice bank |
| `tools/` | score, voice bank builder, minifier, Whisper tests |
