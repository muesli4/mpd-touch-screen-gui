<!--
SPDX-FileCopyrightText: 2020 - 2022 Doron Behar
SPDX-FileCopyrightText: Moritz Bruder <muesli4 at gmail dot com>

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Usage

The goal of this project is to provide a graphical application suited for a dedicated player based on MPD.

## Example Use Case

My DIY HiFi system and digital audio streaming solution (based on Raspberry Pi 2B and HifiBerry AMP+) showing on an IL9341 LCD including a touch screen (with 320x240 resolution).  As the display lacks any hardware acceleration and is run from the framebuffer, the draw operations are largely reduced. I.e. there is no periodic rendering. The screen is updated when necessary.

# Features

* Global button controls on the left side that are visible in every view. This enables basic playback controls and switching the view.
* Several views that will be extended in the future:
    * Cover view that displays a cover or a replacement with song information. Swiping gestures allow control over the playback (left, right and press) and volume (up and down).  
        ![cover swipe](/example-images/cover-swipe.png)
        ![cover swipe text](/example-images/cover-swipe-text.png)
    * A playlist view enables viewing and changing the song from the current playlist. Touch gestures allow scrolling throught the playlist vertically and horizontally. Longer swipes will scroll more.
        ![playlist](/example-images/playlist.png)
    * In the search view an on-screen keyboard is presented that enables to search for songs case-insensitive. The results may be browsed and played in the same manner as with the playlist view.
        ![search input](/example-images/search-input.png)
        ![search result](/example-images/search-result.png)
    * A final view enables shutting down and rebooting the machine.
        ![shutdown](/example-images/shutdown.png)
* A configurable UDP client enables basic UI controls (intended for use with an IR remote) in 6 directions (next and previous in x, y and natural order):
    ```
    > ./mpd-touch-screen-gui-send 
    Usage: ./mpd-touch-screen-gui-send CMD [SERVER [PORT]]
    CMD    - One of: l, r, u, d, n, p, a, <, >, m
    SERVER - Override config value for server.
    PORT   - Override config value for port.
    ```
    To use the UDP interface, it has to be activated in the program config. The protocol is very simple and consists just of the letters.

## Configuration

On the first run the program will install the default configuration file (usually in `$HOME/.config/mpd-touch-screen-gui/program.conf` and `client.conf` for the client). See [client.conf](data/client.conf) and [program.conf](data/program.conf) for the default configurations. There is quite a bit you can change that changes behavior and appearance of the application.

If you do not intend to run it on the same machine, you should set the MPD environment variables (i.e. `MPD_HOST`).

# Requirements

* `SDL2_ttf` (font rendering)
* `SDL2_image` (cover image loading)
* `sdl2` (rendering)
* `libmpdclient` (communication with MPD instance)
* `icu` (sometimes named `icu-uc` / `icu` / `libicu` - use lower-case conversion for case insensitive search)
* `boost_filesystem`
* `boost_system`
* `boost_asio` (UDP interface for remote navigation)
* [`libwtk-sdl2`](../../../libwtk-sdl2) (widgets)
* [libconfig](https://www.hyperrealm.com/libconfig/libconfig.html)

## Additional Build Requirements

These are only required on the host:

* C++17 compiler such as `gcc`
* `pkg-config`
* `autoconf` & `automake` & `libtool`

# Installation

*mpd-touch-screen-gui* uses autotools as build system:
* `autoreconf --install`
* `./configure`
* `make`
* `make install`

# Contact

If you find my code useful please let me know. I am also interested in your use case, any suggestions, improvements or criticism. Cheers!
