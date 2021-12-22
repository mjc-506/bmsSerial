# bmsSerial
Code to talk to a bq76PL455A BMS chip, as used in BMW PHEV battery packs

If using a complete BMW PHEV pack (5 or 6 modules), then I would recommend using simpBMS, which talks to the primary CSC (white case) over CAN, and requires no hardware modifications (aside from a bit of wiring perhaps). If, however, you need to use a partial pack, or more than one pack, this code will allow from one to 16 modules in a single string. (Using the BMW hardware over CAN will work for partial packs, but will not allow balancing. The BMW hardware over CAN will also not allow >6 (or 5) modules in a string.)

# Required Hardware Modifications
This code talks to the first CSC over UART, which then communicates with any additional CSCs over the TI twisted pair comms. You will need to perform a small modification to the first CSC to get to the UART lines. It is possible to use the primary CSC (white case) or any of the other CSCs (black case) as your first module. Subsequent modules use the BMW harness (modified if required for different numbers of modules) to connect up.

#Using the white primary CSC
<text and photos>

#Using a black secondary CSC
<text and photos>
