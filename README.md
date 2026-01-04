# Music-player
Still WIP.
For use in Arduino IDE esp32 version must be 2.0.14, to enable the use of TFT e_SPI

This repository contain the source code for a full function Music Player.

This MP should be use with any MCU with Dual Core, Freertos, >= 2MB PSRAM and atleast have 23 nor more GPIO pins, if planing to use this whole code as it, otherwise pins amount dont matter. Currently deveploping it on a ESP32-S3

Functions:
Reverse playback.
Realtime EQ.
Waveform visualizer.
UI(mostly done).
Display cover art.
Song selection.
Auto Update every bootup or manual
(adding more as this going)

Planing to add:
Bluetooth.
Left/Right Balance.
Album.
Playlists.
(adding more as this going)

Limitation:
Audio only use .wav file, this is mainly for the reverse playback.
Art Cover only allow .bmp file, its just easier.

here are a few demo of the cuurent work(sort by time, first is the latest)
https://www.reddit.com/r/arduino/comments/1pcjqaz/real_time_eq_and_adjusting_change/
https://www.reddit.com/r/arduino/comments/1p18srr/music_player_half_way_progress_demo/
https://www.reddit.com/r/arduino/comments/1ok20k8/on_the_fly_reverse_playback/


