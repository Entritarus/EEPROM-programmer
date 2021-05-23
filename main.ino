// we will need to use I2C communications
#include <Wire.h>
// data buffer for storing a eeprom page
char dataBuffer[16] = {};
// data buffer for storing incoming data from serial communication
char inputBuffer[128] = {};
// default eeprom address (read the datasheet)
int icaddress = 0b1010000;

// default config for 24c04
unsigned int bytesPerPage = 16;
unsigned int pageCount = 32;
unsigned int memoryPageCount = 2;
unsigned int memorySubPageCount = 4;
// command symbol storage
char command;
// write to a page in a memory page
void pageWrite(char *data, byte page) {
  // begin transmission
  Wire.beginTransmission(icaddress);
  // write byte address
  Wire.write(page * bytesPerPage); 
  // write data from data buffer
  for (int i = 0; i < bytesPerPage; i++) {
    Wire.write(*(data+i));
  }
  // end transmission with a stop condition
  Wire.endTransmission(1);
  // wait till the write cycle is finished
  delay(50);
}

// prints EEPROM memory page
void readMemPage(unsigned int memPage) {
  // some EEPROM chips require additional bits to address more memory
  icaddress = 0b1010000;
  icaddress |= memPage;
  // begin transmission 
  Wire.beginTransmission(icaddress);
  // start from zeroth byte at a memory page
  Wire.write(0);
  // end transmission, we won't write anything
  Wire.endTransmission(1);
  uint8_t j = 0;
  // from every page request bytes
  for (int i = 0; i < pageCount/memoryPageCount; i++) { 
    Wire.requestFrom(icaddress, bytesPerPage, 1);
    // print all requested bytes
    while (Wire.available()) {
      Serial.print(Wire.read(), HEX);
      Serial.print('\t');
      // go to a new line each 8 bytes printed
      if (!((j+1)%8)) Serial.println("");
      j++;
    }
  }
}
// write to a memory subpage
void memoryWrite(unsigned int subMemPage) {
  //reset I2C address to set a mempage
  icaddress = 0b1010000;
  // number of subpages = 4, number of mempages = 2;
  // number of current mempage = 1/2 number of current subpage
  icaddress |= subMemPage>>1;
  // we need to switch between subpages using offset
  // in one mempage there are 2 subpages
  // for odd subpage numbers don't add offset
  // for even subpage numbers add offset
  // offset = submemPage%2
  // since this function uses pageWrite, we need to multiply
  // offset by number of pages in one subpage
  // there are 128 bytes in one subpage
  // there are n bytes in one page
  // there are 128/n pages in one subpage
  unsigned int offset = subMemPage%2*128/bytesPerPage;
  Serial.println("Enter text");
  // wait till there's data in the input buffer
  while(!Serial.available());
  // read 128 bytes
  for (int i = 0; i < 128; i++) inputBuffer[i]=0;
  Serial.readBytesUntil(10,inputBuffer, 128);
    for(uint8_t i = 0; i<bytesPerPage*pageCount/memorySubPageCount/* && Serial.available()>0*/; i++) {
      dataBuffer[i%bytesPerPage] = inputBuffer[i];
      if(((i+1)%bytesPerPage) == 0/*||Serial.available() == 0*/) {
        // after populating the buffer, we need to send it
        // at the time of writing the last element, i is equal to 16*page-1
        // we can send data buffer to the memory at the page address i/bytesPerPage + offset
        pageWrite(dataBuffer, i/bytesPerPage+offset);
        Serial.print("Writing at page ");
        Serial.println(i/bytesPerPage+offset, DEC);
      }
    }
  
  // clean the Serial input buffer
  while(Serial.read()>-1);
  Serial.println("Communication ended");
}

void setup() {
  // initialize serial communitcations
  Wire.begin();
  Serial.begin(9600);
  // set timeout for parseInt
  Serial.setTimeout(10000);
  Serial.println("Command list:");
  Serial.println("e - erase chip");
  Serial.println("d - dump chip");
  Serial.println("p - page write");
  Serial.println("s - reset chip");
  Serial.println("c - configure");
  Serial.println("Default config - 24c04");
}

void loop() {
  // if there's incoming data
  if(Serial.available()) {
    // get command letter
    command = Serial.read();
    // erase the memory
    if (command == 'e') {
      Serial.println("Erasing chip");
      // fill up the data buffer with 0xff
      for(uint8_t i = 0; i < 16; i++) {
        dataBuffer[i] = 0xff;
      }
      // erase at every memory page
      icaddress = 0b1010000;
      for(int i = 0; i < memoryPageCount; i++) {
        icaddress |= i;
        for(int j = 0; j < pageCount/memoryPageCount; j++) {
          pageWrite(dataBuffer,j);
        }
      }
      Serial.println("Done Erasing");
    }
    // print data from entire memory
    if (command == 'd') {
      Serial.println("Showing contents of eeprom");
      for(int i = 0; i < memoryPageCount; i++) {
        Serial.print("Memory page ");
        Serial.println(i);
        readMemPage(i);
        icaddress++;
      }
    }
    // write to a memory subpage
    if (command == 'p') {
      Serial.println("Enter memory subpage number");
      // get subpage number
      uint8_t memSubPage = Serial.parseInt();
      // clear input to 
      Serial.read();
      Serial.read();
      // check if chosen subpage is in range
      if (memSubPage < memorySubPageCount){
        memoryWrite(memSubPage);
      } else
        Serial.println("Invalid memory page");
    }
    // reset routine, needed for resetting the memory chip in case of power cutoff
    // refer to the datasheet for more info
    if (command == 's') {
      Serial.println("Resetting");
      Wire.requestFrom(icaddress, 1);
      Wire.read();
      Wire.endTransmission();
    }
    // configure variables for work with certain memory chips
    if (command == 'c') {
      Serial.println("Select EEPROM config");
      Serial.println("1 - 24c01");
      Serial.println("2 - 24c02");
      Serial.println("4 - 24c04");
      Serial.println("8 - 24c08");
      Serial.println("16 - 24c16");
      uint8_t chip = Serial.parseInt();
      Serial.read();
      Serial.read();
      switch(chip) {
        case 1:
          bytesPerPage = 8;
          pageCount = 16;
          memoryPageCount = 1;
          memorySubPageCount = 1;
        break;
        case 2:
          bytesPerPage = 8;
          pageCount = 32;
          memoryPageCount = 1;
          memorySubPageCount = 2;
        break;
        case 4:
          bytesPerPage = 16;
          pageCount = 32;
          memoryPageCount = 2;
          memorySubPageCount = 4;
        break;
        case 8:
          bytesPerPage = 16;
          pageCount = 64;
          memoryPageCount = 4;
          memorySubPageCount = 8;
        break;
        case 16:
          bytesPerPage = 16;
          pageCount = 128;
          memoryPageCount = 8;
          memorySubPageCount = 16;
        break;
      }
    }
  }
  
}
