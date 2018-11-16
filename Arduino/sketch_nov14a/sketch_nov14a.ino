//Code for ESP8266
#include <SPI.h>
#include <cstdint>

enum CommandType : uint8_t {
  MOTOR1,
  MOTOR2,
  DELAY,
  END
};

const int numDrinks = 6;

//const char *drinkNames[][] = {};

int drinkRecipes[][4] {
  {2, 2, 2, 2},
  {4, 0, 4, 0},
  {0, 4, 0, 4},
  {8, 0, 0, 0},
  {0, 8, 0, 0},
  {0, 0, 8, 0},
};

struct Command {
  CommandType cmd;
  int32_t payload;
};

void SPI_sendObject(const Command &value)
{
  const uint8_t *p = (uint8_t*) &value;
  unsigned int i;
  for (i = 0; i < sizeof(value); i++)
    SPI.transfer(*p++);
}
int drinkOrder = 3;


void setup() {
  Serial.begin(115200);
  SPI.begin();
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
  SPI.beginTransaction(SPISettings(18000000, MSBFIRST, SPI_MODE0));
  SPI.setClockDivider(SPI_CLOCK_DIV8);
  delay(800);
}

void sendCommand(CommandType cmd, int32_t p) {
  Command c;
  c.cmd = cmd;
  c.payload = p;
  digitalWrite(SS, LOW);
  SPI_sendObject(c);
  digitalWrite(SS, HIGH);
}

//Good rule of thumb: spend no more than 20 ms in the loop
//to yield control the the WiFi and TCP stack so it can
//do its thing.
void loop() {
  if (drinkOrder == 0) {
    ;
  }
  else {
    sendCommand(MOTOR1, -1000);
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < drinkRecipes[drinkOrder][i]; j++) {
        sendCommand(MOTOR2, 500);
        sendCommand(DELAY, 100);
        sendCommand(MOTOR2, -500);
      }
      sendCommand(MOTOR1, 200);
    }
    sendCommand(MOTOR1, 450);
    sendCommand(END, 0);
    drinkOrder = 0;
    Serial.println("Sent order commands.");
  }
}

