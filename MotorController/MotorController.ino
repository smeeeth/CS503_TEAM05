#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
  #include "Wire.h"
#endif
#include "DualMC33926MotorShield.h"

MPU6050 mpu;
DualMC33926MotorShield md;

#define LED_PIN 13 // (Arduino is 13, Teensy is 11, Teensy++ is 6)
#define encoder0PinA  2
#define encoder0PinB  5
#define encoder1PinA  3
#define encoder1PinB  6

volatile int encoder0Pos = 0;
volatile int encoder1Pos = 0;

bool blinkState = false;
void doEncoder0();
void doEncoder1();

// ================================================================
// ===                 FAULT DETECTION ROUTINE                  ===
// ================================================================

void stopIfFault(){
  if (md.getFault()){
    Serial.println("fault");
    while (1);
  }
}

// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup() {
  // join I2C bus (I2Cdev library doesn't do this automatically)
  #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    Wire.begin();
    TWBR = 24; // 400kHz I2C clock (200kHz if CPU is 8MHz)---->Two Wire Bit Rate Generator
  #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
    Fastwire::setup(400, true);
  #endif

  // initialize serial communication
  //Serial.begin(115200);
  Serial.begin(9600);
  while (!Serial); // wait for Leonardo enumeration, others continue immediately

  // NOTE: 8MHz or slower host processors, like the Teensy @ 3.3v or Ardunio
  // Pro Mini running at 3.3v, cannot handle this baud rate reliably due to
  // the baud timing being too misaligned with processor ticks. You must use
  // 38400 or slower in these cases, or use some kind of external separate
  // crystal solution for the UART timer.

  // initialize device
  mpu.initialize(); //MPU6050.cpp
  mpu.setFullScaleGyroRange(MPU6050_GYRO_FS_2000);

  // supply your own gyro offsets here, scaled for min sensitivity
  mpu.setXGyroOffset(220);
  mpu.setYGyroOffset(76);
  mpu.setZGyroOffset(-85);
  mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

  //encoder setup
  //setup encoder 0
  pinMode(encoder0PinA, INPUT);
  digitalWrite(encoder0PinA, HIGH);       // turn on pull-up resistor
  pinMode(encoder0PinB, INPUT);
  digitalWrite(encoder0PinB, HIGH);       // turn on pull-up resistor
  attachInterrupt(0, doEncoder0, CHANGE);  // encoder pin on interrupt 0 - pin 2
  //setup encoder 1
  pinMode(encoder1PinA, INPUT);
  digitalWrite(encoder1PinA, HIGH);       // turn on pull-up resistor
  pinMode(encoder1PinB, INPUT);
  digitalWrite(encoder1PinB, HIGH);       // turn on pull-up resistor
  attachInterrupt(1, doEncoder1, CHANGE);  // encoder pin on interrupt 1 - pin 3

  // configure LED for output
  pinMode(LED_PIN, OUTPUT);

  //motor setup
  md.init();

}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

//BALANCER VALUES
int pwm_r = 0;
int pwm_l = 0;
int PWMMAX = 300;
double theta_ref = 0.0; //0.035
double gyro_off = 0.44;
double Kangle = 0.7; //0.2
double Bangle = 0.2; //0.2
double PWMout = 0.0;
int torque_stall_r = 37;  //get rid of this perhaps?
int torque_stall_l = 36;  //get rid of this perhaps?
bool use_torque = true;
int16_t ax, ay, az;
int16_t gx, gy, gz;

//IR VALUES
bool A0_read = LOW;
bool A1_read = LOW;
bool last_B0_read = LOW;
bool last_B1_read = LOW;

//ENCODER VALUES
double theta = 0;
double xLocation = 0;
double yLocation = 0;
const int ENCODERCOUNTS = 32;
const double CIRCUMFERENCE = 17.9542020153; //circumference of one wheel
const double WHEELBASE = 10.16; //distance between wheels and center (must be in same units as circumference, and input for challenge)
const double MULTIPLER = 5; //multiplier to amplify distance from point into PWM

//CLOCK VALUES
long previousMillis = 0;        // will store last time LED was updated
long interval = 1000;       // interval at which to blink (milliseconds)
int counter = 0;


//testing vals
int error_right = 0;
int error_left = 0;
double velocity = 0;
double Kvelocity = 0.0001;
double KvPWM = 0.01;
const double THETA_MAX = 0.055;
double deltaLean = 0;
double deltaVelPWM = 0;
bool stopping = false;
double distLeft;
double distRight;
bool flag =false;


//motor 1 is the right wheel
//motor 2 is the left wheel
int motorCounter = 0;
double angle = 0.0;

void loop() {

  //get current time
  unsigned long currentMillis = millis();

  //update loop counter
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    motorCounter++; //increment coutner variable for motor
  }  
  else{
    //code to 'waddle'
    if (motorCounter < 10){
      left_turn();
    }
    else if (motorCounter < 20){
      right_turn();
    }
    else{
      motorCounter = 0;
    }
//    if(motorCounter > 10){
//      dist_two();
//      if(abs(theta) <= 1.91){    // 0.65  = 90 degree
//        left_turn();
//      }
//      else{
//        balance();
//      }
//    }
//    else{
//      balance();
//      theta= 0;
//    }
  }

}


/* See this expanded function to get a better understanding of the
 * meanings of the four possible (pinA, pinB) value pairs:
 */
//right wheel
void doEncoder0() {
  A0_read = digitalRead(encoder0PinA);
  bool current_B = digitalRead(encoder0PinB);
  if (current_B != last_B0_read) {
    if (A0_read == LOW && current_B == LOW){
      encoder0Pos = encoder0Pos - 1;
      void dist_Change_Minus_left();
    }
    else if (A0_read == LOW && current_B == HIGH){
      encoder0Pos = encoder0Pos + 1;
      dist_Change_Plus_left();
    } 
    else if (A0_read == HIGH && current_B == LOW){
      encoder0Pos = encoder0Pos + 1;
      dist_Change_Plus_left();
    }
    else{
      encoder0Pos = encoder0Pos - 1;
      void dist_Change_Minus_left();
    }
    last_B0_read = current_B;
  }
}

//left wheel
void doEncoder1() {
  A1_read = digitalRead(encoder1PinA);
  bool current_B = digitalRead(encoder1PinB);
  if (current_B != last_B1_read){
    if (A1_read == LOW && current_B == LOW){
      encoder1Pos = encoder1Pos - 1;
      dist_Change_Minus_right();
    }
    else if (A1_read == LOW && current_B == HIGH){
      encoder1Pos = encoder1Pos + 1;
      dist_Change_Plus_right();
    }
    else if (A1_read == HIGH && current_B == LOW){
      encoder1Pos = encoder1Pos + 1;
      dist_Change_Plus_right();
    }
    else{
      encoder1Pos = encoder1Pos - 1;
      dist_Change_Minus_right();
    }
      last_B1_read = current_B;
  }
}

void balance() {
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  double raw_angle = atan2(ay,az);

  double angle_err = raw_angle + theta_ref;
  double angular_rate = ((double)gx)/131.0 - gyro_off;
  double deltaPWM = -Kangle*angle_err - Bangle*angular_rate;
  PWMout += deltaPWM;// + deltaVelPWM;
  deltaVelPWM = 0;

  int PWMint = (int)PWMout;

  if (use_torque){
    if (PWMint > 0){
      pwm_l = -PWMint - torque_stall_l;
      pwm_r = PWMint + torque_stall_r;
    } 
    else{
      pwm_l = -PWMint + torque_stall_l;
      pwm_r = PWMint - torque_stall_r;
    }
  } 
  else{
    pwm_l = -PWMint;
    pwm_r = PWMint;
  }

  if (PWMint >= PWMMAX){
    PWMint = PWMMAX;
  }
  else if (PWMint <= -PWMMAX){
    PWMint = -PWMMAX;
  }

  md.setM1Speed(pwm_r);
  md.setM2Speed(pwm_l); 
}


void dist_Change_Plus_left(){
  distLeft += 2 * 1 * 1 / (ENCODERCOUNTS/CIRCUMFERENCE);
}

void dist_Change_Minus_left(){
  distLeft +=- 2 * 1 * 1 / (ENCODERCOUNTS/CIRCUMFERENCE);
}

void dist_Change_Plus_right(){
  distRight += 2 * 1 * 1 / (ENCODERCOUNTS/CIRCUMFERENCE);
}

void dist_Change_Minus_right(){
  distRight +=- 2 * 1 * 1 / (ENCODERCOUNTS/CIRCUMFERENCE);
}

void dist_two(){
  //add new values to global variable
  // Serial.print(distRight);
  double distAverage = (distLeft + distRight) / 2;
  double thetaChange = (distLeft - distAverage)/ WHEELBASE;
  theta = theta + thetaChange;
  xLocation = xLocation + (distAverage * cos(theta));
  distLeft = distRight = 0;
  Serial.println(theta);
}

void left_turn(){
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  double raw_angle = atan2(ay, az);

  double angle_err = raw_angle + theta_ref;
  double angular_rate = ((double)gx) / 131.0 - gyro_off;
  double deltaPWM = -Kangle * angle_err - Bangle * angular_rate;
  PWMout += deltaPWM;// + deltaVelPWM;
  deltaVelPWM = 0;

  int PWMint = (int)PWMout;

  if (use_torque){
    if (PWMint > 0){
      pwm_l = -PWMint - torque_stall_l;
      pwm_r = PWMint + torque_stall_r;
    } 
    else{ 
      pwm_l = -PWMint + torque_stall_l;
      pwm_r = PWMint - torque_stall_r;
    }
  } 
  else{
    pwm_l = -PWMint;
    pwm_r = PWMint;
  }

  if (PWMint >= PWMMAX){
    PWMint = PWMMAX;
  }
  else if (PWMint <= -PWMMAX){
    PWMint = -PWMMAX;
  }

  md.setM1Speed(pwm_r/3+100);
  md.setM2Speed(pwm_l/3+100);
  
}

void right_turn(){
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  double raw_angle = atan2(ay, az);

  double angle_err = raw_angle + theta_ref;
  double angular_rate = ((double)gx) / 131.0 - gyro_off;
  double deltaPWM = -Kangle * angle_err - Bangle * angular_rate;
  PWMout += deltaPWM;// + deltaVelPWM;
  deltaVelPWM = 0;

  int PWMint = (int)PWMout;

  if (use_torque){
    if (PWMint > 0){
      pwm_l = -PWMint - torque_stall_l;
      pwm_r = PWMint + torque_stall_r;
    }
    else{
      pwm_l = -PWMint + torque_stall_l;
      pwm_r = PWMint - torque_stall_r;
    }
  } 
  else{
    pwm_l = -PWMint;
    pwm_r = PWMint;
  }

  if (PWMint >= PWMMAX) {
    PWMint = PWMMAX;
  }
  else if (PWMint <= -PWMMAX) {
    PWMint = -PWMMAX;
  }

  md.setM1Speed(pwm_r/3-100);
  md.setM2Speed(pwm_l/3-100);

}


