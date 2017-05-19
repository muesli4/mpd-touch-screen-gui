# Usage

This the frontend GUI application for my DIY HiFi system and digital audio streaming solution (based on Raspberry Pi 2B and HifiBerry AMP+) showing on an IL9341 LCD including a touch screen (with 320x240 resolution). It allows controlling a running MPD instance. (See MPD environment variables if you don't intend to run it on the same machine.)

As the display lacks any hardware acceleration and is run from the framebuffer, the draw operations are largely reduced. I.e. there is no periodic rendering. The screen is updated when necessary.

## How to adapt to your needs

You will have to change the cover path constants. Most of the other parts are system agnostic. You may disable the dimming functionality (by removing macro definitions) and change other constants to your liking.

# Features

* Global button controls on the left side (most importantly to switch the view).
* Cover view that displays a cover or a replacement with song information. Swiping gestures allow control over the playback and volume (left, right, up, down and press).
* A playlist view allows viewing and changing the song from the current playlist.
* In the search view a on-screen keyboard is presented that allows to search for songs case-insensitive. The results may be browsed and played in the same manner as with the playlist view.
* A final view allows shutting down and rebooting the machine.

# Requirements

* SDL2_ttf
* SDL2_image
* sdl2
* libmpdclient
* icu-uc

# State

Still experimental but pretty functional overall. I used a quick and dirty implementation of an IMGUI library (mostly for rapid development and learning purposes).

# Contact

If you find my code useful please let me know. I'm also interested in your use case, any suggestions, improvements or criticism. Cheers!
