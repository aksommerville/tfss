# tfss: Trivial Finite-State Synthesizer

2026-01-17: I want a synthesizer that can plug into Raylib, just to understand how that hook-up works.
Won't be anything fancy.
And it's a good opportunity to experiment with a most bare-bones strategy.
So, a pure C synthesizer with no dependencies, not even stdlib.

## Design Constraints

- Run at any sane rate. 20..200k sounds good.
- Emit multiple channels but only 1 or 2 are meaningful. Per-channel pan.
- No stdlib.
- - This means we'll borrow buffers provided by the caller.
- - Will need to generate sine and bend tables. See egg2.
- MIDI file and stream decoding built-in.
- - One song at a time.
- - Strict track limit for files.
- - Also accept plain MIDI events in real time, possibly conflicting with the song.
- Do include raw PCM channels since it's so easy to.
- - Caller owns the buffer, and caller is responsible for matching our sample rate.
- No post-processing. Voices can be written directly onto the main output. At signal cadence, there's no channel, just a list of voices.

2026-01-17: It works, at least I can build it with Raylib for web and the sound seems ok, and performance is not as bad as I expected.
Two unresolved questions:
 - Is it possible to ask Raylib for the driver's main rate? I hate forcing it to resample but that's what we're doing now.
 - Is there a way to lock the driver, and do we need to? This won't be a problem in web, since it's strictly single-threaded, but I expect native is using an I/O thread behind the scenes.
 
tfss itself is not even close to finished. Just got it playing songs with a trivial square wave.
I'm waiting to see whether Alex has any appetite for using this, before proceeding.
