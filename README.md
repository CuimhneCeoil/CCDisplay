# hd44780-i2c
This is a Linux kernel driver for Hitachi HD44780 LCDs attached to I2C bus via PCF8574 I/O expander. Ideal to use with Raspberry Pi and other small devices running Linux where I2C bus is available.

`$ uname -snrmv | tr -d '\n' > /dev/lcd0`
![hd44780-i2c driver in action](http://i.imgur.com/ct9uiRb.jpg)

### Features
The main goal was to expose HD44780-based LCDs behind regular Linux device files normally found in `/dev` directory. Thus, writing to the display is as easy as `echo Hello, world! > /dev/lcd0`.

There are no imposed limitations on number of concurrently attached devices. In practice, a single I2C bus allows up to 128 uniquely addressable devices. Furthermore, standard HD44780 LCD to I2C adapters usually use the same, hardcoded I2C address (like 0x27) and it's not possible to change them. The PCF8574 IC supports up to 8 different addresses, so a custom-build adapter might be the solution.

Multiple LCD geometries are supported (20x4, 16x8 and 8x1) and it's trivial to add new ones if needed.

Supported escape sequences:
* `\r` - Carriage return
* `\n` - Line feed (new line)

Supported control codes:
* `Ctrl-S`/`0x13` - Backlight off
* `Ctrl-Q`/`0x11` - Backlight on

Supported VT100 terminal control escape sequences:
* `<ESC>c` - Reinitialize the display.  Note: no `[` in the sequence.
* `<ESC>[nJ` - Erase in display. 
    * n=0 or not specified - Erase from the active position to the end of the screen, inclusive (default)
    * n=1 - Erase from start of the screen to the active position, inclusive
    * n=2 - Erase all of the display – all lines are erased and the cursor does not move.
* `<ESC>[H` - Cursor home
* `<ESC>[row;colH` - Moves the active position to the position specified by the parameters. This sequence has two parameter values, the first specifying the row position and the second specifying the column position. A parameter value of zero or one for the first or second parameter moves the active position to the first row or column in the display, respectively. The default condition with no parameters present is equivalent to a cursor to home action
* `<ESC>[nA` - Move cursor up n lines
* `<ESC>[nB` - Move cursor down n lines
* `<ESC>[nC` - Move cursor right n characters
* `<ESC>[nD` - Move cursor left n characters
* `<ESC>[nK` - Clear line from cursor.
    * n=0 or not specified - Clear line from cursor right, inclusive.
    * n=1 - Clear line from cursor left, inclusive.
    * n=2 - Clear entire line

**Note**: if column or row is beyond the display it the position will wrap around, it does not scroll.

Modified VT100 control escape sequences:
* `<ESC>[m>`/`<ESC>[0m` - turn off cursor attributes
* `<ESC>[4m` - turn on cursor display
* `<ESC>[5m` - turn on cursor blink
    
Device attributes exported via sysfs (`/sys/class/hd44780/<device_name>`):
* `backlight` - controls LCD backlight. Possible values: `0`, `1`
* `char0` through `char7` - sets or displays custom character 0 thorugh 7 respectivly.  Characters are encoded as 8 5-bit patterns encoded in base 32.
* `cursor_blink` - controls cursor blink. Possible values: `0`, `1`
* `cursor_display` - displays or hides cursor. Possible values: `0`, `1`
* `geometry` - sets LCD geometry. Possible values: `20x4`, `16x2`, `8x1`
* `one_line` - each print to the LCD device will start at home position and will clear to end of display after printing.

#### Character encoding

The custom characters are 8 lines of 5 bits.  Each line is encoded as a base 32 digit.  The values of the encodings are [0-9A-V].  The following shows the encoding for a vary small "hi" over a "U" pattern pattern:

```
*   *   10001 - encodes as -> H
*** *   11101 --------------> T
* * *   10101 --------------> L
        00000 --------------> 0
*   *   10001 --------------> H
*   *   10001 --------------> H
*   *   10001 --------------> H
 ***    01110 --------------> E
```
To set character 0 to this character execute `echo HTL0HHHE > /sys/class/hd44780/lcd0/char0`

The site https://omerk.github.io/lcdchargen/ will assist in the creation of the patterns.

The site https://www.unitconverters.net/numbers/binary-to-base-32.htm will provide assistance with the encoding.

To print the character simply print `\x0`

#### Table of special characters

The table below shows the special characters interpreted by this driver

| Dec | Hex | Oct | Char | Description |
| --- | --- | --- | ---- | ----------- |
|  0  |  0  |  0  | Null | custom character 0 |
|  1  |  1  |  1  | SOX  | custom character 1 |
|  2  |  2  |  2  | STX  | custom character 2 |
|  3  |  3  |  3  | ETX  | custom character 3 |
|  4  |  4  |  4  | EOT  | custom character 4 |
|  5  |  5  |  5  | ENQ  | custom character 5 |
|  6  |  6  |  6  | ACK  | custom character 6 |
|  7  |  7  |  7  | BEL  | custom character 7 |
| 10  |  A  | 10  |  LF  | line feed          |
| 13  |  D  | 15  |  CR  | carriage return    |
| 17  | 11  | 21  | DC1  | backlight on (Ctrl-Q) |
| 19  | 13  | 23  | DC3  | backlight off (Ctrl-S) |
| 27  | 1B  | 33  | ESC  | escape             |

### Usage
1. Insert kernel module: `insmod hd44780.ko`.
2. Let the I2C adapter know that there's a new device attached: `echo hd44780 0x27 > /sys/class/i2c-adapter/i2c-1/new_device`.
You may need to replace the device's I2C address and adapter path with proper values.
3. At this point a new device should appear (`/dev/lcd0`) and you should be able to write to it.
