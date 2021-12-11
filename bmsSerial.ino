#include "PL455.h"
#include "config.h"

PL455 bms;

void setup() {
  CONSOLE.begin(9600); //for console
  delay(2000);
  CONSOLE.println("Serial BMS test\n\nUsing bq76PL455A\n");
  bms.init(1000000);
}

void loop() {
  byte data[4] = {0x11, 0x80, 0x30, 0xF8};
  bms.writeRegister(SCOPE_BRDCST, 0, 0x0E, data, 4);
  delay(1000);
  bms.readRegister(SCOPE_SINGLE, 5, 0, 0x0A, 1);
//  CONSOLE.print("Detected ");
//  CONSOLE.print(bms.getNumModules());
//  CONSOLE.println(" CSCs");
  delay(1000);

}
