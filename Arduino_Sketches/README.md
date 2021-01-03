# Arduino Sketches

These are the Logic Engine sketches by Paul Murphy, and include basic functions to get logics doing their thing.

For sketches with more advanced functionality, see Neil's firmware here : https://github.com/nhutchison/LogicEngine

## LE_AVR_slim
A super basic sketch, intended to run front and rear logics from a single AVR board (or an Arduino UNO)

## LE_Reactor_Zero_V6_basic
The sketch that ships out with the V6 (2021) Reactor Zero boards. Uses standard Adafruit libraries to make text scrolling easy. Includes support for new 112 rear LED board.

## LE_bare_bones
This sketch is a intended to be a minimal implimentation. It includes code for standard logic display animations only (no communication or PSI code). Intended for using as groundwork for a more complicated sketch.

## LE_Teensy_Reactor_Jawalite_0.5
This is the sketch that shipped with the Teensy Reactor board in the 2017 Logic Engine kits. It runs front and rear logic displays from a single Teensy 3.2 board. Adjustable settings (delay, fade, brightness, color) are stored in EEPROM and can be adjusted via the trimpots on the Teensy Reactor board, or via Jawalite serial commands (from a JEDI controller, Marcduino or other microcontroller). It will recognise and respond to a small handful of display sequence commands (Alarm, March, Leia, Failure).
