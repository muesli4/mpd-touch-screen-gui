# Usage

This the frontend GUI application for my DIY HiFi system and digital audio streaming solution (based on Raspberry Pi 2B and HifiBerry AMP+) showing on an IL9341 LCD including a touch screen (with 320x240 resolution). It allows controlling a running MPD instance. (See MPD environment variables if you don't intend to run it on the same machine.)

As the display lacks any hardware acceleration and is run from the framebuffer, the draw operations are largely reduced. I.e. there is no periodic rendering. The screen is updated when necessary.

## Configuration

The repository ships with a default configuration file (place it in your config folder, usually `$HOME/.config`, or with the executable). All of the settings are pretty self-explanatory.

# Features

* Global button controls on the left side (most importantly to switch the view).
* Cover view that displays a cover or a replacement with song information. Swiping gestures allow control over the playback and volume (left, right, up, down and press).
* A playlist view allows viewing and changing the song from the current playlist.
* In the search view a on-screen keyboard is presented that allows to search for songs case-insensitive. The results may be browsed and played in the same manner as with the playlist view.
* A final view allows shutting down and rebooting the machine.

## Example images

![cover swipe](/example-images/cover-swipe.png)
![cover swipe text](/example-images/cover-swipe-text.png)
![playlist](/example-images/playlist.png)
![search input](/example-images/search-input.png)
![search result](/example-images/search-result.png)
![shutdown](/example-images/shutdown.png)

# Requirements

* SDL2_ttf
* SDL2_image
* sdl2
* libmpdclient
* icu-uc
* boost_filesystem
* boost_system
* C++17

# State

Still experimental but pretty functional overall. I used a quick and dirty implementation of an IMGUI library (mostly for rapid development and learning purposes) which will be replaced soon. Sit tight for improvements and extended functionality (e.g., control all of the UI with an infa-red remote control).

# Contact

If you find my code useful please let me know. I'm also interested in your use case, any suggestions, improvements or criticism. Cheers!
