/* Library for the TI bq76PL455A 16 cell BMS chip,
 *  talking over UART. Written using the TI datasheet
 *  and scoping a BMW PHEV battery pack using this
 *  BMS.
 *  
 *  Matthew Collins December 2021
 */

#include "Arduino.h"
#include "PL455.h"

//PL455 register settings, stored here as LSByte first
const byte REG03[4] = { 0b00000010, 0b11111111, 0b11111111, 0b11111111}; //sample all cells, all aux, and vmodule
const byte REG07[1] = { 0b01111011}; //sample multiple times on same channel, 12.6us sampling, 8x oversample (recommended by TI)
const byte REG0C[1] = { 0b00001000}; //start autoaddressing
const byte REG0D[1] = { 16}; //16 battery cells
const byte REG0E[1] = { 0b00011001}; //internal reg enabled, addresses set by autoaddressing, comparators disabled, hysteresis disabled, faults unlatched
const byte REG0F[1] = { 0b10000000}; //AFE_PCTL enabled (recommended by TI)
const byte REG13[1] = { 0b10001000}; //balance continues up to 1 second following balancing enable, balancing continues through fault
const byte REG1E[2] = { 0b00000001, 0b00000000}; //enable module voltage readings
const byte REG28[1] = { 0x55}; //shutdown module 5 seconds after last comm
const byte REG32[1] = { 0b00000000}; //automonitor off
const byte REG3E[1] = { 0xCD}; //ADC sample period (60usec  - 0xBB, recommended by TI, but BMW set 0xCD);
const byte REG3F[4] = { 0x44, 0x44, 0x44, 0x44}; //AUX ADC sample period 12.6usec (recommended by TI);

//PL455::PL455(bmsbaud) {
//  //init(bmsbaud); //don't init in the constructor - called before setup()
//}

void PL455::commReset(bool reset) { //performs comms break or reset
  pinMode(1, OUTPUT);
  if (reset == 0) { //comms break
    //hold TX low for 12 bit periods
    unsigned long lowTime = 12000000/setBaud;
    digitalWrite(1, LOW);
    delayMicroseconds(lowTime);
    digitalWrite(1, HIGH);
  } else { //comms reset
    //hold TX low for at least 200usec
    digitalWrite(1, LOW);
    delayMicroseconds(300);
    digitalWrite(1, HIGH);
  }
}

void PL455::findMinMaxCellVolt() { //goes through the cell voltages finding the min, max, difference
  minCellVoltage = 65535;
  maxCellVoltage = 0;
  for (byte module = 0; module < numModules; module++) {
    for (byte cell = 0; cell < 16; cell++) {
      uint16_t cellVolt = cellVoltages[module][cell];
      if (cellVolt > CELL_IGNORE_VOLT) { //only process connected cells
        if (cellVolt > maxCellVoltage) {
          maxCellVoltage = cellVolt;
        }
        if (cellVolt < minCellVoltage) {
         minCellVoltage = cellVolt;
        }
      }
    }
  }
  difCellVoltage = maxCellVoltage - minCellVoltage;
//  CONSOLE.print("min: ");
//  CONSOLE.print(minCellVoltage);
//  CONSOLE.print(" max: ");
//  CONSOLE.print(maxCellVoltage);
//  CONSOLE.print(" diff: ");
//  CONSOLE.println(difCellVoltage);
}

void PL455::chooseBalanceCells() { //works out which cells need balancing
  for (int module = 0; module < numModules; module++) {
    for (int cell = 0; cell < 16; cell++) {
      if ((cellVoltages[module][cell] > minCellVoltage + BALANCE_TOLERANCE) && ((cellVoltages[module][cell] > BALANCE_MIN_VOLT) || ((BALANCE_WHILE_CHARGE == 1) && (1==1)))) { //replace 1==1 with a 'charging' variable
        balanceCells[module][cell] = 1;
      } else {
        balanceCells[module][cell] = 0;
      }
    }
  }
}

bool PL455::getBalanceStatus(byte module, byte cell) { //returns 1 if the cell is balancing
  return balanceCells[module][cell];
}

uint16_t PL455::adc2volt(uint16_t adcReading) { //converts ADC readings into voltage, 10ths of a millivolt
  uint32_t voltage = (uint32_t(50000) * uint32_t(adcReading)) / uint32_t(65535);
  return uint16_t(voltage);
}

float PL455::adc2temp(uint16_t adcReading) { //converts ADC readings into temperature, degC
  const float To = 290;
  const float Ro = 150;
  const float Rfix = 245;
  const float B = 1000;
  float R = (adcReading*Rfix)/(65535-adcReading);
  float invTemp = (1/To) + (1/B)*log(R/Ro);
  float temperature = (1/invTemp)-273;
  return float(temperature);
}

float PL455::getTemperature(byte module, byte sensor) {
  int aux = 0;
  switch (sensor) {
    case 0:
      aux = 0;
      break;
    case 1:
      aux = 2;
      break;
    case 2:
      aux = 1;
      break;
    default:
      aux = 0;
      break;
  }
  return (PL455::adc2temp(auxVoltages[module][aux]));
}

uint16_t PL455::getModuleVoltage(byte module) { //provides the overall voltage of the chosen module, in hundredths of a volt
  uint16_t moduleADC = moduleVoltages[module];
  uint32_t moduleVolts = (uint32_t(12500) * uint32_t(moduleADC)) / uint32_t(65535);
  return uint16_t(moduleVolts);
}

uint16_t PL455::getCellVoltage(byte module, byte cell) { //provides cell voltage, in 10ths of a millivolt
  uint16_t cellVolts = PL455::adc2volt(cellVoltages[module][cell]);
  return cellVolts;
}

uint16_t PL455::getAuxVoltage(byte module, byte aux) { //provides aux input voltage, in 10ths of a millivolt
  uint16_t auxVolts = PL455::adc2volt(auxVoltages[module][aux]);
  return auxVolts;
}

uint16_t PL455::getMinCellVoltage() {
  uint16_t minVolt = PL455::adc2volt(minCellVoltage);
  return minVolt;
//  return minCellVoltage;
}

uint16_t PL455::getDifCellVoltage() {
  uint16_t difVolt = PL455::adc2volt(difCellVoltage);
  return difVolt;
//  return difCellVoltage;
}

byte PL455::getSOC() {
  return SOC;
}

void PL455::setSOC() {
  SOC = map(minCellVoltage, SOC_LOW_ADC, SOC_HIGH_ADC, SOC_LOW_PERCENT, SOC_HIGH_PERCENT);
}

byte PL455::getInitFrame(byte _readWrite, byte scope, byte data_size) {
  byte initFrame=0; //variables for the initialization frame
  byte _data_size = 0; //bits 2 - 0
  byte _addr_size = ADDR_SIZE; //bit 3. 0 is 8bit address size (recommended by TI), 1 is 16bit (used by BMW)
  byte _req_type = 0; //bits 6 - 4
  byte _frm_type = 1; //bit 7. Will always be 1 here (0 is a response back from the PL455s)
  
  switch (data_size) {
    case 0:
      _data_size=0;
      break;
    case 1:
      _data_size=1;
      break;
    case 2:
      _data_size=2;
      break;
    case 3:
      _data_size=3;
      break;
    case 4:
      _data_size=4;
      break;
    case 5:
      _data_size=5;
      break;
    case 6:
      _data_size=6;
      break;
    case 8:
      _data_size=7;
      break;
    case 7:
    default://doesn't accept 7 bytes, or more than 8 bytes of data
      CONSOLE.print("ERROR: cannot write with ");
      CONSOLE.print(data_size);
      CONSOLE.println(" bytes!");
      break;
  }

  switch (scope) {
    case 0:
    case 1:
    case 3:
      _req_type = _readWrite | (scope << 1);
      break;
    default:
      CONSOLE.print("ERROR: Bad init frame scope: ");
      CONSOLE.print(scope);
      CONSOLE.println("!");
      break;
  }

  initFrame = _data_size | (_addr_size << 3) | (_req_type << 4) | (_frm_type << 7);
  return initFrame;
}

void PL455::writeRegister(byte scope, byte device_addr, byte register_addr, byte *data, byte data_size) {
  byte frameSize = 0;
  byte _initFrame = getInitFrame(1, scope, data_size);

  switch (scope) {
    case SCOPE_SINGLE:
    case SCOPE_GROUP:
      frameSize = data_size + 3 + ADDR_SIZE; //data, plus register address(es), plus device or group ID, plus command frame (CRC is calculated and added in send_Frame())
      break;
    case SCOPE_BRDCST:
      frameSize = data_size + 2 + ADDR_SIZE; //no group or device ID
      break;
    default: //panic!
       CONSOLE.print("ERROR: Bad scope - ");
       CONSOLE.print(scope);
       CONSOLE.println("!");
  }
  byte frame[frameSize];
  frame[0] = _initFrame;
    switch (scope) {
    case SCOPE_SINGLE:
    case SCOPE_GROUP:
      frame[1] = device_addr;
      frame[2] = 0;
      frame[2 + ADDR_SIZE] = register_addr;
      break;
    case SCOPE_BRDCST:
      frame[1] = 0;
      frame[1 + ADDR_SIZE] = register_addr;
      break;
  }
  for(byte i=0; i<data_size; i++) {
    frame[frameSize - data_size + i] = data[data_size - i - 1];
  }
  send_Frame(frame, frameSize);
}

void PL455::readRegister(byte scope, byte device_addr, byte group_id, byte register_addr, byte bytesToReturn) {
  byte frameSize = 0;
  byte _initFrame = getInitFrame(0, scope, 1);

  switch (scope) {
    case SCOPE_SINGLE:
    case SCOPE_BRDCST:
      frameSize = 4 + ADDR_SIZE; //command frame, (max) device address, register address (2 bytes if ADDR_SIZE=1), number of bytes to return
      break;
    case SCOPE_GROUP:
      frameSize = 5 + ADDR_SIZE; //command frame, group ID, register address, max device address, number of bytes to return
      break;
    default: //panic!
       CONSOLE.print("ERROR: Bad scope - ");
       CONSOLE.print(scope);
       CONSOLE.println("!");
  }
  byte frame[frameSize];
  frame[0] = _initFrame;
  switch (scope) {
    case SCOPE_GROUP:
      frame[1] = group_id;
      frame[2] = 0;
      frame[2 + ADDR_SIZE] = register_addr;
      frame[3 + ADDR_SIZE] = device_addr;
      break;
    case SCOPE_SINGLE:
      frame[1] = device_addr;
      frame[2] = 0;
      frame[2 + ADDR_SIZE] = register_addr;
      break;
    case SCOPE_BRDCST:
      frame[1] = 0;
      frame[1 + ADDR_SIZE] = register_addr;
      frame[2 + ADDR_SIZE] = device_addr;
  }
  frame[frameSize - 1] = bytesToReturn-1;
  send_Frame(frame, frameSize);
  registerRequested = register_addr;
  deviceRequested = device_addr;
  scopeRequested = scope;
  waitingForResponse = 1;
  sentRequest = 1;
  commTimeout = 0;
}

void PL455::init(uint32_t bmsbaud) { //'bmsbaud' sets the baud once running - the first frame is always 250000baud.
  bmsSteps = 100/(100-BALANCE_DUTYCYCLE); //managed balancing/voltage measurement
  bmsStepPeriod = BMS_CYCLE_PERIOD/bmsSteps;
  PL455::commReset(1);
  BMS.begin(250000);
  delay(100);
  byte baudsetting = 1; //keep to 250k in case of errors
  switch(bmsbaud) {
    case 125:
    case 125000:
      setBaud = 125000;
      baudsetting = 0;
      break;
    case 250:
    case 250000:
      setBaud = 250000;
      baudsetting = 1;
      break;
    case 500:
    case 500000:
      setBaud = 500000;
      baudsetting = 2;
      break;
    case 1:
    case 1000:
    case 1000000:
      setBaud = 1000000;
      baudsetting = 3;
      break;
    default:
      CONSOLE.println("ERROR - bad BMS baud setting");
      //set an error signal? wait forever?
  }
  //send packet to change speed to baud
  byte commdata[2] = {0, 0};
  commdata[0] = 0b11100000; //enable all comms apart from fault
  commdata[1] = baudsetting << 4; //set UART baud
  writeRegister(SCOPE_BRDCST, 0, 0x10, commdata, 2);
  BMS.flush(); //give time for the serial to be sent
  BMS.begin(setBaud);
  delayMicroseconds(20); //at least 10usec
  configure(); //general config
  setAddresses();
  //now do per-device configuration
  switch(numModules) {
    case 0: //no modules connected!!
      CONSOLE.println("ERROR: No CSCs connected!");
      break;
    case 1: //only one module
      commdata[0] = 0b10000000; //enable UART, disabled all other comms
      writeRegister(SCOPE_SINGLE, 0, 0x10, commdata, 2);
      break;
    default: // more than one module
      commdata[0] = 0b1100000; //enable UART and high side comms, not fault - first module
      writeRegister(SCOPE_SINGLE, 0, 0x10, commdata, 2);
      for (int i=1; i<numModules-1; i++) {
        commdata[0] = 0b01100000; //enable high and low side comms, not fault, disable UART - middle modules
        writeRegister(SCOPE_SINGLE, i, 0x10, commdata, 2);
      }
      commdata[0] = 0b00100000; //enable low side comms, not fault, disable high side and UART - last module
      writeRegister(SCOPE_SINGLE, numModules-1, 0x10, commdata, 2);
      break;
  }
  commTimeout = 0;
}

void PL455::configure() {
  writeRegister(SCOPE_BRDCST, 0, 0x07, REG07, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x0D, REG0D, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x0E, REG0E, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x0F, REG0F, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x13, REG13, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x1E, REG1E, 2);
  writeRegister(SCOPE_BRDCST, 0, 0x28, REG28, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x32, REG32, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x03, REG03, 4);
  writeRegister(SCOPE_BRDCST, 0, 0x3E, REG3E, 1);
  writeRegister(SCOPE_BRDCST, 0, 0x3F, REG3F, 4);
}

int PL455::getNumModules() {
  return int(numModules);
}

void PL455::setAddresses() {
  writeRegister(SCOPE_BRDCST, 0, 0x0C, REG0C, 1); //starts autoaddressing
  for (byte i=0; i<MAX_MODULES; i++) {
    delay(20);
    byte addr[1] = {i};
    writeRegister(SCOPE_BRDCST, 0, 0x0A, addr, 1); //address 0 - 15
  }
  //all modules will now have an address. Now we check with each one until we get no response.
  elapsedMillis timeout;
  byte checkModule = 0;
  while(timeout < 1000) { //allow 1000ms for each module to respond. This should be more than enough!!!
    PL455::listenSerial();
    if ((!sentRequest) && (checkModule != MAX_MODULES)) { //haven't sent our request yet, but don't ask for addresses > 15 (16th module)
      readRegister(SCOPE_SINGLE, checkModule, 0, 0x0A, 1); //read address of module
      timeout = 0; //don't include time taken to send request
    } else if(!waitingForResponse) { //just received a response
      if(bytesReceived == 4) { //init byte, data byte, 2 CRC bytes
        byte received[bytesReceived];
        for (int i=0; i<bytesReceived; i++) {
          received[i] = serialRXbuffer[i]; //copy into new array
        }
        if (received[1] == checkModule) { //received the right address back
          checkModule++;
          timeout=0;
          waitingForResponse=0;
          sentRequest=0;
        }
      }
    } //otherwise, we have sent a request, but haven't received a response yet
  }
  //timed out waiting for response, last module must have been the highest address
  CONSOLE.println("Received no further responses from modules.");
  numModules = checkModule;
}

uint16_t PL455::CRC16(byte *message, int mlength) {
  uint16_t CRC = 0;

  for (int i=0; i<mlength; i++)
  {
    CRC ^= (*message++) & 0x00FF;
    CRC = crc16_table[CRC & 0x00FF] ^ (CRC >> 8);
  }
  return CRC;
}

void PL455::send_Frame(byte *message, int messageLength) {
  uint16_t CRC;
  CRC = CRC16(message, messageLength);
  byte toSend[messageLength+2]; //+2 for CRC
  for (int i=0; i<messageLength; i++) {
    toSend[i] = message[i];
  }
  toSend[messageLength] = CRC & 0x00FF;
  toSend[messageLength+1] = (CRC & 0xFF00) >> 8;
//  CONSOLE.print("Sending 0x");
  for (int i=0; i<messageLength+2; i++) {
    BMS.write(toSend[i]);
//    if(toSend[i]<0x10) {
//      CONSOLE.print("0");
//    }
//    CONSOLE.print(toSend[i], HEX);
//    CONSOLE.print(" ");
  }
//  CONSOLE.println();
  BMS.flush();
//  delay(1);
}

void PL455::listenSerial() {//captures all the serial RX
  while(BMS.available() > 0)
  {
    byte inByte = BMS.read();
    if (waitingForResponse) { //if we're not looking for a response, this is definately noise.
      if (!rxInProgress) { //first byte of response
        //check this is a response byte
        bool respbyte = (inByte >> 7) & 1;
        byte numBytes = inByte & 0b01111111;
        if((respbyte == 0) && (numBytes <= 128)) { //probably a response byte
          serialRXbuffer[0] = inByte;
          bytesToReceive = numBytes+1; //number of _data_ bytes to receive. Also receive the init byte, plus two CRC bytes
          bytesReceived = 1;
          rxInProgress = 1;
        } else { //not a response byte - void
          rxInProgress = 0;
          bytesToReceive = 0;
          bytesReceived = 0;
        }
      } else { //subsequent bytes
        serialRXbuffer[bytesReceived] = inByte;
        bytesReceived++;
      }
      if (bytesReceived == bytesToReceive + 3) { //received complete frame
        byte completeFrame[bytesReceived];
//        CONSOLE.print("Received 0x");
        for (int i=0; i<bytesReceived; i++) {
          completeFrame[i] = serialRXbuffer[i]; //copy frame to new array
//          CONSOLE.print(completeFrame[i], HEX);
//          CONSOLE.print(" ");
        }
//        CONSOLE.println();
        if ( CRC16(completeFrame, bytesReceived) == 0) { //CRC checks ok
          waitingForResponse = 0;
          rxInProgress = 0;
          bytesToReceive = 0;
          commTimeout = 0;
        } else { //void
          rxInProgress = 0;
          bytesToReceive = 0;
          bytesReceived = 0;
        }
      }
    }
  }
}

void PL455::runBMS() { //called frequently from main()
  PL455::listenSerial();
  if (commTimeout > COMM_TIMEOUT) { //no response
    CONSOLE.println("ERROR: comms timeout?");
    commTimeout = 500; //don't spam the console
  }
  if (micros() - bmsStepPeriod > bmsStepTime) {
    PL455::setSOC();
    if (bmsStep == 0) { //first step - turn off balancing
      byte balanceDisable[2] = {0, 0};
      PL455::writeRegister(SCOPE_BRDCST, numModules-1, 0x14, balanceDisable, 2);
      bmsStepTime = micros();
      bmsStep++;
    } else if (bmsStep == 1) { //second step, read voltages
      //PL455::readRegister(byte scope, byte device_addr, byte group_id, byte register_addr, byte bytesToReturn);
      if (voltsRequested == 0) { //nothing requested yet
        PL455::readRegister(SCOPE_SINGLE, voltsRequested, 0, 0x02, 1); //slightly abuse the readRegister function to send a command
        voltsRequested++;
      } else if (waitingForResponse == 0) { //received a response
        //reset flags
        registerRequested = 0;
        deviceRequested = 0;
        scopeRequested = 0;
        sentRequest = 0;
        //read response
        for (unsigned int cell=0; cell<16; cell++) {
          uint16_t reading;
          reading = (serialRXbuffer[2*cell + 1] << 8) | serialRXbuffer[2*cell + 2];
          cellVoltages[voltsRequested-1][15-cell] = reading;
        }
        for (unsigned int aux=0; aux<8; aux++) {
          uint16_t reading;
          reading = (serialRXbuffer[2*aux + 33] << 8) | serialRXbuffer[2*aux + 34];
          auxVoltages[voltsRequested-1][7-aux] = reading;          
        }
        moduleVoltages[voltsRequested-1] = (serialRXbuffer[49] << 8) | serialRXbuffer[50];
        if(voltsRequested == numModules) {// received the last module data
          voltsRequested = 0;
          //update data
          PL455::findMinMaxCellVolt();
          PL455::chooseBalanceCells();
          //turn balancing back on
          for (unsigned int module=0; module<numModules; module++) {
            byte balanceEnable[2] = {0, 0};
            for (unsigned int cell=0; cell<8; cell++) {
              balanceEnable[0] = balanceEnable[0] | (balanceCells[module][cell] << cell);
            }
            for (unsigned int cell=8; cell<16; cell++) {
              balanceEnable[1] = balanceEnable[1] | (balanceCells[module][cell] << (cell-8));
            }
            PL455::writeRegister(SCOPE_SINGLE, module, 0x14, balanceEnable, 2);
          }
          //reset flags
          bmsStepTime = micros();
          bmsStep++;
        } else {
          PL455::readRegister(SCOPE_SINGLE, voltsRequested, 0, 0x02, 1);
          voltsRequested++; //don't reset the step timer - make sure this happens as quick as possible
        }
      }
    } else { //other steps, keep balancing on
      for (unsigned int module=0; module<numModules; module++) {
        byte balanceEnable[2] = {0, 0};
        for (unsigned int cell=0; cell<8; cell++) {
          balanceEnable[0] = balanceEnable[0] | (balanceCells[module][cell] << cell);
        }
        for (unsigned int cell=8; cell<16; cell++) {
          balanceEnable[1] = balanceEnable[1] | (balanceCells[module][cell] << (cell-8));
        }
        PL455::writeRegister(SCOPE_SINGLE, module, 0x14, balanceEnable, 2);
      }
      bmsStepTime = micros();
      bmsStep++;
      if (bmsStep >= bmsSteps) {
        bmsStep = 0;
      }
    }
  }
}
