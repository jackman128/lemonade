// SPI full-duplex slave example
// STM32 acts as a SPI slave and reads 8 bit data frames over SPI.
// Master also gets a reply from the slave, which is a a simple count (0, 1, 2, 3)
// that is incremented each time a data frame is received.
// Serial output is here for debug

#include <Arduino.h>
#include <BasicStepperDriver.h>
#include <A4988.h>
#include <SPI.h>
#include <cstdint>
#include <LinkedList.h>

const int MOTOR_STEPS = 200;
// Target RPM for cruise speed
const int RPM = 250;
// Acceleration and deceleration values are always in FULL steps / s^2
const int MOTOR_ACCEL = 500;
const int MOTOR_DECEL = 500;

// Microstepping mode. If you hardwired it to save pins, set to the same value here.
const int MICROSTEPS = 16;
const int DIR1 = PA5;
const int STEP1 = PA6;
const int ENABLE = PA7;
const int DIR2 = PB0;
const int STEP2 = PB1;
const int statusPin = PA10;

const int MS1 = 10;
const int MS2 = 11;
const int MS3 = 12;
A4988 stepper1(MOTOR_STEPS, DIR1, STEP1, ENABLE);//, MS1, MS2, MS3);
A4988 stepper2(MOTOR_STEPS, DIR2, STEP2, ENABLE);//, MS1, MS2, MS3);

enum CommandType : uint8_t {
  MOTOR1,
  MOTOR2,
  DELAY,
  END
};

enum State {
  WORKING,
  DONE
};

struct Command {
  CommandType cmd;
  int32_t payload;
};

SPIClass SPI_2(2);


void SPI_readObject(Command &value)
{
    uint8_t *p = (uint8_t*) &value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          *p++ = SPI_2.transfer(0);
    //return i;
}

bool ready = true;
LinkedList<Command*> cmdList;

void setup()
{
  Serial.begin(115200);
  delay(100);
  pinMode(PC13, OUTPUT);
  pinMode(statusPin, OUTPUT);
  digitalWrite(PC13, LOW);
  digitalWrite(statusPin, LOW);
  SPI_2.begin();
  SPI_2.beginTransactionSlave(SPISettings(18000000, MSBFIRST, SPI_MODE0));
  stepper1.begin(RPM, MICROSTEPS);
  stepper1.setSpeedProfile(stepper1.LINEAR_SPEED, MOTOR_ACCEL, MOTOR_DECEL);
  stepper1.disable();
  //stepper1.begin(RPM, MICROSTEPS);
  //stepper1.setSpeedProfile(stepper1.LINEAR_SPEED, MOTOR_ACCEL, MOTOR_DECEL);
  //stepper1.disable();
  LinkedList<Command*> cmdList = LinkedList<Command*>();
}

uint8_t count = 0;
State state = DONE;

void loop()
{
  Command readCommand;
  Command *listCommand = nullptr;
  switch (state) {
    case DONE:
    SPI_readObject(readCommand);
    if (readCommand.cmd == DELAY || readCommand.cmd == MOTOR1 || readCommand.cmd == MOTOR2) {
      listCommand = new Command;
      *listCommand = readCommand;
      cmdList.add(listCommand);
      listCommand = nullptr;
    }
    else if (readCommand.cmd == END) {
      listCommand = new Command;
      *listCommand = readCommand;
      cmdList.add(listCommand);
      listCommand = nullptr;
      state = WORKING;
      digitalWrite(PC13, HIGH);
      digitalWrite(statusPin, HIGH);
    }
    break;
    
    case WORKING:
    readCommand = *cmdList.get(0);
    cmdList.pop();
    switch (readCommand.cmd) {
      case DELAY:
      delay(readCommand.payload);
      break;
      case MOTOR1:
      stepper1.enable();
      stepper1.rotate(readCommand.payload);
      stepper1.disable();
      break;
      case MOTOR2:
      stepper2.enable();
      stepper2.rotate(readCommand.payload);
      stepper2.disable();
      break;
      case END:
      state = DONE;
      digitalWrite(PC13, LOW);
      digitalWrite(statusPin, LOW);
      break;
    }
    break;
  }
  
}

