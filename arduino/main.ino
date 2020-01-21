#include <SPI.h>
#include <SD.h>

Sd2Card card;

#define CHIP_SELECT 10

uint8_t buffer_block[512] = {0x00};
char request[5];
uint32_t block;

void setup() {
  for(int i = 0; i < 3; i++) {
      pinMode(i+5, OUTPUT);
      digitalWrite(i+5, LOW);
    }

  Serial.begin(115200);
  while(!Serial);

  if (!card.init(SPI_FULL_SPEED, CHIP_SELECT)) {
    while(1);
  }
}

void loop() {
  if (Serial.available() > 0) {
    Serial.readBytes(request, 5);
    if (request[0] == 0) {
      char buff[4];
      uint32_t* buf = (uint32_t *) &buff;
      *buf = card.cardSize();
      Serial.write(buff, 4);
    } else if (request[0] == 1) {
      uint32_t* block_to_send = (uint32_t*) &request[1];
      card.readBlock(*block_to_send, buffer_block);
      Serial.write(buffer_block, 512);
    } else if (request[0] == 2) {
      uint32_t* block_to_write = (uint32_t*) &request[1];

      card.writeStart(*block_to_write, 8);
      for(int i = 0; i < 8; i++) {
        Serial.readBytes(buffer_block, 512);
        card.writeData(buffer_block);
      }
      card.writeStop();
    }
  }
}