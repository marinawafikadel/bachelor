#include <SPI.h>
#include <SdFat.h>
//#include <SdFatUtil.h>
#include <SFEMP3Shield.h>
#include <SoftwareSerial.h>
//SoftwareSerial BTSerial(10, 11);
int state = 0;
const uint8_t volume = 0;
uint8_t    result;                    // globally check return values from various functions
uint8_t    rec_state;                 // state of recording function.
uint8_t    counter = 0;
//uint16_t numberOfFiles = 0;
int numberOfFiles = 0;
char Dateiname[13];
unsigned int nameLen;
int led = 13;
char *nbuffer;
#define stop_BUTTON A4
#define REC_BUTTON A3
#define RESET_BUTTON A5
#define VS1053_INT_ENABLE 0xC01A
#define MAX_BUFF   13 
#define TRIGGER_COUNT 7
int triggerPins[TRIGGER_COUNT] = {A0,A1,A2 ,3,4,5};
int lastTrigger = 0;
SdFat             sd;
SdFile            file;
SdFile            root;
SFEMP3Shield      MP3player;
uint8_t record(char *filename)
{
  uint8_t wordsToWrite;
  uint16_t data, wordsToRead, wordsWaiting;
  uint32_t timer;
  byte wbuff[256]; // we write up to 256 bytes at a time twice

  // make sure state begins at 0
  rec_state = 0;

  /* 1. set VS1053 clock to 4.5x = 55.3 MHz; our board uses a 12.288MHz crystal */
  MP3player.Mp3WriteRegister(SCI_CLOCKF, 0xC000);
  timer = millis();
  while (!digitalRead(MP3_DREQ) || millis() - timer > 100UL) {;}
  
  /* 2. clear SCI_BASS */
  MP3player.Mp3WriteRegister(SCI_BASS, 0);

  /* 3. reset VS1053 */
  MP3player.Mp3WriteRegister(SCI_MODE, (MP3player.Mp3ReadRegister(SCI_MODE) | SM_RESET));
  // Wait until DREQ is high or 100ms
  timer = millis();
  while (!digitalRead(MP3_DREQ) || millis() - timer > 100UL) {;}

  /* 4. disable all interrupts except SCI */
MP3player.Mp3WriteRegister(SCI_WRAMADDR, VS1053_INT_ENABLE);
  MP3player.Mp3WriteRegister(SCI_WRAM, 0x2);

  /* 5. need to load plugin for recording (exported as oggenc.053, 
        original filename is venc44k2q05.plg but it's too large for an SD card) */
  if (MP3player.VSLoadUserCode("oggenc.053")) {
    Serial.println(F("Could not load plugin"));
    return 1;
  }
 

  // load file for recording
  if (filename) {
    if (!file.open(filename, O_RDWR | O_CREAT)) {
      Serial.println(F("Could not open file"));
      return 2;
    }
  } else {
    Serial.println(F("No filename given"));
    return 3;
  }

  /* 6. Set VS1053 mode bits as needed. If USE_LINEIN is set
        then the line input will be used over the on-board microphone */
  #ifdef USE_LINEIN
    MP3player.Mp3WriteRegister(SCI_MODE, SM_LINE1 | SM_ADPCM | SM_SDINEW);
  #else
    MP3player.Mp3WriteRegister(SCI_MODE, SM_ADPCM | SM_SDINEW);
  #endif

  /* 7. Set recording levels on control registers SCI_AICTRL1/2 */
  // Rec level: 1024 = 1. If 0, use AGC.
  MP3player.Mp3WriteRegister(SCI_AICTRL1, 1024);
  // Maximum AGC level: 1024 = 1. Only used if SCI_AICTRL1 is set to 0.
  MP3player.Mp3WriteRegister(SCI_AICTRL2, 0);
  
  /* 8. no VU meter to set */
   
  /* 9. set a value to SCI_AICTRL3, in this case 0 */
  MP3player.Mp3WriteRegister(SCI_AICTRL3, 0);

  /* 10. no profile to set for VOX */

  /* 11. Active encoder by writing 0x34 to SCI_AIADDR for Ogg Vorbis */
  MP3player.Mp3WriteRegister(SCI_AIADDR, 0x34);

  /* 12. wait until DREQ pin is high before reading data */
  timer = millis();
  while (!digitalRead(MP3_DREQ) || millis() - timer > 100UL) {;}


  /**
   * Handles recording data:
   * state == 0 -> normal recording
   * state == 1 -> user and micro requested end of recording
   * state == 2 -> stopped recording, but data still being collected
   * state == 3 -> recoding finished
   */
  
  Serial.println(F("Recording started..."));
  timer = millis(); // start timing.
  while (rec_state < 3) {
//    printline(LINE2, (millis() - timer) / 1000);
    // detect if key is pressed and stop recording
   if((digitalRead(stop_BUTTON) == HIGH)&& !rec_state){
    digitalWrite(led, LOW);
    Serial.write('4');
     rec_state = 1;
     MP3player.Mp3WriteRegister(SCI_AICTRL3, 1);
   }
    // check for serial request to stop recording
    if (Serial.available()) {
      if (Serial.read() == 's' && !rec_state) {
        rec_state = 1;
        MP3player.Mp3WriteRegister(SCI_AICTRL3, 1);
      }
    }

    // see how many 16-bit words there are waiting in the VS1053 buffer
    wordsWaiting = MP3player.Mp3ReadRegister(SCI_HDAT1);

    // if user has requested and VS1053 has stopped recording increment state to 2
    if (rec_state == 1 && MP3player.Mp3ReadRegister(SCI_AICTRL3) & (1 << 1)) {
      rec_state = 2;
      // reread the HDAT1 register to make sure there are no extra words left.
      wordsWaiting = MP3player.Mp3ReadRegister(SCI_HDAT1);
    }

    // read and write 512-byte blocks. Except for when recording ends, then write a smaller block.
    while (wordsWaiting >= ((rec_state < 2) ? 256 : 1)) {
      wordsToRead = min(wordsWaiting, 256);
      wordsWaiting -= wordsToRead;

      // if this is the last block, read one 16-bit value less as it is handled separately
      if (rec_state == 2 && !wordsWaiting) {wordsToRead--;}

      wordsToWrite = wordsToRead / 2;

      // transfer the 512-byte block in two groups of 256-bytes due to memory limitations,
      // except if it's the last block to transfer, then transfer all data except for the last 16-bits
      for (int i = 0; i < 2; i++)
      {
        for (uint8_t j = 0; j < wordsToWrite; j++)
        {
          data = MP3player.Mp3ReadRegister(SCI_HDAT0);
          wbuff[2 * j] = data >> 8;
          wbuff[2 * j + 1] = data & 0xFF;
        }
        file.write(wbuff, 2 * wordsToWrite);
      }
            
      // if last data block
      if (wordsToRead < 256)
      {
        rec_state = 3;

        // read the very last word of the file
        data = MP3player.Mp3ReadRegister(SCI_HDAT0);

        // always write first half of the last word
        file.write(data >> 8);

        // read SCI_AICTRL3 twice, then check bit 2 of the latter read
        MP3player.Mp3ReadRegister(SCI_AICTRL3);
        if (!(MP3player.Mp3ReadRegister(SCI_AICTRL3) & (1 << 2)))
        {
          // write last half of the last word only if bit 2 is clear
          file.write(data & 0xFF);
        }
      }
    }
  }

  // done, now close file
  file.close();

  MP3player.Mp3WriteRegister(SCI_MODE, (MP3player.Mp3ReadRegister(SCI_MODE) | SM_RESET));
  Serial.println(F("Recording finished"));
  return 0;
}
void setup()
{
  Serial.begin(9600);
  //BTSerial.begin(38400);
  if (!sd.begin(SD_SEL, SPI_HALF_SPEED)) sd.initErrorHalt();
  if (!sd.chdir("/")) sd.errorHalt("sd.chdir");
  
  while (root.openNext(sd.vwd(), O_READ)) {
     numberOfFiles+=1;
     root.close();   
  }
  // initialise the MP3 Player Shield; begin() will attempt to load patches.053  
  result = MP3player.begin();
  if (result != 0) {
    Serial.print(F("Error code: "));
    Serial.print(result);
    Serial.println(F(" when trying to start MP3 player"));
    if (result == 6) {
      Serial.println(F("Warning: patch file not found, skipping."));
    }
  }
  
  Serial.println(numberOfFiles);
  pinMode(led, OUTPUT);
  for (int i=0; i<TRIGGER_COUNT; i++)
  {
    pinMode(triggerPins[i], INPUT_PULLUP);
  }
  pinMode(REC_BUTTON, INPUT_PULLUP);
   pinMode(stop_BUTTON, INPUT_PULLUP);
 pinMode(RESET_BUTTON, INPUT_PULLUP);
  // default volume is too loud (higher 8-bit values means lower volume).
  MP3player.setVolume(volume, volume);
}
void loop()
{
   if(Serial.available() > 0)
 { 
    // Checks whether data is comming from the serial port
    state = Serial.read(); // Reads the data from the serial port
 }
  if((digitalRead(REC_BUTTON) == HIGH)){
    Serial.write('3');
   digitalWrite(led, HIGH);
  char filename[MAX_BUFF];
  static uint8_t recfn = numberOfFiles-6;
  // static uint8_t recfn = 0;
 // strcpy(filename, "track000.ogg");
  
 strcpy(filename, "track000.ogg");
  filename[6] = (recfn / 10) + '0';
          filename[7] = (recfn % 10) + '0';
          recfn++;
          if (recfn == 100) {
            recfn = 0;
            }
           
           result = record(filename);
   
      if (!result) {
      Serial.println("finished");
      Serial.println(filename);
      } else {
        Serial.println("Can't record:");

        if      (result == 1) {Serial.println(F("no plugin"));}
        else if (result == 2) {Serial.println("cannot make file");}
        else                  {Serial.println("wrong filename");}
      }

      delay(2000);
      MP3player.vs_init();         // restart MP3 player after recording to prevent lockups during playback
      MP3player.setVolume(volume, volume); // volume does need to be reset.

          
}


  else{
   
     if ((digitalRead(triggerPins[0]) == HIGH)){
      if(counter==0){
      if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK000.ogg");
      }
      else{
        if(counter==1){
           if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK008.ogg");
        }
        else{
           if(counter==2){
           if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK016.ogg");
        }
      }
     
      }
     }
        if(digitalRead(triggerPins[1]) == HIGH){
          if(counter==0){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK001.ogg");
          }
          else{
            if(counter==1){
             if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK009.ogg"); 
            }
            else{
               if(counter==2){
             if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK017.ogg"); 
            }
          }
      
        }
        }
        if(digitalRead(triggerPins[2]) == HIGH){
          if(counter==0){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK002.ogg");
          }
          else{
            if(counter==1){
               if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK010.ogg");
            }
            else{
          if(counter==2){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK018.ogg");
            }
          }
      
        }
        }
      
        if(digitalRead(triggerPins[3]) == HIGH){
          if(counter==0){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK003.ogg");
          }
          else{
          if(counter==1){
             if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK011.ogg");
          }
          else{
            if(counter==2){
               if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK019.ogg");
            }
          }
       
          }
      
        }
        if(digitalRead(triggerPins[4]) == HIGH){
          if (counter==0){
              counter=0;
          }
          else{
            static bool lastPress, thisPress;
            lastPress = thisPress;
           delay(10); // See comment, below
           thisPress = digitalRead(triggerPins[4]);
           if (thisPress && !lastPress) {
            counter--;
             if(counter==0){
              if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
        
       MP3player.playMP3("MODE1.ogg");
            }
            else{
              if(counter==1){
              if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
        
       MP3player.playMP3("MODE2.ogg");
            }
            }
           Serial.print(counter);
           }
  }
          
      
        }
        
         if(digitalRead(triggerPins[5]) == HIGH){
          if (counter==2){
              counter=2;
          }
          else{
            static bool lastPress, thisPress;
            lastPress = thisPress;
           delay(10); // See comment, below
           thisPress = digitalRead(triggerPins[5]);
           if (thisPress && !lastPress) {
            counter++;
            if(counter==1){
              if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
      
       MP3player.playMP3("MODE2.ogg");
     
            }
            else{
              if(counter==2){
              if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
        
       MP3player.playMP3("MODE3.ogg");
            }
            }
           Serial.print(counter);
           }
  }
          }
      
        
        if(digitalRead(RESET_BUTTON) == HIGH){
          sd.remove("TRACK000.ogg");
          sd.remove("TRACK001.ogg");
          sd.remove("TRACK002.ogg");
          sd.remove("TRACK003.ogg");
          sd.remove("TRACK004.ogg");
          sd.remove("TRACK005.ogg");
          sd.remove("TRACK006.ogg");
          sd.remove("TRACK007.ogg");
          sd.remove("TRACK008.ogg");
          sd.remove("TRACK009.ogg");
          sd.remove("TRACK010.ogg");
          sd.remove("TRACK011.ogg");
          sd.remove("TRACK012.ogg");
          sd.remove("TRACK013.ogg");
          sd.remove("TRACK014.ogg");
          sd.remove("TRACK015.ogg");
          sd.remove("TRACK016.ogg");
          sd.remove("TRACK017.ogg");
          sd.remove("TRACK018.ogg");
          sd.remove("TRACK019.ogg");
          sd.remove("TRACK020.ogg");
          sd.remove("TRACK021.ogg");
          sd.remove("TRACK022.ogg");
          sd.remove("TRACK023.ogg");
          sd.remove("TRACK024.ogg");
          sd.remove("TRACK025.ogg");
          sd.remove("TRACK026.ogg");
          sd.remove("TRACK027.ogg");
          sd.remove("TRACK028.ogg");
           Serial.write('6');
        }
        
         if (state == '1'){ 
          Serial.print("hi");
 
       if(counter==0){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK004.ogg");
          }
          else{
          if(counter==1){
             if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK012.ogg");
          }
          else{
            if(counter==2){
               if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK020.ogg");
            }
          }
       
          }
          state=0;
       
 }
     if (state == '2') 
 {
     Serial.print("hello");
     if(counter==0){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK005.ogg");
          }
          else{
          if(counter==1){
             if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK013.ogg");
          }
          else{
            if(counter==2){
               if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK021.ogg");
            }
          }
       
          }
         state=0;
     
 }     
   if (state == '3') 
 {
   if(counter==0){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK006.ogg");
          }
          else{
          if(counter==1){
             if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK014.ogg");
          }
          else{
            if(counter==2){
               if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK022.ogg");
            }
          }
       
          }
       state=0;
      
 }        

   if (state == '4') 
 {
    if(counter==0){
          if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK007.ogg");
          }
          else{
          if(counter==1){
             if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK015.ogg");
          }
          else{
            if(counter==2){
               if (MP3player.isPlaying()){
           MP3player.stopTrack();
      } 
       MP3player.playMP3("TRACK023.ogg");
            }
          }
       
          }
       state=0;
     
 } 
        
      
  }

}
