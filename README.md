# Usage

This the frontend GUI application for my DIY HiFi system and digital audio streaming solution (based on Raspberry Pi 2B and HifiBerry AMP+) showing on an IL9341 LCD including a touch screen (with 320x240 resolution). It allows controlling a running MPD instance.

As the display lacks any hardware acceleration and is run from the framebuffer, the draw operations are largely reduced. I.e. there is no periodic rendering. The screen is updated when necessary.

## Configuration

On the first run the program will install the default configuration file (usually in `$HOME/.config/mpd-touch-screen-gui/program.conf` and `client.conf` for the client). See [client.conf](data/client.conf) and [program.conf](data/program.conf) for the default configurations. There is quite a bit you can change that changes behavior and appearance of the application.

If you don't intend to run it on the same machine, you should set the MPD environment variables (i.e. `MPD_HOST`).

# Features

* Global button controls on the left side that are visible in every view. This allows basic playback controls and switching the view.
* Several views that will be extended in the future:
    * Cover view that displays a cover or a replacement with song information. Swiping gestures allow control over the playback (left, right and press) and volume (up and down).  
        ![cover swipe](/example-images/cover-swipe.png)
        ![cover swipe text](/example-images/cover-swipe-text.png)
    * A playlist view allows viewing and changing the song from the current playlist. Touch gestures allow scrolling throught the playlist vertically and horizontally. Longer swipes will scroll more.  
        ![playlist](/example-images/playlist.png)
    * In the search view an on-screen keyboard is presented that allows to search for songs case-insensitive. The results may be browsed and played in the same manner as with the playlist view.  
        ![search input](/example-images/search-input.png)
        ![search result](/example-images/search-result.png)
    * A final view allows shutting down and rebooting the machine.  
        ![shutdown](/example-images/shutdown.png)
* A configurable UDP client allows basic UI controls (intended for use with an IR remote) in 6 directions (next and previous in x, y and natural order)

# Requirements

* `SDL2_ttf` (font rendering)
* `SDL2_image` (cover image loading)
* `sdl2` (rendering)
* `libmpdclient` (communication with MPD instance)
* `icu-uc` (lower-case conversion for case insensitive search)
* `boost_filesystem`
* `boost_system`
* `boost_asio` (UDP interface for remote navigation)
* `libwtk-sdl2` (widgets)
* C++17

# Installation

*mpd-touch-screen-gui* uses autotools as build system:
* `autoreconf --install`
* `./configure`
* `make`
* `make install`

# Contact

If you find my code useful please let me know. I'm also interested in your use case, any suggestions, improvements or criticism. Cheers!
