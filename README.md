# Xeyes for Windows

This code came from the no longer available
[Yutaka Hirata xeyes repo](https://github.com/yutakakn/xeyes)
as demonstrated in
[video](https://www.youtube.com/watch?v=S0Xo-uvGuJY&t=2s).
We added CMake and updated the API use to compile with MSVC, GCC, Clang...

## Overview

In the old days, UNIX workstations had large and monochrome screen.
So, It was easy to lose sight of the mouse cursor.
One solution was xeyes. The xeyes application is able to follow
the mouse cursor.
Xeyes for Windows is an xeyes application that runs on Windows.

This software is based on WinEyes 1.2.

Special thanks to Robert W. Buccigrossi.

## Sample movie

[![Xeyes for Windows](https://img.youtube.com/vi/S0Xo-uvGuJY/0.jpg)](https://www.youtube.com/watch?v=S0Xo-uvGuJY "Introduction")


## Supported OS

Microsoft Windows 10 and 11

## Usage

### Command line options:

- Specifying window size and position.
  - -geometry WIDTHxHEIGHT+XOFF+YOFF
  - -geometry WIDTHxHEIGHT
  - -geometry +XOFF+YOFF
- Specifying screen no of multi monitors.
  - -monitor screen_no
    - screen_no: 1, 2, ... 32

*Sample of command line options:*

```
; Display the app on primary monitor.
xeyes.exe -monitor 1

; Display the app on third monitor.
xeyes.exe -monitor 3

; All monitors are displayed as one coordinate axis,
; with the origin at the upper left corner of the primary monitor
; at X=2000, Y=800.
; The width of the applicaiton is 300 and the height is 700.
xeyes.exe -geometry 300x200+2000+700

; Display the app at X coordinate 100 and Y coordinate 80,
; with the origin in the upper left of second monitor.
xeyes.exe -monitor 2 -geometry +100+80
```

Terminate all xeyes application by ALT-space to bring up the system menu and then select "Terminate all xeyes".

Move the eyes by left-click, hold, and dragging on the eyes

Resizing the eyes by Double-click or right-click the eyes to bring up the windows frame.
Click and drag the frame.  Then double-click or right-click the eyes to remove the frame.

### Always on top:
Hit ALT-space to bring up the system menu and then select "Always on top".
Alternately: Double-click or right-click the eyes to bring up the windows frame.
Then right-click on the menu bar to bring up the system menu.
Select "Always on top".
Then double-click or right-click the eyes to remove the frame.


## History

*08/28/2022 Ver1.0*
- Added some new features.
- Bug fix a problem of original version.
- Improved mouse cursor detection.


## License

GPLv2

[Please find attached file](LICENSE)

## Author

(C) 2022 Yutaka Hirata(YOULAB)

GitHub: https://github.com/yutakakn

Homepage: https://hp.vector.co.jp/authors/VA013320/index.html

## Original version

WinEyes:  Released under GPL by Robert W. Buccigrossi

https://sourceforge.net/projects/wineyes/
