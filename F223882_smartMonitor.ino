//Importing libraries to be used
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <EEPROM.h>

//Custom design for upwards arrow for UDCHARS extension task
byte customUpArrow[] = {
  B00100,
  B01110,
  B11111,
  B10101,
  B00100,
  B00100,
  B00100,
  B00000
};

//Custom design for downwards arrow for UDCHARS extension task
byte customDownArrow[] = {
  B00000,
  B00100,
  B00100,
  B00100,
  B10101,
  B11111,
  B01110,
  B00100
};

/*Declaring global variables that can be accessed throughout the programme*/
byte state = 0; //Tracks the main state of the programme from synchronisation to reading data, to taking in responses
byte substate = 0; //Tracks the substate of the programme, mainly used for the HCI extension

//Tracks the position of the cursor to print the next Q while looping through synchronisation state
int posX = 0; 
int posY = 0;

//Tracks the index of the device in the sorted array on display
int deviceArrayAddress = 0;
//Tracks the index of the last device in the sorted array
int deviceArrayEndAddress = 0;
//Tracks the index of the last occupied byte in EEPROM
int eepromAddress = 0;

//Arrow variables that are changed according to the situation, to be displayed
char upArrow;
char downArrow;

//Testing variable to tell if the next byte in EEPROM is available or not
float testFloat;

//Activating LCD for monitor and buttons
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

//Structure declaration for each device
struct Device {
  char id[4];
  char location[16];
  char deviceType;
  bool state; //State attribute is stored as boolean, which would then be processing in the display function to display the corresponding character array
  byte add1; //add1 represent power value for Speaker and Light devices, temperature for Thermostat devices, and null value for Light and Camera devices
};

//Initializing an empty array of devices to be populated in the reading state
Device deviceArray[20];

void setup() {
  //Standard procedure
  Serial.begin(9600);
  lcd.begin(16,2);

  //Creating the custom characters for the upwards and downwards arrows
  lcd.createChar(0, customUpArrow);
  lcd.createChar(1, customDownArrow);
}

//Function to display the device being pointed at in the right format
void lcdDisplay(Device device) {
  //Decider for which arrows are needed
  //When there is only one device
  if ((deviceArrayAddress == 0) && (deviceArrayAddress == deviceArrayEndAddress - 1)) {
    upArrow = ' ';
    downArrow = ' ';
  //When the pointer points at the first device in the array
  } else if (deviceArrayAddress == 0) {
    upArrow = ' ';
    downArrow = 1;
  //When the pointer points at the last device in the array
  } else if (deviceArrayAddress == deviceArrayEndAddress - 1) {
    upArrow = 0;
    downArrow = ' ';
  //When there are devices both in front and behind the device being pointed at
  } else {
    upArrow = 0;
    downArrow = 1;
  }

  //Displaying values on the LCD screen, inserting values at the correct position using the cursor
  lcd.home();
  lcd.write(upArrow);
  lcd.setCursor(1,0);
  lcd.print(device.id);
  lcd.setCursor(5,0);
  lcd.print(device.location);
  lcd.setCursor(0,1);
  lcd.write(downArrow);
  lcd.setCursor(1,1);
  lcd.print(device.deviceType);
  lcd.setCursor(3,1);

  //To decide which character string to display according to the boolean state value
  if (device.state == true) {
    lcd.print(" ON");
    lcd.setBacklight(2); //Setting the background light to green for ON state
  } else {
    lcd.print("OFF");
    lcd.setBacklight(3); //Vice versa to yellow for OFF state
  }

  //Displaying different values of power value for different devices
  lcd.setCursor(7,1);
  if (device.deviceType == 'S' || device.deviceType == 'L') {
    if (device.add1 < 10) {
      lcd.print("  " + (String) device.add1 + "%");
    } else if (device.add1 < 100) {
      lcd.print(" " + (String) device.add1 + "%");
    } else {
      lcd.print("100%");
    }
  } else if (device.deviceType == 'T'){
    if (device.add1 == 9) {
      lcd.print((String)" 9" + char(0xDF) + "C");
    } else if (device.add1 < 32) {
      lcd.print((String) device.add1 + char(0xDF) + "C");
    }
  }
}

//Function to iterate through the array of devices and sort them in algebraic order to be displayed
void sortDevices() {
  Device tempDevice; //Temporary value holding device

  //Implementing bubble sort
  bool swaps = false; 
  do {
    swaps = false;

    //Iterating through the device array
    for (int i = 0; i < deviceArrayEndAddress - 1; i++) {
      //Iterating through each character of the devices' IDs and comparing them
      for (int j = 0; j < 3; j++) {
        if (deviceArray[i].id[j] > deviceArray[i+1].id[j]) {
          swaps = true;
          //Swapping the two devices' positions if their order is wrong
          tempDevice = deviceArray[i];
          deviceArray[i] = deviceArray[i+1];
          deviceArray[i+1] = tempDevice;
          break;
        } 
      }
    }
  } while (swaps == true);
}

//Function to return the amount of SRAM left, used for the SRAM extension
int freeRam() {
  extern int __heap_start,*__brkval;
  int v;
  return (int)&v - (__brkval == 0  
    ? (int)&__heap_start : (int) __brkval);  
}

void loop() {
  //Main finite state machine
  switch (state) {
    case 0: //State 0 is synchronisation
      lcd.setBacklight(5); //Setting background color to purple as stated in the specifications

      //To recognize when the Q's got to the end of one row and needs to be repositioned to the next row
      if (posX > 15 && posY == 0) {
        posX = 0;
        posY = 1;
      } else if (posX > 15 && posY == 1){
        lcd.clear();
        posX = 0;
        posY = 0;
      }
      lcd.setCursor(posX,posY);
      lcd.print("Q");
      delay(1000);

      //To detect user input from the Serial Monitor
      if (Serial.available() > 0) {
        String signal = Serial.readString(); //Storing the synchronisation signal
        signal.trim();

        //If signal is correct and recognized
        if (signal == "X") {

          //Displaying the extension tasks that have been undertaken
          lcd.clear();
          lcd.home();
          lcd.print("UDCHARS,FREERAM,");
          lcd.setCursor(2,1);
          lcd.print("HCI,EEPROM");
          lcd.setBacklight(7);
          for (int i = 3; i > 0; i--) {
            delay(1000);
          }
          lcd.clear();
          lcd.setCursor(6,1);
          lcd.print("START");
          delay(1000);
          lcd.clear();
          state = 1; //Moving onto the next state if the signal is recognized
          break;
        }
      }
      //If signal has not been sent or had not been recognized, position of the cursor is adjusted and loops again
      posX += 1;
      break;
    case 1: //State 1 retrieves the devices from EEPROM to an array

      //Using the testFloat variable to see if there are any devices in EEPROM
      EEPROM.get(eepromAddress, testFloat);
      //If EEPROM is empty
      if ((testFloat == NULL) && (eepromAddress == 0)) {
        lcd.setBacklight(7);
        Serial.println("NOTICE: There are no devices stored in EEPROM.");
        lcd.setCursor(0,0);
        lcd.print(" XXX XXXXXXXXXXXXXXX");
        lcd.setCursor(0,1);
        lcd.print(" X XXX XXXX");
        lcd.clear();
        state = 2; // Move onto state 2 to take in user inputs 
      } else {
        //As eepromAddress keeps track where the last device is stored in EEPROM,
        //If it is zero and this device space is not empty, then that means the pointer is still at the start of EEPROM, this device would have to be added to the device array
        //If it is not zero and this device space is not empty, then that means the pointer is still somewhere midway through EEPROM, this device would have to be added to the device array
        //If it is not zero and this device space is empty, then that means the pointer has reached the end of EEPROM, and so we can move onto state 2
        if (testFloat == NULL) {
          state = 2;
        } else {
          Device currentDevice;
          EEPROM.get(eepromAddress, currentDevice);
          eepromAddress += sizeof(Device); //Iterating through EEPROM using eepromAddress

          //Lesser finite state machine for HCI extension
          //State 0 represent normal displays
          //State 1 represent displays for only ON state devices
          //State 2 represent displays for only OFF state devices
          switch (substate) {
            case 0:
              deviceArray[deviceArrayEndAddress] = currentDevice; //Inserting device on iteration into device array
              deviceArrayEndAddress += 1;
              break;
            case 1:
              //Only inserting devices with ON states
              if (currentDevice.state == true) {
                deviceArray[deviceArrayEndAddress] = currentDevice;
              deviceArrayEndAddress += 1;
              }
              break;
            case 2:
              //Only inserting devices with OFF states
              if (currentDevice.state == false) {
                deviceArray[deviceArrayEndAddress] = currentDevice;
              deviceArrayEndAddress += 1;
              }
              break;
          }
        }
        //Sort device array into algebraic order no matter what array it is
        sortDevices();
      }
      break;
    case 2: //State 2 calls the main phase function which handles user inputs
      if (deviceArrayEndAddress == 0) {
        //Finite substate machine for HCI extension
        //If there are no devices in the device array
        switch (substate) {
          case 0:
            lcd.setBacklight(7);
            lcd.setCursor(0,0);
            lcd.print(" XXX XXXXXXXXXXXXXXX");
            lcd.setCursor(0,1);
            lcd.print(" X XXX XXXX");
            break;
          case 1:
            lcd.setBacklight(2);
            lcd.setCursor(2,0);
            lcd.print("NOTHING'S ON");
            break;
          case 2: 
            lcd.setBacklight(3);
            lcd.setCursor(2,0);
            lcd.print("NOTHING's OFF");
            break;
        }
      } else {
        //Display the device being pointed at on the LCD screen
        lcdDisplay(deviceArray[deviceArrayAddress]);
      }
      //Call to main phase to handle user inputs
      mainPhase();
      break;
  }
}

//Function to handle user inputs, firstly buttons and then text inputs through the serial monitor
void mainPhase() {
  //Variable to label which button is being pressed
  uint8_t pressedButtons = lcd.readButtons();

  //Button input handler
  switch(pressedButtons) {
    case BUTTON_UP:
      lcd.clear();
      //Check if the pointer is pointed at the first device of the device array
      if (deviceArrayAddress != 0) {
        //Move the display pointer back up along the device array
        deviceArrayAddress -= 1;
      }
      delay(300); //Delay to allow humans to perceive the change on the LCD screen
      break;
    case BUTTON_DOWN:
      lcd.clear();
      //Check if the pointer is pointed at the last device of the device array
      if (deviceArrayAddress != deviceArrayEndAddress - 1) {
        //Move the display pointer down along the device array
        deviceArrayAddress += 1;
      }
      delay(300);
      break;
    case BUTTON_RIGHT:
      lcd.clear();
      //Reset all counters as the device array would have to be re-retrieved
      deviceArrayAddress = 0;
      deviceArrayEndAddress = 0;
      eepromAddress = 0;
      //Changing substates from normal to ON state devices only to satisfy HCI extension
      if (substate == 0) {
        substate = 1;
      //If already in ON state devices only, then it would switch back to normal substate
      } else {
        substate = 0;
      }
      //Calling back to state 1 to read data
      state = 1;
      delay(300);
      break;
    case BUTTON_LEFT:
      lcd.clear();
      //Reset all counters as the device array would have to be re-retrieved
      deviceArrayAddress = 0;
      deviceArrayEndAddress = 0;
      eepromAddress = 0;
      //Changing substates from normal to OFF state devices only to satisfy HCI extension
      if (substate == 0) {
        substate = 2;
      //If already in OFF state devices only, then it would switch back to normal substate
      } else {
        substate = 0;
      }
      //Calling back to state 1 to read data
      state = 1;
      delay(300);
      break;
    case BUTTON_SELECT:
      //To show my student ID when select button is pressed
      lcd.clear();
      lcd.setBacklight(5);
      lcd.setCursor(4,0);
      lcd.print("F223882");
      //To show the remaining SRAM available 
      lcd.setCursor(0,1);
      lcd.print("SRAM: ");
      lcd.print(freeRam());
      lcd.print("B left");
      delay(1000);
      lcd.clear();
      break;
  }
  //Serial monitor text input handlers
  if (Serial.available() > 0) {
    String command = Serial.readString();
    command.trim();
    //Determining which action the user is requesting
    //Adding devices
    if (command.startsWith("A-")) {
      //Validation for the right formatting
      if ((command[5] == '-') && (command[7] == '-')) {
        //Initializing a temporary device to store the inputted information
        Device newDevice;
        //Setting ID for the temp device
        command.substring(2,5).toCharArray(newDevice.id, 4);
        //Setting location for the temp device
        command.substring(8).toCharArray(newDevice.location, command.substring(8).length() + 1);
        //Validation for location field
        if (newDevice.location == NULL) {
          Serial.println("ERROR: Please enter a location for your device.");
          return;
        }
        //Setting device type for the temp device
        newDevice.deviceType = command[6];
        //Setting state for the temp device
        newDevice.state = false;
        //Setting additional field for the temp device depending on the device type
        if (newDevice.deviceType == 'S' || newDevice.deviceType == 'L'){
          newDevice.add1 = 50;
        } else if (newDevice.deviceType == 'T') {
          newDevice.add1 = 18;
        } else if (newDevice.deviceType == 'O' || newDevice.deviceType == 'C') {
          newDevice.add1 = NULL;
        } else {
          Serial.println("ERROR: Please enter correct device type.");
          return;
        }

        //Checking if a device in the device array has the same device ID as the user input
        bool alreadyExist = true;
        if (deviceArrayEndAddress == 0) {
          alreadyExist = false;
        } else {
          for (int i = 0; i < deviceArrayEndAddress; i++) {
            for (int j = 0; j < 3; j++) {
              if (deviceArray[i].id[j] != newDevice.id[j]) {
                alreadyExist = false;
                break;
              } 
            }
          }
        }
        //Checking if the maximum capacity of the device array has been reached
        if (alreadyExist == false) {
            if (deviceArrayEndAddress*sizeof(Device) ==  sizeof(deviceArray)) {
              Serial.println("ERROR: Maximum device capacity reached. Unable to add new device.");
              state = 1;
            } else {
              //Adding new device to EEPROM 
              EEPROM.put(eepromAddress, newDevice);
              //Resetting all pointers to read from EEPROM again
              eepromAddress = 0;
              deviceArrayEndAddress = 0;
              deviceArrayAddress = 0;
              state = 1;
            }
        } else {
          Serial.println("ERROR: Device ID is already occupied.");
        }
      } else {
        Serial.println("ERROR: followed by the none conforming line string");
      }
      
    //Changing state of the specified device
    } else if (command.startsWith("S-")) {
      if (command[5] == '-') {
        bool found = false;
        //Initializing a temporary device
        Device findingDevice;
        //To find the device of which state the user is looking to change, iterating through device array
        for (int i = 0; i < sizeof(Device) * deviceArrayEndAddress; i += sizeof(Device)) {
          EEPROM.get(i, findingDevice);
          //Comparing IDs character by character
          if ((findingDevice.id[0] == command.substring(2,5)[0]) && (findingDevice.id[1] == command.substring(2,5)[1]) && (findingDevice.id[2] == command.substring(2,5)[2])) {
            found = true;
            //Setting state attribute on temporary device
            if (command.substring(6) == "ON") {
              findingDevice.state = true;
            } else if (command.substring(6) == "OFF") {
              findingDevice.state = false;
            } else {
              Serial.println("ERROR: Please enter a correct state.");
            }
            //EEPROM.put only writes the device to that location when it is different than what is already there
            EEPROM.put(i, findingDevice);
          }
        }
        //Catch for when a device cannot be found
        if (found == false) {
          Serial.println("ERROR: Device Not Found.");
        } else {
          //Reset pointers to read from EEPROM again
          eepromAddress = 0;
          deviceArrayEndAddress = 0;
          state = 1;
        }
      } else {
        Serial.println("ERROR: followed by the none conforming line string");
      }
    //Changing power values for specified device
    } else if (command.startsWith("P-")) {
      if (command[5] == '-') {
        //Validation for power value, making sure it exists
        if (command.substring(6) == NULL) {
          Serial.println("ERROR: Please enter a power value.");
          return;
        }
        //Saving power value to variable
        byte add = command.substring(6).toInt();
        //Iterating through device array to find specified device
        bool found = false;
        Device findingDevice;
        for (int i = 0; i < sizeof(Device) * deviceArrayEndAddress; i += sizeof(Device)) {
          EEPROM.get(i, findingDevice);
          if ((findingDevice.id[0] == command.substring(2,5)[0]) && (findingDevice.id[1] == command.substring(2,5)[1]) && (findingDevice.id[2] == command.substring(2,5)[2])) {
            found = true;
            //Validation for power value, making sure it is in range 
            if ((findingDevice.deviceType == 'S') || (findingDevice.deviceType == 'L')) {
              if ((add >= 0)&& (add <= 100)) {
                findingDevice.add1 = add;
              } else {
                Serial.println("ERROR: Please keep input value between 0 and 100 for this device.");
              }
            } else if (findingDevice.deviceType == 'T') {
                if ((add >= 9)&& (add <= 32)) {
                  findingDevice.add1 = add;
                } else {
                  Serial.println("ERROR: Please keep input value between 9 and 32 for this device.");
                }
            } else {
              Serial.println("ERROR: This device does not support power output.");
            }
            EEPROM.put(i, findingDevice);
          }
        }
        if (found == false) {
          Serial.println("ERROR: Device Not Found.");
        } else {
          eepromAddress = 0;
          deviceArrayEndAddress = 0;
          state = 1;
        }
      } else {
        Serial.println("ERROR: followed by the none conforming line string");
      }
    //Removes specified device
    } else if (command.startsWith("R-")) {
      //Validation for ID input
      if (command.substring(2) == NULL) {
        Serial.println("ERROR: Please enter an ID value.");
        return;
      } else if (command.substring(2).length() != 3) {
        Serial.println("ERROR: Please enter a valid ID value.");
        return;
      }
      //Iterating through device array to find specified device
      bool found = false;
      Device findingDevice;
      for (int i = 0; i < sizeof(Device) * deviceArrayEndAddress; i += sizeof(Device)) {
        EEPROM.get(i, findingDevice);
        if ((findingDevice.id[0] == command.substring(2)[0]) && (findingDevice.id[1] == command.substring(2)[1]) && (findingDevice.id[2] == command.substring(2)[2])) {
          found = true;
          Device tempDevice;
          //Iterate through EEPROM from the byte at the start of the device removed
          do {
            //Get the next device in line store in EEPROM
            EEPROM.get(i + sizeof(Device), tempDevice);
            //Erase the next device in line in EEPROM
            for (int z = i + sizeof(Device); z < i + sizeof(Device) + sizeof(Device); z++) {
              EEPROM.write(z, 0);
            }
            //Put the next device in line into the device that was removed
            EEPROM.put(i, tempDevice);
            //testFloat to see if the next next device in line exists, or had the pointer reached the end of the device array
            EEPROM.get(i + sizeof(Device), testFloat);
          } while (testFloat != NULL);
          //Essentially, as I am iterating through EEPROM to load them into a device array
          //A gap in between two device would not work very well
          //Which is why EEPROM is treated as a stack
        }
      }
      //Catch for when specified ID doesn't exist
      if (found == false) {
        Serial.println("ERROR: Device Not Found.");
      } else {
        eepromAddress = 0;
        deviceArrayEndAddress = 0;
        deviceArrayAddress = 0;
        state = 1;
      }
    } else {
      Serial.println("ERROR: followed by the none conforming line string");
    }
  }
}




