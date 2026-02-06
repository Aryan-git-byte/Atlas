# Atlas DevBoar

## about the Board:
It is one of those perfect devboard i need for my projects, especically my IoTs one, and i was pretty tired of conecting different modules together, wiring em and all, SO i thought that why cant i make a board than fulfills my need, and this may even come handy for many other hackers/dev too. 
This board comprises of inbuilt BMS, GSM, GPRS, GPS, RS485 communication, mic, speaker, sd slots etc
with the main brain being esp32 s3 and A9G for gps/gprs/gsm.

## How to use it:

Its pretty simple can just upload the code in firmawre/ through the usb connector or power it through the jst, and just upload code , interact with other modules as describe in schematic and BOOM ur powerful devboard workingggg.
i will also try to make the complete datasheet very soon(probably after final exams ;D), 
some serial cmds to test the board:

`help or ?` - show command menu   
`status` - show system info   
`reboot` - restart system   
`sd.ls` - list files  
`sd.cat` <filename> - read file   
`sd.rm` <filename> - delete file  
`gsm.at` <command> - send AT command  
`gsm.rssi` - signal strength  
`gsm.sms` <number> <message> - send SMS   
`gps.on` - enable GPS   
`gps.off` - disable GPS   
`gps.loc` - get GPS location  
`485.tx` <message> - send data    
`pin.set` <pin> <0|1> - set pin state   
`pin.get` <pin> - read pin state  
`pwr.sleep` <seconds> - light sleep   
`pwr.vbat` - read battery voltage 

## Here are some cool images of the board:

![2d view of board](Assets/2d-model.png)
 ![3d view of board](Assets/3d-model-1.png)
 ![3d view of board](Assets/3d-model-2.png)

## Schematic:

![schematic](Assets/Sheet_1.png)
![schematic](Assets/Sheet_2.png)

## BOM:
| ID | Component / Part Name                    | Quantity |
| -- | ---------------------------------------- | -------- |
| 1  | ZX-MSIM-481.35J-Z6 (SIM holder)          | 1        |
| 2  | TF-CARD H1.8 SY (MicroSD slot)           | 1        |
| 3  | 100 nF Capacitor                         | 9        |
| 4  | 10 kΩ Resistor                           | 8        |
| 5  | KH-PH-2P-W JST Connector                 | 1        |
| 6  | Main Power Switch                        | 1        |
| 7  | HX-6×6×7.3 Tactile Button                | 2        |
| 8  | USB Type-C 3.1 (16-Pin)                  | 1        |
| 9  | 470 µF Capacitor                         | 1        |
| 10 | 100 µF Capacitor                         | 1        |
| 11 | YLED0805G LED                            | 2        |
| 12 | 5.1 kΩ Resistor                          | 2        |
| 13 | TP4056 Li-ion Charger IC                 | 1        |
| 14 | AMS1117-3.3V Regulator                   | 1        |
| 15 | 1 kΩ Resistor                            | 4        |
| 16 | 1.2 kΩ Resistor                          | 1        |
| 17 | 100 Ω Resistor                           | 1        |
| 18 | 10 kΩ Resistor                           | 1        |
| 19 | 10 µF Capacitor                          | 4        |
| 20 | 47 µF Capacitor                          | 1        |
| 21 | 22 µF Capacitor                          | 1        |
| 22 | SS34 Schottky Diode                      | 1        |
| 23 | FS8205 MOSFET Protection                 | 1        |
| 24 | DW01A Battery Protection IC              | 1        |
| 25 | MT3608 Boost Converter                   | 1        |
| 26 | 22 µH Inductor                           | 1        |
| 27 | 1 µF Capacitor                           | 2        |
| 28 | CMA-4544PF-W Microphone                  | 1        |
| 29 | 22 Ω Resistor                            | 2        |
| 30 | 2.2 kΩ Resistor                          | 1        |
| 31 | 10 Ω Resistor                            | 2        |
| 32 | A9G GSM/GPRS/GPS Module                  | 1        |
| 33 | ESP32-S3-WROOM-1 (N16R8)                 | 1        |
| 34 | MAX3485 RS485 Transceiver                | 1        |
| 35 | 75 kΩ Resistor                           | 1        |
| 36 | Male Header 2.54mm 1×15                  | 2        |
| 37 | Male Header 2.54mm 1×3                   | 1        |
| 38 | Speaker                                  | 1        |
| 39 | U.FL RF Connector (UFL1202A1-001-GPG-50) | 2        |

## GG, let me know if anything's left out