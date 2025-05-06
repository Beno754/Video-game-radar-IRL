/*

SAMPLE PROGRAMS USED FOR DIGITAL COMPASS
-Too much for me to understand 3D orientation.

This sketch takes data from a bluetooth connection,
Parses and stores the data as a client
Uses a gyroscope/digital compass to orientate North.
Illuminates LEDs in a ring based on polar co-ordinates in client data
Which ring of LEDs to reference is based on the distance value.
Smaller distance, closer to center ring is selected.
*/



//Command formats:
//£-U-a-90-r-25-c-FF0000-,-*
//£-U-a-90-r-12-c-FF0000-,-U-a-180-r-1-c-FF00FF-,-*
//£-S-5-*

// Local user values (for blip display)
class User {

public:
  int Radius = 0, Angle = 0;
  byte Red = 255, Green = 0, Blue = 0;

  void Hex2Col(String hex) {
    ////Serial.println("---COLOUR---");
    char arr[2];

    arr[0] = hex[0];
    arr[1] = hex[1];
    Red = (int)strtol(arr, NULL, 16);
    ////Serial.println(Red);

    arr[0] = hex[2];
    arr[1] = hex[3];
    Green = (int)strtol(arr, NULL, 16);
    ////Serial.println(Green);

    arr[0] = hex[4];
    arr[1] = hex[5];
    Blue = (int)strtol(arr, NULL, 16);
    ////Serial.println(Blue);


    ////Serial.println("---END---");
  }
};


// ----- Libraries
#include <Wire.h>



// ----- Flags
bool Gyro_synchronised = false;
bool Flag = false;

// ----- Debug
//#define Switch A0                       // Connect an SPST switch between A0 and GND to enable/disable tilt stabilazation
bool stablize = false;
long Loop_start_time;




// ----- BT
#include <SoftwareSerial.h>
int bluetoothTx = 5;                                 // TX-O pin of bluetooth mate, Arduino d5
int bluetoothRx = 6;                                 // RX-I pin of bluetooth mate, Arduino d6
SoftwareSerial bluetooth(bluetoothTx, bluetoothRx);  //Uses GPIO for communication with BT device

//how much users we want to use
const byte maxUsers = 7;
//list of users
User us[6];





// VARIABLES FOR PARSING INCOMING DATA
//  £-r-5-a-0-cFFFFFF-,-r-15-a-90-,-r-25-a-180-,-r-25-a-270-,-*
const byte startData = '£'; // Start of data token

const byte startUser = 'U'; // User Token
const byte startR = 'r'; // Radius Token
const byte startA = 'a'; // Angle Token
const byte startC = 'c'; // Colour Token
const byte startS = 'S'; // Resolution Token
const byte endUser = ','; // Group Separater Token

const byte endData = '*'; // End of data token
const byte readBufferSize = 255;

// Incoming data buffer
char buff[readBufferSize];
int buffIndex = -1;  //index of the read buffer
int curUser = 0;     // read user counts (every time we see a "," we add a new user)

// Data read flags
bool dataStarted = false;
bool dataDone = false;






////// LED RING
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

#define PIN 7  // Use pin 7 for LED data bus

// Short hand variables
#define RED strip.Color(255, 0, 0)
#define GREEN strip.Color(0, 255, 0)
#define BLUE strip.Color(0, 0, 255)
#define YELLOW strip.Color(255, 242, 0)
//#define PURPLE strip.Color(255, 0, 255)
//#define WHITE strip.Color(255, 255, 255)

Adafruit_NeoPixel strip = Adafruit_NeoPixel(37, PIN, NEO_GRB + NEO_KHZ800);  // Init leds (37 in sequence)
//ring sizes
#define outerRingSize 16
#define midRingSize 12
#define innerRingSize 8

//led ring indexes
byte outerRing[outerRingSize];
byte midRing[midRingSize];
byte innerRing[innerRingSize];
byte centerRing;


int angleOffset = -5;  // Offset the angle from being directly over LED (+- 2.5 degrees each side)
int ringRes = 10;      // How much meters distance before the next outer ring is used

//connection state (for indication and debuggig)
enum con_state { DisCon,
                 LimCon,
                 GoodCon };
con_state thisCon;

//used for limited fading animation
int r = 255;
int g = 242;
bool fading = true;




//angle from North
float compassOffset = 0;  // Updated based on compass
//store array selection
byte *thisRing;
int thisRingCount;
//////






// Init the first user as the North Pole (To help orientate user)
void showNorth() {
  us[0].Angle = 0;     // Relative North
  us[0].Radius = 255;  // Maximum range
  us[0].Red = 255;
  us[0].Green = 242;
  us[0].Blue = 0;
}





// Setup initial bluetooth device on startup
void BTsetup() {
  bluetooth.begin(115200);  // The Bluetooth Mate defaults to 115200bps
  bluetooth.print("$");     // Print three times individually
  bluetooth.print("$");
  bluetooth.print("$");           // Enter command mode
  delay(100);                     // Short delay, wait for the Mate to send back CMD
  bluetooth.println("U,9600,N");  // Temporarily Change the baudrate to 9600, no parity
  // 115200 can be too fast at times for NewSoftSerial to relay the data reliably
  bluetooth.begin(9600);  // Start bluetooth serial at 9600


  delay(1000);
  bluetooth.println("Press A to continue");

  //break only when we recieve 'A'
  while (true) {
    //If using BT
    if (bluetooth.available()) {
      if (bluetooth.read() == 'A') {
        //Serial.read();
        break;
      }
    }
    //If using PC
    if (Serial.available()) {
      if (Serial.read() == 'A') {
        break;
      }
    }
  }
}


// Initialise LED ring variables
// Sequentially add the index to each array
void ledSetup() {
  //global loop counter
  int n = 0;
  //create reference to each LED
  for (int i = 0; i < outerRingSize; i++) {
    outerRing[i] = n;
    n++;
  }
  for (int i = 0; i < midRingSize; i++) {
    midRing[i] = n;
    n++;
  }
  for (int i = 0; i < innerRingSize; i++) {
    innerRing[i] = n;
    n++;
  }
  centerRing = n;

  //Strip
  strip.begin();
  strip.setBrightness(10);
  strip.show();  // Initialize all pixels to 'off'
  //connection state
  thisCon = GoodCon;
}


// Initial code for gyroscope
void gyroSetup() {
  //Serial.println("Start gyroSetup");

  // ----- Configure the magnetometer
  configure_magnetometer();
  //Serial.println("After configure_magnetometer");

  /* ----- Calibrate the magnetometer
    Calibrate only needs to be done occasionally.
    Enter the magnetometer values into the "header"
    then set "Record_data = true". */
  if (Record_data == true) {
    calibrate_magnetometer();
  }
  //Serial.println("After calib_mag");



  // ----- Configure the gyro & magnetometer
  config_gyro();
  //Serial.println("Start moving gyro around");
  calibrate_gyro();
  //Serial.println("After calib_gyro");


  Loop_start_time = micros();  // Controls the Gyro refresh rate
}













//  Arduino Initial Entry
void setup() {
  // ----- Serial communication
  //Serial.begin(9600);
  //Serial.println("v6");


  ledSetup();                    // Setup LED strip
  setLedCol(centerRing, GREEN);  // Turn center LED green for asthetics


  BTsetup();  // BT Setup


  // I2C devices
  Wire.begin();  //Start I2C as master
  Wire.setClock(400000);

  // Print to BT device to know Gyro calibration can happen
  bluetooth.println("Start Gyro Setup");
  // Setup Gyro
  gyroSetup();
  bluetooth.println("Done Gyro");

  // Update connection status LED
  thisCon = GoodCon;
  ConCheck();


  //Serial.println("Done Setup!");
}


// Main Arduino Loop
void loop() {

  // Do Gyro Sub routine
  LoopGyro();

  // Do LED Sub routine
  ledUpdate();

  // BT Loop
  parseIncoming();


  // ----- Loop control
  /*
     Adjust the loop count for a yaw reading of 360 degrees
     when the MPU-9250 is rotated exactly 360 degrees.
     (Compensates for any 16 MHz Xtal oscillator error)
  */
  while ((micros() - Loop_start_time) < 8000)
    ;
  Loop_start_time = micros();

}  // Loop












// Command Format: £-U-a-90-r-12-c-00FF00-,-*
void parseIncoming() {

  if (bluetooth.available())  // If data incoming from bluetooth device
  {
    //read character
    byte in = bluetooth.read();
    //Serial.print(in); //Serial.print("->"); //Serial.println(endData);

    // Compare to starting data character
    if (in == startData) {
      if (dataStarted) return;  //Start data recorded twice, ignore

      //Serial.println("Matched start");
      dataStarted = true;  // Set reading flag
      buffIndex = 0;       // Reset read index buffer

    } else if (in == endData) {
      //Compared character to end of command character

      dataStarted = false;
      dataDone = true;
      buff[buffIndex] = '\0';  //terminate string with blank character
      buffIndex = 0;           // reset the index back for looping later

    } else if (dataStarted) {
      //add character to buffer and increment
      buff[buffIndex] = in;
      buffIndex++;
    }
  }

  if (!dataDone) return;  // we only want to continue if data is finished

  //Serial.println("Got Msg OK");
  bluetooth.println("Got Msg:");
  bluetooth.println(buff);
  //Serial.println("Msg:");
  //Serial.println(buff);
  //Serial.println("---");


  // flag to break loop
  bool breakLoop = false;

  //parse command line
  while (buffIndex < readBufferSize) {

    //exit
    if (breakLoop) break;


    //get next chars
    String parsed = parseNextArg();
    byte b = parsed[0];
    //Serial.print("parsed main: "); //Serial.println(parsed);
    //Serial.print(b); //Serial.print("->"); //Serial.println(startUser);


    // Effective switch statements to identify which command to run
    if (b == startUser) {
      //Serial.println("About to deal with user");
      SwitchStartUser();

    } else if (b == endUser) {
      //Serial.println("EndUser");
      SwitchEndUser();

    } else if (b == startS) {
      //Serial.println("Resolution");
      SwitchResolution();

    } else if (b == endData) {
      //Serial.println("EndData");
      breakLoop = true;

    } else if (b == ' ' || b == '\0') {
      //Serial.println("break main empty");
      breakLoop = true;
    }

  }  //while

  //Serial.println("Finished with command string!");
  dataDone = false;
  dataStarted = false;
  curUser = 0;
}



// Return values between control/separation characters
String parseNextArg() {
  int startI = -1;  // Substring start index
  int endI = -1;    // Substring end index

  String retS = "";

  // Continue through inBuffer at global index
  for (int i = buffIndex; i < readBufferSize; i++) {
    buffIndex = i;  //update global buffer Index value

    // Check if this character is '-' or '[0 value]'
    if (buff[i] == '-' || buff[i] == '\0') {
      // If we have not yet started, record index
      if (startI == -1) {
        startI = i;
      } else {
        // We have encountered another control, so focus on data inbetween
        endI = i;
        break;
      }
    } else if (startI != -1)
      // If not a control char, add this to string
      retS += buff[i];
  }

  // Return with string if we have a completed message, return empty otherwise
  if (endI != -1)
    return retS;
  else
    return "";
}




void SwitchStartUser() {
  // Incremenet new user (Ignore first user as this is NORTH)
  curUser++;

  bool breakLoop = false;  // Loop break flag

  while (buffIndex < readBufferSize) {
    //exit
    if (breakLoop) break;


    // get next chars
    String parsed = parseNextArg();  // Parse values from buffer data
    byte b = parsed[0];
    //Serial.print("parsed user: "); //Serial.print(parsed); //Serial.print("->"); //Serial.println(b);

    // Compare first character with Commands:
    if (b == startA) {
      //Serial.println("User - Angle");
      us[curUser].Angle = parseNextArg().toInt();
    }

    else if (b == startR) {
      //Serial.println("User - Radius");
      us[curUser].Radius = parseNextArg().toInt();
    }

    else if (b == startC) {
      //Serial.println("User - Colour");
      us[curUser].Hex2Col(parseNextArg());
    }

    else if (b == endUser) {
      //Serial.println("User - End");
      breakLoop = true;
    }

    else if (b == ' ' || b == '\0') {
      //Serial.println("Breaking No Chars");
      breakLoop = true;
    } else {
      //Serial.println("Breaking Default");
      breakLoop = true;
    }
  }

  //Serial.println("Out of User While loop, Print User:");
  //Serial.print("R"); //Serial.print(us[curUser].Radius);
  //Serial.print("A"); //Serial.println(us[curUser].Angle);

  //Serial.print("R"); //Serial.print(us[curUser].Red);
  //Serial.print("G"); //Serial.print(us[curUser].Green);
  //Serial.print("B"); //Serial.println(us[curUser].Blue);
}




// Update distance between users per ring.
// How many meters away before the blip moves to an outer ring layer (to visually see distance)
void SwitchResolution() {
  ringRes = parseNextArg().toInt();
}


void SwitchEndUser() {

}














// Handle all LED functions
void ledUpdate() {
  DrawBlips();
  ConCheck();
  //delay(100);
  strip.show();
}











/// Draw the blips on the LED ring
void DrawBlips() {

  //default all states
  strip.clear();

  // SET NORTH
  showNorth();

  // Draw a blip for each user saved
  for (int i = 0; i < maxUsers; i++) {

    // break if empty array
    if (us[i].Angle == 0 && us[i].Radius == 0) {
      break;
    }

    float finAngle = Heading + us[i].Angle;  // Draw blip based on Polar Angle, then add North offset

    // Keep in bounds (0-360)
    if (finAngle > 360) finAngle -= 360;
    if (finAngle < 0) finAngle = 360 - finAngle;


    SelectRing(i);  //assign ring based on user distance


    float segAngles = (float)360 / thisRingCount;                                   // Angles between leds
    int thisSeg = (int)((finAngle - angleOffset) / segAngles);                      // how many segAngles have passed, this will be the led index we need
    setLedCol(thisRing[thisSeg], strip.Color(us[i].Red, us[i].Green, us[i].Blue));  // Highlight this segment


  }  // for
}



// Choose which array of LEDs to use
void SelectRing(int i) {

  // select ring based on magnitude of distance resolution per ring
  if (us[i].Radius < ringRes) {  // Within 1 x distance
    thisRing = innerRing;
    thisRingCount = innerRingSize;

  } else if (us[i].Radius < (ringRes * 2)) {  // within 2 x distance
    thisRing = midRing;
    thisRingCount = midRingSize;

  } else {  // further away than 2 x distance
    thisRing = outerRing;
    thisRingCount = outerRingSize;
  }
}


// Update center LED colour based on connection status
void ConCheck() {
  switch (thisCon) {
    case DisCon:
      setLedCol(centerRing, RED);
      break;
    case LimCon:
      if (fading) {
        if (g > 0) {
          strip.setPixelColor(centerRing, r, g, 0);
          r -= 1;
          g -= 1;
        } else {
          fading = false;
        }
      } else {
        if (g < 242) {
          strip.setPixelColor(centerRing, r, g, 0);
          r += 1;
          g += 1;
        } else {
          fading = true;
        }
      }
      break;
    case GoodCon:
      setLedCol(centerRing, BLUE);
      break;
  }
}

// Sets an LED by index a colour value
void setLedCol(int id, uint32_t col) {
  strip.setPixelColor(id, col);
  //strip.show();
}








// COPIED PROGRAM BELOW











// ----- Gyro
#define MPU9250_I2C_address 0x68                  // I2C address for MPU9250
#define MPU9250_I2C_master_enable 0x6A            // USER_CTRL[5] = I2C_MST_EN
#define MPU9250_Interface_bypass_mux_enable 0x37  // INT_PIN_CFG[1]= BYPASS_EN

#define Frequency 125     // 8mS sample interval
#define Sensitivity 65.5  // Gyro sensitivity (see data sheet)

#define Sensor_to_deg 1 / (Sensitivity * Frequency)  // Convert sensor reading to degrees
#define Sensor_to_rad Sensor_to_deg *DEG_TO_RAD      // Convert sensor reading to radians

#define Loop_time 1000000 / Frequency  // Loop time (uS)
long Loop_start;
// Loop start time (uS)

int Gyro_x, Gyro_y, Gyro_z;
long Gyro_x_cal, Gyro_y_cal, Gyro_z_cal;
float Gyro_pitch, Gyro_roll, Gyro_yaw;
float Gyro_pitch_output, Gyro_roll_output;

// ----- Accelerometer
long Accel_x, Accel_y, Accel_z, Accel_total_vector;
float Accel_pitch, Accel_roll;

// ----- Magnetometer
#define AK8963_I2C_address 0x0C            // I2C address for AK8963
#define AK8963_cntrl_reg_1 0x0A            // CNTL[4]=#bits, CNTL[3:0]=mode
#define AK8963_status_reg_1 0x02           // ST1[0]=data ready
#define AK8963_data_ready_mask 0b00000001  // Data ready mask
#define AK8963_overflow_mask 0b00001000    // Magnetic sensor overflow mask
#define AK8963_data 0x03                   // Start address of XYZ data
#define AK8963_fuse_ROM 0x10               // X,Y,Z fuse ROM

// ----- Compass heading
/*
  The magnetic declination for Lower Hutt, New Zealand is +22.5833 degrees
  Obtain your magnetic declination from http://www.magnetic-declination.com/
  Uncomment the declination code within the main loop() if you want True North.
*/
float Declination = -0.6166667;  //  Degrees ... replace this declination with yours
int Heading;

int Mag_x, Mag_y, Mag_z;  // Raw magnetometer readings
float Mag_x_dampened, Mag_y_dampened, Mag_z_dampened;
float Mag_x_hor, Mag_y_hor;
float Mag_pitch, Mag_roll;

// ----- Record compass offsets, scale factors, & ASA values
/*
   These values seldom change ... an occasional check is sufficient
   (1) Open your Arduino "Serial Monitor
   (2) Set "Record_data=true;" then upload & run program.
   (3) Replace the values below with the values that appear on the Serial Monitor.
   (4) Set "Record_data = false;" then upload & rerun program.
*/
bool Record_data = false;
int Mag_x_offset = 92, Mag_y_offset = 106, Mag_z_offset = -120;    // Hard-iron offsets
float Mag_x_scale = 0.98, Mag_y_scale = 0.97, Mag_z_scale = 1.06;  // Soft-iron scale factors
float ASAX = 1.18, ASAY = 1.18, ASAZ = 1.14;                       // (A)sahi (S)ensitivity (A)djustment fuse ROM values.

/*
   Calibrating
  XYZ Max/Min: -345 213 -323  353 692 1280

  Hard-iron: -66  15  986

  Soft-iron: 1.09 0.90  1.03

  ASA: 1.18 1.18  1.14
*/






void LoopGyro() {

  //////////////
  //        PITCH & ROLL CALCULATIONS       //
  //////////////

  /*
     --------------------
     MPU-9250 Orientation
     --------------------
     Component side up
     X-axis facing forward
  */

  // ----- read the raw accelerometer and gyro data
  read_mpu_6050_data();  // Read the raw acc and gyro data from the MPU-6050


  // ----- Adjust for offsets
  Gyro_x -= Gyro_x_cal;  // Subtract the offset from the raw gyro_x value
  Gyro_y -= Gyro_y_cal;  // Subtract the offset from the raw gyro_y value
  Gyro_z -= Gyro_z_cal;  // Subtract the offset from the raw gyro_z value



  // ----- Calculate travelled angles
  /*
    ---------------------------
    Adjust Gyro_xyz signs for:
    ---------------------------
    Pitch (Nose - up) = +ve reading
    Roll (Right - wing down) = +ve reading
    Yaw (Clock - wise rotation)  = +ve reading
  */
  Gyro_pitch += -Gyro_y * Sensor_to_deg;  // Integrate the raw Gyro_y readings
  Gyro_roll += Gyro_x * Sensor_to_deg;    // Integrate the raw Gyro_x readings
  Gyro_yaw += -Gyro_z * Sensor_to_deg;    // Integrate the raw Gyro_x readings

  // ----- Compensate pitch and roll for gyro yaw
  Gyro_pitch += Gyro_roll * sin(Gyro_z * Sensor_to_rad);  // Transfer the roll angle to the pitch angle if the Z-axis has yawed
  Gyro_roll -= Gyro_pitch * sin(Gyro_z * Sensor_to_rad);  // Transfer the pitch angle to the roll angle if the Z-axis has yawed


  // ----- Accelerometer angle calculations
  Accel_total_vector = sqrt((Accel_x * Accel_x) + (Accel_y * Accel_y) + (Accel_z * Accel_z));  // Calculate the total (3D) vector
  Accel_pitch = asin((float)Accel_x / Accel_total_vector) * RAD_TO_DEG;                        //Calculate the pitch angle
  Accel_roll = asin((float)Accel_y / Accel_total_vector) * RAD_TO_DEG;                         //Calculate the roll angle

  // ----- Zero any residual accelerometer readings
  /*
     Place the accelerometer on a level surface
     Adjust the following two values until the pitch and roll readings are zero
  */
  Accel_pitch -= -0.2f;  //Accelerometer calibration value for pitch
  Accel_roll -= 1.1f;    //Accelerometer calibration value for roll

  // ----- Correct for any gyro drift
  if (Gyro_synchronised) {
    // ----- Gyro & accel have been synchronised
    Gyro_pitch = Gyro_pitch * 0.9996 + Accel_pitch * 0.0004;  //Correct the drift of the gyro pitch angle with the accelerometer pitch angle
    Gyro_roll = Gyro_roll * 0.9996 + Accel_roll * 0.0004;     //Correct the drift of the gyro roll angle with the accelerometer roll angle
  } else {
    // ----- Synchronise gyro & accel
    Gyro_pitch = Accel_pitch;  //Set the gyro pitch angle equal to the accelerometer pitch angle
    Gyro_roll = Accel_roll;    //Set the gyro roll angle equal to the accelerometer roll angle
    Gyro_synchronised = true;  //Set the IMU started flag
  }



  // ----- Dampen the pitch and roll angles
  Gyro_pitch_output = Gyro_pitch_output * 0.9 + Gyro_pitch * 0.1;  //Take 90% of the output pitch value and add 10% of the raw pitch value
  Gyro_roll_output = Gyro_roll_output * 0.9 + Gyro_roll * 0.1;     //Take 90% of the output roll value and add 10% of the raw roll value

  //////////////
  //        MAGNETOMETER CALCULATIONS       //
  //////////////
  /*
     --------------------------------
     Instructions for first time use
     --------------------------------
     Calibrate the compass for Hard-iron and Soft-iron
     distortion by temporarily setting the header to read
     bool    Record_data = true;

     Turn on your Serial Monitor before uploading the code.

     Slowly tumble the compass in all directions until a
     set of readings appears in the Serial Monitor.

     Copy these values into the appropriate header locations.

     Edit the header to read
     bool    Record_data = false;

     Upload the above code changes to your Arduino.

     This step only needs to be done occasionally as the
     values are reasonably stable.
  */

  // ----- Read the magnetometer
  read_magnetometer();

  // ----- Fix the pitch, roll, & signs
  /*
     MPU-9250 gyro and AK8963 magnetometer XY axes are orientated 90 degrees to each other
     which means that Mag_pitch equates to the Gyro_roll and Mag_roll equates to the Gryro_pitch

     The MPU-9520 and AK8963 Z axes point in opposite directions
     which means that the sign for Mag_pitch must be negative to compensate.
  */
  Mag_pitch = -Gyro_roll_output * DEG_TO_RAD;
  Mag_roll = Gyro_pitch_output * DEG_TO_RAD;

  // ----- Apply the standard tilt formulas
  Mag_x_hor = Mag_x * cos(Mag_pitch) + Mag_y * sin(Mag_roll) * sin(Mag_pitch) - Mag_z * cos(Mag_roll) * sin(Mag_pitch);
  Mag_y_hor = Mag_y * cos(Mag_roll) + Mag_z * sin(Mag_roll);

  // ----- Disable tilt stabization if switch closed
  if (!stablize) {
    // ---- Test equations
    Mag_x_hor = Mag_x;
    Mag_y_hor = Mag_y;
  }

  // ----- Dampen any data fluctuations
  Mag_x_dampened = Mag_x_dampened * 0.9 + Mag_x_hor * 0.1;
  Mag_y_dampened = Mag_y_dampened * 0.9 + Mag_y_hor * 0.1;

  // ----- Calculate the heading
  Heading = atan2(Mag_x_dampened, Mag_y_dampened) * RAD_TO_DEG;  // Magnetic North

  /*
     By convention, declination is positive when magnetic north
     is east of true north, and negative when it is to the west.
  */

  Heading += Declination;  // Geographic North
  if (Heading > 360.0) Heading -= 360.0;
  if (Heading < 0.0) Heading += 360.0;

  // ----- Allow for under/overflow
  if (Heading < 0) Heading += 360;
  if (Heading >= 360) Heading -= 360;

  // ----- Display Heading, Pitch, and Roll
  ////Serial.println(Heading); // PRINT HEADING
}


// -------------------------------
//  Read magnetometer
// -------------------------------
void read_magnetometer() {
  // ----- Locals
  int mag_x, mag_y, mag_z;
  int status_reg_2;

  // ----- Point to status register 1
  Wire.beginTransmission(AK8963_I2C_address);  // Open session with AK8963
  Wire.write(AK8963_status_reg_1);             // Point to ST1[0] status bit
  Wire.endTransmission();
  Wire.requestFrom(AK8963_I2C_address, 1);  // Request 1 data byte
  while (Wire.available() < 1)
    ;                                        // Wait for the data
  if (Wire.read() & AK8963_data_ready_mask)  // Check data ready bit
  {
    // ----- Read data from each axis (LSB,MSB)
    Wire.requestFrom(AK8963_I2C_address, 7);  // Request 7 data bytes
    while (Wire.available() < 7)
      ;                                               // Wait for the data
    mag_x = (Wire.read() | Wire.read() << 8) * ASAX;  // Combine LSB,MSB X-axis, apply ASA corrections
    mag_y = (Wire.read() | Wire.read() << 8) * ASAY;  // Combine LSB,MSB Y-axis, apply ASA corrections
    mag_z = (Wire.read() | Wire.read() << 8) * ASAZ;  // Combine LSB,MSB Z-axis, apply ASA corrections
    status_reg_2 = Wire.read();                       // Read status and signal data read

    // ----- Validate data
    if (!(status_reg_2 & AK8963_overflow_mask))  // Check HOFL flag in ST2[3]
    {
      Mag_x = (mag_x - Mag_x_offset) * Mag_x_scale;
      Mag_y = (mag_y - Mag_y_offset) * Mag_y_scale;
      Mag_z = (mag_z - Mag_z_offset) * Mag_z_scale;
    }
  }
}


// --------------------
//  Read MPU 6050 data
// --------------------
void read_mpu_6050_data() {

  // ----- Locals
  int temperature;  // Needed when reading the MPU-6050 data ... not used

  // ----- Point to data
  Wire.beginTransmission(0x68);  // Start communicating with the MPU-6050
  Wire.write(0x3B);              // Point to start of data
  Wire.endTransmission();        // End the transmission



  // ----- Read the data
  Wire.requestFrom(0x68, 14);  // Request 14 bytes from the MPU-6050
  while (Wire.available() < 14)
    ;  // Wait until all the bytes are received


  Accel_x = Wire.read() << 8 | Wire.read();      // Combine MSB,LSB Accel_x variable
  Accel_y = Wire.read() << 8 | Wire.read();      // Combine MSB,LSB Accel_y variable
  Accel_z = Wire.read() << 8 | Wire.read();      // Combine MSB,LSB Accel_z variable
  temperature = Wire.read() << 8 | Wire.read();  // Combine MSB,LSB temperature variable
  Gyro_x = Wire.read() << 8 | Wire.read();       // Combine MSB,LSB Gyro_x variable
  Gyro_y = Wire.read() << 8 | Wire.read();       // Combine MSB,LSB Gyro_x variable
  Gyro_z = Wire.read() << 8 | Wire.read();       // Combine MSB,LSB Gyro_x variable
}


// ----------------------------
//  Configure magnetometer
// ----------------------------
void configure_magnetometer() {

  //Serial.println("Config Magneto!");
  /*
     The MPU-9250 contains an AK8963 magnetometer and an
     MPU-6050 gyro/accelerometer within the same package.

     To access the AK8963 magnetometer chip The MPU-9250 I2C bus
     must be changed to pass-though mode. To do this we must:
      - disable the MPU-9250 slave I2C and
      - enable the MPU-9250 interface bypass mux
  */
  // ----- Disable MPU9250 I2C master interface
  Wire.beginTransmission(MPU9250_I2C_address);  // Open session with MPU9250
  Wire.write(MPU9250_I2C_master_enable);        // Point USER_CTRL[5] = I2C_MST_EN
  Wire.write(0x00);                             // Disable the I2C master interface
  Wire.endTransmission();

  // ----- Enable MPU9250 interface bypass mux
  Wire.beginTransmission(MPU9250_I2C_address);      // Open session with MPU9250
  Wire.write(MPU9250_Interface_bypass_mux_enable);  // Point to INT_PIN_CFG[1] = BYPASS_EN
  Wire.write(0x02);                                 // Enable the bypass mux
  Wire.endTransmission();

  // ----- Access AK8963 fuse ROM
  /* The factory sensitivity readings for the XYZ axes are stored in a fuse ROM.
     To access this data we must change the AK9863 operating mode.
  */
  Wire.beginTransmission(AK8963_I2C_address);  // Open session with AK8963
  Wire.write(AK8963_cntrl_reg_1);              // CNTL[3:0] mode bits
  Wire.write(0b00011111);                      // Output data=16-bits; Access fuse ROM
  Wire.endTransmission();
  delay(100);  // Wait for mode change

  // ----- Get factory XYZ sensitivity adjustment values from fuse ROM
  /* There is a formula on page 53 of "MPU-9250, Register Map and Decriptions, Revision 1.4":
      Hadj = H*(((ASA-128)*0.5)/128)+1 where
      H    = measurement data output from data register
      ASA  = sensitivity adjustment value (from fuse ROM)
      Hadj = adjusted measurement data (after applying
  */
  Wire.beginTransmission(AK8963_I2C_address);  // Open session with AK8963
  Wire.write(AK8963_fuse_ROM);                 // Point to AK8963 fuse ROM
  Wire.endTransmission();
  Wire.requestFrom(AK8963_I2C_address, 3);  // Request 3 bytes of data
  while (Wire.available() < 3)
    ;                                          // Wait for the data
  ASAX = (Wire.read() - 128) * 0.5 / 128 + 1;  // Adjust data
  ASAY = (Wire.read() - 128) * 0.5 / 128 + 1;
  ASAZ = (Wire.read() - 128) * 0.5 / 128 + 1;

  // ----- Power down AK8963 while the mode is changed
  /*
     This wasn't necessary for the first mode change as the chip was already powered down
  */
  Wire.beginTransmission(AK8963_I2C_address);  // Open session with AK8963
  Wire.write(AK8963_cntrl_reg_1);              // Point to mode control register
  Wire.write(0b00000000);                      // Set mode to power down
  Wire.endTransmission();
  delay(100);  // Wait for mode change

  // ----- Set output to mode 2 (16-bit, 100Hz continuous)
  Wire.beginTransmission(AK8963_I2C_address);  // Open session with AK8963
  Wire.write(AK8963_cntrl_reg_1);              // Point to mode control register
  Wire.write(0b00010110);                      // Output=16-bits; Measurements = 100Hz continuous
  Wire.endTransmission();
  delay(100);  // Wait for mode change
}


// -------------------------------
//  Calibrate magnetometer
// -------------------------------
void calibrate_magnetometer() {
  //Serial.println("Calibrating");

  strip.clear();

  // ----- Locals
  int mag_x, mag_y, mag_z;
  int status_reg_2;  // ST2 status register

  int mag_x_min = 32767;  // Raw data extremes
  int mag_y_min = 32767;
  int mag_z_min = 32767;
  int mag_x_max = -32768;
  int mag_y_max = -32768;
  int mag_z_max = -32768;

  float chord_x, chord_y, chord_z;  // Used for calculating scale factors
  float chord_average;


  int ping = 0;

  // ----- Record min/max XYZ compass readings
  for (int counter = 0; counter < 16000; counter++)  // Run this code 16000 times
  {
    Loop_start = micros();  // Start loop timer

    // ----- Point to status register 1
    Wire.beginTransmission(AK8963_I2C_address);  // Open session with AK8963
    Wire.write(AK8963_status_reg_1);             // Point to ST1[0] status bit
    Wire.endTransmission();
    Wire.requestFrom(AK8963_I2C_address, 1);  // Request 1 data byte
    while (Wire.available() < 1)
      ;                                        // Wait for the data
    if (Wire.read() & AK8963_data_ready_mask)  // Check data ready bit
    {
      // ----- Read data from each axis (LSB,MSB)
      Wire.requestFrom(AK8963_I2C_address, 7);  // Request 7 data bytes
      while (Wire.available() < 7)
        ;                                               // Wait for the data
      mag_x = (Wire.read() | Wire.read() << 8) * ASAX;  // Combine LSB,MSB X-axis, apply ASA corrections
      mag_y = (Wire.read() | Wire.read() << 8) * ASAY;  // Combine LSB,MSB Y-axis, apply ASA corrections
      mag_z = (Wire.read() | Wire.read() << 8) * ASAZ;  // Combine LSB,MSB Z-axis, apply ASA corrections
      status_reg_2 = Wire.read();                       // Read status and signal data read

      // ----- Validate data
      if (!(status_reg_2 & AK8963_overflow_mask))  // Check HOFL flag in ST2[3]
      {
        // ----- Find max/min xyz values
        mag_x_min = min(mag_x, mag_x_min);
        mag_x_max = max(mag_x, mag_x_max);
        mag_y_min = min(mag_y, mag_y_min);
        mag_y_max = max(mag_y, mag_y_max);
        mag_z_min = min(mag_z, mag_z_min);
        mag_z_max = max(mag_z, mag_z_max);
      }
    }
    delay(4);  // Time interval between magnetometer readings
  }

  // ----- Calculate hard-iron offsets
  Mag_x_offset = (mag_x_max + mag_x_min) / 2;  // Get average magnetic bias in counts
  Mag_y_offset = (mag_y_max + mag_y_min) / 2;
  Mag_z_offset = (mag_z_max + mag_z_min) / 2;

  // ----- Calculate soft-iron scale factors
  chord_x = ((float)(mag_x_max - mag_x_min)) / 2;  // Get average max chord length in counts
  chord_y = ((float)(mag_y_max - mag_y_min)) / 2;
  chord_z = ((float)(mag_z_max - mag_z_min)) / 2;

  chord_average = (chord_x + chord_y + chord_z) / 3;  // Calculate average chord length

  Mag_x_scale = chord_average / chord_x;  // Calculate X scale factor
  Mag_y_scale = chord_average / chord_y;  // Calculate Y scale factor
  Mag_z_scale = chord_average / chord_z;  // Calculate Z scale factor

  // ----- Record magnetometer offsets
  /*
     When active this feature sends the magnetometer data
     to the Serial Monitor then halts the program.
  */
  if (Record_data == true) {
    // ----- Display data extremes
    //Serial.print("XYZ Max/Min: ");
    //Serial.print(mag_x_min); //Serial.print("   ");
    //Serial.print(mag_x_max); //Serial.print("   ");
    //Serial.print(mag_y_min); //Serial.print("   ");
    //Serial.print(mag_y_max); //Serial.print("   ");
    //Serial.print(mag_z_min); //Serial.print("   ");
    //Serial.println(mag_z_max);
    //Serial.println("");

    // ----- Display hard-iron offsets
    //Serial.print("Hard-iron: ");
    //Serial.print(Mag_x_offset); //Serial.print("   ");
    //Serial.print(Mag_y_offset); //Serial.print("   ");
    //Serial.println(Mag_z_offset);
    //Serial.println("");

    // ----- Display soft-iron scale factors
    //Serial.print("Soft-iron: ");
    //Serial.print(Mag_x_scale); //Serial.print("   ");
    //Serial.print(Mag_y_scale); //Serial.print("   ");
    //Serial.println(Mag_z_scale);
    //Serial.println("");

    // ----- Display fuse ROM values
    //Serial.print("ASA: ");
    //Serial.print(ASAX); //Serial.print("   ");
    //Serial.print(ASAY); //Serial.print("   ");
    //Serial.println(ASAZ);

    // ----- Halt program
    while (true)
      ;  // Wheelspin ... program halt
  }
}


// -----------------------------------
//  Configure the gyro & accelerometer
// -----------------------------------
void config_gyro() {
  // ----- Activate the MPU-6050
  Wire.beginTransmission(0x68);  //Open session with the MPU-6050
  Wire.write(0x6B);              //Point to power management register
  Wire.write(0x00);              //Use internal 20MHz clock
  Wire.endTransmission();        //End the transmission

  // ----- Configure the accelerometer (+/-8g)
  Wire.beginTransmission(0x68);  //Open session with the MPU-6050
  Wire.write(0x1C);              //Point to accelerometer configuration reg
  Wire.write(0x10);              //Select +/-8g full-scale
  Wire.endTransmission();        //End the transmission

  // ----- Configure the gyro (500dps full scale)
  Wire.beginTransmission(0x68);  //Open session with the MPU-6050
  Wire.write(0x1B);              //Point to gyroscope configuration
  Wire.write(0x08);              //Select 500dps full-scale
  Wire.endTransmission();        //End the transmission
}


// -----------------------------------
//  Calibrate gyro
// -----------------------------------
void calibrate_gyro() {
  int ping;

  // ----- Calibrate gyro
  for (int counter = 0; counter < 2000; counter++)  //Run this code 2000 times
  {
    Loop_start = micros();

    read_mpu_6050_data();  //Read the raw acc and gyro data from the MPU-6050
    Gyro_x_cal += Gyro_x;  //Add the gyro x-axis offset to the gyro_x_cal variable
    Gyro_y_cal += Gyro_y;  //Add the gyro y-axis offset to the gyro_y_cal variable
    Gyro_z_cal += Gyro_z;  //Add the gyro z-axis offset to the gyro_z_cal variable

    while (micros() - Loop_start < Loop_time)
      ;  // Wait until "Loop_time" microseconds have elapsed

    if (counter == ping) {
      ping += 100;
    }
  }

  Gyro_x_cal /= 2000;  //Divide the gyro_x_cal variable by 2000 to get the average offset
  Gyro_y_cal /= 2000;  //Divide the gyro_y_cal variable by 2000 to get the average offset
  Gyro_z_cal /= 2000;  //Divide the gyro_z_cal variable by 2000 to get the average offset
}