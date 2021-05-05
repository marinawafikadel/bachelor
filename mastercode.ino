#include <SoftwareSerial.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//SoftwareSerial BTSerial(10, 11);
int state = 0;
#define TRIGGER_COUNT 7
int triggerPins[TRIGGER_COUNT] = {A0,A1,A2 ,A3,A4,A5,8};
//lcd
//lcd screen size
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
//buzzer
int const buzzPin = 9;
void setup() {
  // put your setup code here, to run once:
  
   Serial.begin(9600);
   if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
   Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  for (int i=0; i<TRIGGER_COUNT; i++)
  {
    pinMode(triggerPins[i], INPUT_PULLUP);
  }
  pinMode(buzzPin, OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
     display.clearDisplay();
     display.setTextSize(1);
     display.setTextColor(WHITE);
     display.setCursor(0, 10);
     display.println("hello");
     
     if(Serial.available() > 0)
 { 
    // Checks whether data is comming from the serial port
    state = Serial.read();
     Serial.print(state);
   // Reads the data from the serial port
   
 }
  
     if ((digitalRead(triggerPins[0]) == HIGH)){
      static bool lastPress, thisPress;
            lastPress = thisPress;
           delay(10); // See comment, below
           thisPress = digitalRead(triggerPins[0]);
           if (thisPress && !lastPress) {
            Serial.write('1');
           
           }}
          
  if ((digitalRead(triggerPins[1]) == HIGH)){
      static bool lastPress, thisPress;
            lastPress = thisPress;
           delay(10); // See comment, below
           thisPress = digitalRead(triggerPins[1]);
           if (thisPress && !lastPress) {
            Serial.write('2');
           
           }}

      if ((digitalRead(triggerPins[2]) == HIGH)){
      static bool lastPress, thisPress;
            lastPress = thisPress;
           delay(10); // See comment, below
           thisPress = digitalRead(triggerPins[2]);
           if (thisPress && !lastPress) {
            Serial.write('3');
           
           }}
      if ((digitalRead(triggerPins[3]) == HIGH)){
      static bool lastPress, thisPress;
            lastPress = thisPress;
           delay(10); // See comment, below
           thisPress = digitalRead(triggerPins[3]);
           if (thisPress && !lastPress) {
            Serial.write('4');
           
           }}
           
     
       if ((digitalRead(triggerPins[6]) == HIGH)){
           digitalWrite(buzzPin, HIGH);
           delay(10000);
           digitalWrite(buzzPin, LOW);
      }

      if(state=='1'){
        display.println("Now in mode 2");
        
     
     
      }
     
       if(state=='2'){
        display.println("Now in mode 3");
       
      
       
      }
    
      if(state=='3'){
        display.println("Recording started");
       
       
        
      }
     
       if(state=='4'){
        display.println("Recording stopped");
       
         
       
      
      }
     
      if(state=='6'){
        display.println("RESET is done");
        
       
       
      }
     
      display.display();
      
     }
      
       
        
      
     
     
