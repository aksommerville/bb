# Beepbot

A collection of synthesizers intended for use in video games.

## BBA vs BBB vs BBC

- BBC is not recommended because it doesn't exist yet.
- BBA for Arduino. Maybe other cases, if you need the absolute minimum memory usage.
- BBB high performance, moderate quality, potentially high memory usage (plan on 20..50 MB).

## TODO

-[x] bba: Limited low-memory synth eg for Arduino.
- -[x] Play songs.
- -[x] Convert from MIDI. cli:mid2bba
- -[x] Test on hardware.
- -[ ] Song tooling, eg tweak instruments.
- -[ ] Helper for streaming MIDI events.
-[x] bbb: PCM only at the output end, synthesize notes individually.
- -[x] Basic tuned instruments.
- -[x] Opinionated drums.
- -[x] Opinionated sound effects.
- -[x] Memory cache limit.
- -[x] Disk cache.
- -[ ] Disk cache is currently unconstrained. Should we allow a configurable size limit? (How would that work?)
- -[x] Live PCM limit.
- -[ ] Tempo tracking.
- -[ ] Default instruments.
-[ ] bbc: The big kahuna.
-[x] Accept MIDI-In at demo, necessary for further testing.
-[ ] Drivers.
- -[x] Pulse.
- -[ ] ALSA.
- -[ ] MacOS.
- -[ ] Windows.
- -[ ] MIDI-In drivers.
-[ ] Wrapper library.
-[ ] CLI.
- -[x] mid2bba
- -[ ] play (file or stream)
-[ ] Github repo
