/* Library for the TI bq76PL455A 16 cell BMS chip,
 *  talking over UART. Written using the TI datasheet
 *  and scoping a BMW PHEV battery pack using this
 *  BMS.
 *  
 *  Matthew Collins December 2021
 */

#include "Arduino.h"
#include "PL455.h"

//PL455 register settings
const byte REG07[1] = { 0b01111000}; //sample multiple times on same channel, 12.6us sampling, no averaging
const byte REG0C[1] = { 0b00001000}; //start autoaddressing
const byte REG0D[1] = { 16}; //16 battery cells
const byte REG0E[1] = { 0b00011001}; //internal reg enabled, addresses set by autoaddressing, comparators disabled, hysteresis disabled, faults unlatched
const byte REG0F[1] = { 0b10000000}; //AFE_PCTL enabled (recommended by TI)
const byte REG13[1] = { 0b10001000}; //balance continues up to 1 second following balancing enable, balancing continues through fault
const byte REG1E[2] = { 0b00000000, 0b00000001}; //enable module voltage readings
const byte REG28[1] = { 0b11000000}; //shutdown module 1 second after last comm
const byte REG32[1] = { 0b00000000}; //automonitor off

//PL455::PL455(bmsbaud) {
//  //init(bmsbaud); //don't init in the constructor - called before setup()
//}

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
    frame[frameSize - data_size + i] = data[i];
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
}

void PL455::init(uint32_t bmsbaud) { //'bmsbaud' sets the baud once running - the first frame is always 250000baud.
  BMS.begin(250000);
  delay(100);
  byte baudsetting = 1; //keep to 250k in case of errors
  switch(bmsbaud) {
    case 125:
    case 125000:
      bmsbaud = 125000;
      baudsetting = 0;
      break;
    case 250:
    case 250000:
      bmsbaud = 250000;
      baudsetting = 1;
      break;
    case 500:
    case 500000:
      bmsbaud = 500000;
      baudsetting = 2;
      break;
    case 1:
    case 1000:
    case 1000000:
      bmsbaud = 1000000;
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
  BMS.begin(bmsbaud);
  delay(10);
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
}

void PL455::configure() {
  writeRegister(SCOPE_BRDCST, 0, 0x07, REG07, 1);
  delay(10);
  writeRegister(SCOPE_BRDCST, 0, 0x0D, REG0D, 1);
  delay(10);
  writeRegister(SCOPE_BRDCST, 0, 0x0E, REG0E, 1);
  delay(10);
  writeRegister(SCOPE_BRDCST, 0, 0x0F, REG0F, 1);
  delay(10);
  writeRegister(SCOPE_BRDCST, 0, 0x13, REG13, 1);
  delay(10);
  writeRegister(SCOPE_BRDCST, 0, 0x1E, REG1E, 2);
  delay(10);
  writeRegister(SCOPE_BRDCST, 0, 0x28, REG28, 1);
  delay(10);
  writeRegister(SCOPE_BRDCST, 0, 0x32, REG32, 1);
  delay(10);
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
  while(timeout < 100) { //allow 100ms for each module to respond. This should be more than enough!!!
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
  CONSOLE.print("Sending 0x");
  for (int i=0; i<messageLength+2; i++) {
    BMS.write(toSend[i]);
    if(toSend[i]<0x10) {
      CONSOLE.print("0");
    }
    CONSOLE.print(toSend[i], HEX);
    CONSOLE.print(" ");
  }
  CONSOLE.println();
  BMS.flush();
}

void PL455::serialEvent1() //captures all the serial RX
{
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
          bytesToReceive = numBytes+1; //number of data bytes to receive. Also receive the init byte, plus two CRC bytes
          bytesReceived = 1;
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
        CONSOLE.print("Received 0x");
        for (int i=0; i<bytesReceived; i++) {
          completeFrame[i] = serialRXbuffer[i]; //copy frame to new array
          CONSOLE.print(completeFrame[i], HEX);
          CONSOLE.print(" ");
        }
        CONSOLE.println();
        if ( CRC16(completeFrame, bytesReceived) == 0) { //CRC checks ok
          waitingForResponse = 0;
          rxInProgress = 0;
          bytesToReceive = 0;
        } else { //void
          rxInProgress = 0;
          bytesToReceive = 0;
          bytesReceived = 0;
        }
      }
    }
  }
}
