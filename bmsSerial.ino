#include "PL455.h"
#include "config.h"

PL455 bms;
elapsedMillis report;

void runReport() {
  if (report > REPORTING_PERIOD) {
    report = report - REPORTING_PERIOD;
    CONSOLE.print("Modules: ");
    CONSOLE.print(bms.getNumModules());
    CONSOLE.print(", Pack voltage: ");
    unsigned long packVolts = 0;
    for (byte module=0; module<bms.getNumModules(); module++) {
      packVolts = packVolts + bms.getModuleVoltage(module);
    }
    CONSOLE.print(float(packVolts)/100, min(VOLTS_DECIMALS, 2));
    CONSOLE.print("V, Min cell voltage: ");
    CONSOLE.print(float(bms.getMinCellVoltage())/10000,VOLTS_DECIMALS);
    CONSOLE.print("V, Imbalance: ");
    CONSOLE.print(float(bms.getDifCellVoltage())/10,1);
    CONSOLE.println("mV.\n");
    for (unsigned int module=0; module < bms.getNumModules(); module++) {
      CONSOLE.print("Module ");
      CONSOLE.print(module+1);
      CONSOLE.print(" - Voltage: ");
      CONSOLE.print(float(bms.getModuleVoltage(module))/100, min(VOLTS_DECIMALS, 2));
      CONSOLE.println("V.");
      for (unsigned int cell=0; cell<16; cell++) {
        CONSOLE.print("Cell");
        CONSOLE.print(cell+1);
        CONSOLE.print(" = ");
        CONSOLE.print(float(bms.getCellVoltage(module, cell))/10000, VOLTS_DECIMALS);
        CONSOLE.print("V  ");
      }
      CONSOLE.println();
      for (unsigned int aux=0; aux<8; aux++) {
        CONSOLE.print("Aux");
        CONSOLE.print(aux+1);
        CONSOLE.print(" = ");
        CONSOLE.print(float(bms.getAuxVoltage(module, aux))/10000, VOLTS_DECIMALS);
        CONSOLE.print("V  ");
      }
      CONSOLE.println();
      CONSOLE.print("Balancing status: ");
      for (unsigned int cell=0; cell<16; cell++) {
        CONSOLE.print(bms.getBalanceStatus(module, cell));
      }
      CONSOLE.println();
      CONSOLE.println();
      CONSOLE.println();
    }
  }
}

void setup() {
  CONSOLE.begin(9600); //for console
  delay(2000);
  CONSOLE.println("Serial BMS test\n\nUsing bq76PL455A\n");
  bms.init(1000000); //1Mbaud
  report = 0; //reset report counter so we get a few bms results before trying to print status...
}

void loop() {
  bms.runBMS();
  runReport();
}
