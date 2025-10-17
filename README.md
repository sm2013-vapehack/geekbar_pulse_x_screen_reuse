# GeekBar Pulse X Screen Reuse

![PXL_20251016_232409591](https://github.com/user-attachments/assets/f5fbc51c-dfc7-4388-912e-b49b4bb5c5c0)

This repo aims to divert old GeekBar Pulse X and other disposable vapes from the landfill by reusing the SPI LED displays from the devices. There are 2 *.ino files available: the general testing/display mapping script (general_use.ino), and a demo temperature sensor script (tempsense.ino) for use with an AGT20 or AGT10 sensor. The scripts were tested on an Arduino UNO, but they probably work on most other microcontrollers.

# IMPORTANT - READ BEFORE YOU TAKE THE VAPE APART!

For instructions on disassembling the GeekBar Pulse X disposable vape, I like this video: https://www.youtube.com/watch?v=1qDz5shnr1c&t=491s

Make sure to follow the instructions carefully, as there are lots of little things you could break inside the vape if you're not paying attention. 

# Screen compatibility:
Tempsense.ino only works with regular GeekBar Pulse X's, not any special editions like Pulse X edition or Pulse X Jam; you will have to change the mappings for it to work.
General_use.ino is compatible with all GeekBar Pulse X devices, and other vapes that use the same kind of display, like the Viho TRX 50K (not tested), with the exclusion of the "num", "digit", and "digits' serial commands, and the "displayDigit()", "displayDigits()", and "displayNumber()" functions. You will have to change the mappings for those to work.

# Serial command syntax:

baud: 9600

num N           -> Show number N (0-999999)

digit X Y       -> Show digit Y on position X (1-6)

digits a b c d e f -> Show 6 digits (use -1 for blank)

set X Y         -> Turn ON byte X bit Y

clr X Y         -> Turn OFF byte X bit Y

all 1           -> Turn all segments ON

all 0           -> Turn all segments OFF

clear           -> Clear display

# Function syntax:

setBit(int byteindex/segment, int bitindex/brightness, 0/1 for on/off)

clearDigits()

displayDigit(int digitindex, int value)

displayDigits(int d1, d2, d3, d4, d5, d6)

displayNumber(long num)

# Display mappings:

<img width="1536" height="1207" alt="pulsexmap" src="https://github.com/user-attachments/assets/ede244f0-5974-4609-8014-b0dea0dcc0de" />

1f: 7,7 1e: 15,7 2f: 47,7 2e: 55,7 3a: 0,7 3b: 1,7 3c: 2,7 3d: 3,7 3e: 4,7 3f: 5,7 3g: 6.7 4a: 8,7 4b: 9,7 4c: 10,7 4d: 11,7 4e: 12,7 4f: 13,7 4g: 14,7 5a: 40,7 5b: 41,7 5c: 42,7 5d: 43,7 5e: 44,7 5f: 45,7 5g: 46,7 6a: 48,7 6b: 49,7 6c: 50,7 6d: 51,7 6e: 52,7 6f: 53,7 6g: 54,7

# Wiring

Your GeekBar's display will look like this:
![vape-lcd-activation-v0-0mjdpd401a6e1](https://github.com/user-attachments/assets/37f14938-234e-4121-a5ae-5f791790619f)

The GND, VIN, CLK, and DIN pins may be labeled as G, V, C, and D on certain display revisions, but they are the same pins. These pins are what we are going to be using to interface with the screen. Connect V or VIN to 3.3v on your microcontroller, G or GND to ground on your microcontroller, and CLK/C and DIN/D to your microcontroller's MOSI (data) and SCK (clock) pins. For Arduino UNO, those pins are 13 for SCK and 11 for MOSI.
Note: On some screen revisions, the V or VIN pad is not connected to anything. In that case, you can connect 3.3v to C2 instead.

# Conclusion

After you have wired your display up, you can flash one of the sketches to your microcontroller using Arduino IDE. If you would like to make your own projects, you can use general_use.ino. Feel free to post the projects that you make with these usefull little displays in the discussions tab or on the subreddit r/streetlithium. If you have any questions, feel free to e-mail me at sawyermatheson037@gmail.com

WARNING: If you are allergic to AI generated code, please do not download anything from this repo. The code was written mostly using ChatGPT.
