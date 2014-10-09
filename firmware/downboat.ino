#include <EEPROM.h>
#include <CapacitiveSensor.h>

// 7 Segments Pins
volatile const int anodePins[2] = {6, 14};
volatile const int segmentPins[7] = {13,12,11,10,9,8,7};

// Touch Sensor Pins
const int capSendPin = 4;
const int dvRcvPin = 3; // Downvote pin
const int uvRcvPin = 5; // Upvote pin

// Which Pin is the negative number light on?
const int negativeLEDPin = 2;

// Touch sensor configurations
// these are stored {dv, uv}
long vThreshold[2] = {1000, 1000};
long vHoldTime[2] = {275, 275};
boolean vPressed[2] = {false, false};
long vStartPress[2] = {0, 0};
int vScore[2] = {-1, 1};

// Create Touch Sensors
CapacitiveSensor vCaps[2] = {CapacitiveSensor(capSendPin, dvRcvPin), CapacitiveSensor(capSendPin, uvRcvPin)}; 

// vote count
int voteCount = 0;
volatile int voteCountNumbers[2] = {0,0};
int minVotes = -20;
int maxVotes = 99;

// current segment of the 7-seg that is being driven
volatile int cSegment = 0;
volatile int cAnode = 0;

// 7-seg Display Layouts
volatile const byte segmentLayouts[10] = {
  B01111110,  // 0
  B00110000,  // 1
  B01101101,  // 2
  B01111001,  // 3
  B00110011,  // 4
  B01011011,  // 5
  B00011111,  // 6
  B01110000,  // 7
  B01111111,  // 8
  B01110011,  // 9
};

/* Setup
 * Gets the code up and running, loads some values from memory, all of the lights, etc.
 *   
 */
void setup()
{
    // Setup Negative Numbers Light (patentpending)
    pinMode(negativeLEDPin, OUTPUT);
    int negativeValue = EEPROM.read(0);
    
    // Initiate the seven seg anodes
    for(int i=0; i < 2; i++) {
        pinMode(anodePins[i], OUTPUT);
        
        voteCountNumbers[i] = EEPROM.read(i + 1);
        if(voteCountNumbers[i] > 9) {
            voteCountNumbers[i] = 0;
        }
    }
    voteCount = (voteCountNumbers[0] * 10) + voteCountNumbers[1];
    if(negativeValue) {
      voteCount = voteCount * -1;
    }

    // Initates and turn off all the segs
    for(int i=0; i < 7; i++) {
        pinMode(segmentPins[i], OUTPUT); // set segment and DP pins to output
        digitalWrite(segmentPins[i], HIGH);
    }
    
    // Setup the timer interrupt for our display driving cowboys
    // git along lil' photons.. git along!
    cli();
      //set timer1 interrupt at 60Hz
      TCCR1A = 0;// set entire TCCR1A register to 0
      TCCR1B = 0;// same for TCCR1B
      TCNT1  = 0;//initialize counter value to 0
      // set compare match register for 1hz increments
      OCR1A = 9;// = (16*10^6) / (1*1024) - 1 (must be <65536)
      // turn on CTC mode
      TCCR1B |= (1 << WGM12);
      // Set CS12 and CS10 bits for 1024 prescaler
      TCCR1B |= (1 << CS12) | (1 << CS10);  
      // enable timer compare interrupt
      TIMSK1 |= (1 << OCIE1A);
    sei();
}

// Timer 1 interrupt
ISR(TIMER1_COMPA_vect){

  // turn off last segment and anode
    digitalWrite(segmentPins[cSegment], HIGH);
    digitalWrite(anodePins[cAnode], LOW);
    
    // Move to the next segment and loop back at the end
    cSegment++;
    if(cSegment == 7) {
        cSegment = 0;
        
        // At the end of the segments, move to the next display
        cAnode++;
        if(cAnode == 2) {
          cAnode = 0;
        }
    }
    
    // Turn on the currently selected display
    digitalWrite(anodePins[cAnode], HIGH);    
    
    volatile boolean bitValue = 0;
    
    if(voteCountNumbers[cAnode] <= 9) {
        bitValue = bitRead(segmentLayouts[voteCountNumbers[cAnode]], cSegment);
    }
    
    bitValue = ! bitValue; // remove for CC
    digitalWrite(segmentPins[cSegment], bitValue);
}


void loop()        
{
    // Run through the touch sensors and manage the 'click' handling/vote counting
    for(int i = 0; i < 2; i++) {
        // Get the raw capacitive signal  
        long vSignal = vCaps[i].capacitiveSensor(40);
        
        if(!vPressed[i] && (vSignal > vThreshold[i]) && (vStartPress[i] == 0)) {
            // Detect start of a downvote press
            vStartPress[i] = millis();
        }
        else if(!vPressed[i] && (vStartPress[i] != 0) && ((millis() - vStartPress[i]) > vHoldTime[i])) {
            // Detect a hold long enough to 'click'        
            vPressed[i] = true;
            
            // Tally the buttons vote score
            voteCount = voteCount + vScore[i];
    
            // limit the vote count to upper and lower bounds
            if(voteCount < minVotes) {
                voteCount = minVotes;
            }          
            else if(voteCount > maxVotes) {
                voteCount = maxVotes;
            }
                     
            // Deconstruct the vote count into digits for display
            // thousands = (abs(voteCount)/1000); 
            // hundreds = ((abs(voteCount)/100)%10);
            voteCountNumbers[0] = ((abs(voteCount) / 10) % 10); // Tens
            EEPROM.write(1, voteCountNumbers[0]);
            
            voteCountNumbers[1] = (abs(voteCount) % 10); // Ones
            EEPROM.write(2, voteCountNumbers[1]);
            
        }
        else if(vPressed[i] && (vSignal < vThreshold[i])) {
            // Hold has been released
            vPressed[i] = false;
            vStartPress[i] = 0;
    
        }
        //one more case to reset the timer if press is releasd before threshold; to prevent milli buildup
        
        // Store the negative Bit
        if(voteCount > 0) {
          // bit off
          EEPROM.write(0, 0);
        }
        else {
          //bit on
          EEPROM.write(0, 1);
        }
    }

    // Vote Count less than 0? Turn on the negative sign
    digitalWrite(negativeLEDPin, LOW);
    if(voteCount < 0) {
        // Turn on your red light
        digitalWrite(negativeLEDPin, HIGH);
    }
}
