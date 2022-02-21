
#include <FatReader.h>
#include <SdReader.h>
#include <avr/pgmspace.h>
#include "WaveUtil.h"
#include "WaveHC.h"
#include <FastLED.h>

#define LED_PIN  7

#define COLOR_ORDER GRB
#define CHIPSET     WS2811


SdReader card;    // This object holds the information for the card
FatVolume vol;    // This holds the information for the partition on the card
FatReader root;   // This holds the information for the filesystem on the card
FatReader f;      // This holds the information for the file we're play

WaveHC wave;      // This is the only wave (audio) object, since we will only play one at a time

#include <avr/pgmspace.h>

char buffer[15];  // make sure this is large enough for the largest string it must hold
const char count_1[] PROGMEM = "COUNT1.WAV";
const char count_2[] PROGMEM = "COUNT2.WAV";
const char count_3[] PROGMEM = "COUNT3.WAV";
const char count_4[] PROGMEM = "COUNT4.WAV";
const char count_5[] PROGMEM = "COUNT5.WAV";
const char count_6[] PROGMEM = "COUNT6.WAV";
const char count_7[] PROGMEM = "COUNT7.WAV";
const char count_8[] PROGMEM = "COUNT8.WAV";
const char count_9[] PROGMEM = "COUNT9.WAV";

const char *const files[] PROGMEM = {
  count_1, 
  count_2, 
  count_3,
  count_4,
  count_5,
  count_6,
  count_7,
  count_8,
  count_9,
};

// Params for width and height
#define LED_WIDTH_DENSITY 2

#define MATRIX_WIDTH 5
#define MATRIX_HEIGHT 5

// Game variables
byte player[2] = {2,2};
byte resources_collected = 0;
#define RESOURCE_COUNT 2
byte resources[RESOURCE_COUNT][2] = {{4,2}, {3,1}};

// LED config 
#define NUM_LEDS 50
#define BRIGHTNESS 96

CRGB leds_plus_safety_pixel[ NUM_LEDS + 1];
CRGB* const leds( leds_plus_safety_pixel + 1);

uint8_t gHue = 0; // rotating "base color" used by many of the patterns
#define FRAMES_PER_SECOND  120

// All numbers need to be padded to 13 to accomidate '8'
const byte numbers[][13][2] PROGMEM = {
  {{2, 0}, {2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}, {2, 4}}, //1
  {{1, 0}, {2, 0}, {3, 0}, {1, 1}, {1, 2}, {2, 2}, {3, 2}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {3, 4}, {3, 4}}, //2
  {{1, 0}, {2, 0}, {3, 0}, {3, 1}, {2, 2}, {3, 2}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}}, //3
  {{3, 0}, {3, 1}, {1, 2}, {2, 2}, {3, 2}, {1, 3}, {3, 3}, {1, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}}, //4
  {{1, 0}, {2, 0}, {3, 0}, {3, 1}, {1, 2}, {2, 2}, {3, 2}, {1, 3}, {1, 4}, {2, 4}, {3, 4}, {3, 4}, {3, 4}}, //5
  {{1, 0}, {2, 0}, {3, 0}, {1, 1}, {3, 1}, {1, 2}, {2, 2}, {3, 2}, {1, 3}, {1, 4}, {2, 4}, {3, 4}, {3, 4}}, //6
  {{3, 0}, {3, 1}, {3, 2}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}, {3, 4}}, //7
  {{1, 0}, {2, 0}, {3, 0}, {1, 1}, {3, 1}, {1, 2}, {2, 2}, {3, 2}, {1, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}}, //8
  {{1, 0}, {2, 0}, {3, 0}, {3, 1}, {1, 2}, {2, 2}, {3, 2}, {1, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {3, 4}}, //9
};

// Input config 
#define DEBOUNCE 100  // input debouncer


/*
 * Define macro to put error messages in flash memory
 */
#define error(msg) error_P(PSTR(msg))


// ======================  //
//      AUDIO HELPERS      //
// ======================  //

/*
 * Plays a full file from beginning to end with no pause based on the index for the file name
 */
void playFileByIndex(int i) {  
  strcpy_P(buffer, (char *)pgm_read_word(&(files[i])));  // Necessary casts and dereferencing, just copy.
  Serial.println(buffer);
  playcomplete(buffer);  
}


/*
 * Plays a full file from beginning to end with no pause
 */
void playcomplete(char *name) {
  // call our helper to find and play this name
  playfile(name);
  while (wave.isplaying) {
  // do nothing while its playing
  }
}

void playfile(char *name) {
  if (wave.isplaying) {
    wave.stop();
  }
  
  // look in the root directory and open the file
  if (!f.open(root, name)) {
    putstring("Couldn't open file ");
    Serial.print(name);
    return;
  }
  
  // OK read the file and turn it into a wave object
  if (!wave.create(f)) {
    putstring_nl("Not a valid WAV"); return;
  }
  
  wave.play();
}


// ======================  //
//       LED HELPERS       //
// ======================  //

// LED Arrangement:
//
//    19 > 18  > 17 > 16 > 15
//                         |
//                         |
//    10 < 11 <  12 < 13 < 14
//     |
//     |
//     9 > 8 > 7  > 6  >  5
//                        |
//                        |
//    0  >  1  > 2  > 3 > 4
//
// Paired into groups of 2. 

uint16_t XY(uint8_t x, uint8_t y)
{
  uint16_t i;
  if( y & 0x01) {
    // Odd rows run backwards
    uint8_t reverseX = ((MATRIX_WIDTH * LED_WIDTH_DENSITY) - 1) - x;
    i = (y * (MATRIX_WIDTH * LED_WIDTH_DENSITY)) + reverseX;
  } else {
    // Even rows run forwards
    i = (y * (MATRIX_WIDTH * LED_WIDTH_DENSITY)) + x;
  }
  return i;
}

void setLedCellColor(uint8_t x, uint8_t y, CRGB colour)
{
  int nextX = 1;
   
  leds[ XY(2 *x + nextX, y)]  = colour;
  leds[ XY(2 * x, y)]  = colour;
}
 
/*
 * Set the LED state for the player
 */
void drawPlayer()
{
  setLedCellColor(player[0], player[1], CRGB::Green);
}

/*
 * Set the LED state for all resources 
 */
void drawResources()
{  
    for (int i = 0; i < RESOURCE_COUNT; i++){
      setLedCellColor(resources[i][0], resources[i][1], CRGB::Yellow);
   }
}

/*
 * Display a number from 1 - 9, zero-indexed
 */
void displayNumber(int numberIndex) {
  for (int i = 0; i < 13; i++ ) {    
    byte ledRow = pgm_read_byte(&(numbers[numberIndex][i][0]));
    byte ledCol = pgm_read_byte(&(numbers[numberIndex][i][1]));
    
    setLedCellColor(ledRow, ledCol, CRGB::Blue);
 }
}

/*
 * Black out all LEDs, does not call 'show'
 */
void clearDisplay() {
 for (int i = 0; i < NUM_LEDS; i++ ) {
    leds[i]  = CRGB::Black;
 }
}

// ======================  //
//     INPUT HELPERS       //
// ======================  //

void checkJoystick()
{
  int joyUp = digitalRead(A0);
  int joyDown = digitalRead(A1);
  int joyLeft = digitalRead(A2);
  int joyRight = digitalRead(A3);
  
  if (joyLeft == LOW) {
    player[0] = player[0] == 0 ? MATRIX_WIDTH - 1 :(player[0] - 1) % MATRIX_WIDTH;
  } else if (joyRight == LOW) {
    player[0] =  (player[0] + 1) % MATRIX_WIDTH;
  }

  if (joyUp == LOW) {
    player[1] =  (player[1] + 1) % MATRIX_HEIGHT;
    Serial.println(player[1]);
  } else if (joyDown == LOW) {
     player[1] = player[1] == 0 ? MATRIX_HEIGHT - 1 :(player[1] - 1) % MATRIX_HEIGHT;
    Serial.println(player[1]);
  }
}


// ======================  //
//      GAME LOGIC         //
// ======================  //

void checkResourceCollected()
{  
    for (int i = 0; i < RESOURCE_COUNT; i++){
      if (!(resources[i][0] == player[0] && resources[i][1] == player[1])) {
        continue;
      }

      // Player collected a resource 
      resources_collected = resources_collected + 1;

      clearDisplay();
      displayNumber(resources_collected -1);
      FastLED.show();
      playFileByIndex(resources_collected - 1);
      
      createNewResource(i);
      
      // Max score reached. Reset the game after showing the 'success' screen
      if (resources_collected > 1) {
        drawSuccessScreen();
        FastLED.show();
        delay(300);
        resources_collected = 0;
      }

      // Expects player/resources to be redrawn after call
      clearDisplay();
    }
}


/*
 * Create a single resource, verifying that it was created in a valid location
 */
void createNewResource(byte resource_index)  
{
  do 
  {
    resources[resource_index][0] = random(0, MATRIX_WIDTH);
    resources[resource_index][1] = random(0, MATRIX_HEIGHT);
  } while (checkInvalidResourceLocation(resource_index));
  
}

bool checkInvalidResourceLocation(byte resource_index) 
{
   // Check existing resource conflict
   for (int i = 0; i < RESOURCE_COUNT; i++){
      if (i == resource_index) 
      {
        continue;
      }
      
      if (resources[i][0] == resources[resource_index][0] && resources[i][1] == resources[resource_index][1]) 
      {
        return true;
      }
   } 

   // Check player conflict
   if (player[0] == resources[resource_index][0] && player[1] == resources[resource_index][1]) 
   {
      return true;
   }

   return false;
}

void drawSuccessScreen() {
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
}

// ======================  //
//       MAIN LOOP         //
// ======================  //

void loop()
{
  
    clearDisplay();
    
//    for (int i = 0; i < 9; i++ ) {
//
//      checkJoystick();
//     
//     displayNumber(i);
//     FastLED.show();
//
//     playFileByIndex(i);
//         
//     delay(500);
//     
//     clearDisplay();
//    }

    checkJoystick();
    checkResourceCollected();
    
    drawResources();
    drawPlayer();
        
    FastLED.show();

    delay(200);

    EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}


// ======================  //
//         SETUP           //
// ======================  //

/*
* Return the number of bytes currently free in RAM  
*/
int freeRam(void)
{
  extern int  __bss_end; 
  extern int  *__brkval; 
  int free_memory; 
  if((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__bss_end); 
  }
  else {
    free_memory = ((int)&free_memory) - ((int)__brkval); 
  }
  return free_memory; 
} 

/*
 * print error message and halt
 */
void error_P(const char *str) {
  PgmPrint("Error: ");
  SerialPrint_P(str);
  sdErrorCheck();
  while(1);
}

void sdErrorCheck(void)
{
  if (!card.errorCode()) return;
  putstring("\n\rSD I/O error: ");
  Serial.print(card.errorCode(), HEX);
  putstring(", ");
  Serial.println(card.errorData(), HEX);
  while(1);
}

void setup() {
 Serial.begin(9600);           // set up Serial library at 9600 bps for debugging
  
  putstring_nl("\n Start Light Game !");
  
  putstring("Free RAM: ");       // This can help with debugging, running out of RAM is bad
  Serial.println(FreeRam());

  if (!card.init()) {         //play with 8 MHz spi (default faster!)  
    error("Card init. failed!");  // Something went wrong, lets print out why
  }
  
  // enable optimize read - some cards may timeout. Disable if you're having problems
  card.partialBlockRead(true);
  
  // Now we will look for a FAT partition!
  uint8_t part;
  for (part = 0; part < 5; part++) {   // we have up to 5 slots to look in
    if (vol.init(card, part)) 
      break;                           // we found one, lets bail
  }
  if (part == 5) {                     // if we ended up not finding one  :(
    error("No valid FAT partition!");  // Something went wrong, lets print out why
  }
  
  // Lets tell the user about what we found
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(), DEC);     // FAT16 or FAT32?
  
  // Try to open the root directory
  if (!root.openRoot(vol)) {
    error("Can't open root dir!");      // Something went wrong,
  }

  putstring_nl("Files found (* = fragmented):");

  // Print out all of the files in all the directories.
  root.ls(LS_R | LS_FLAG_FRAGMENTED);


  // Add the leds and set the brightness
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness( BRIGHTNESS );  

  // Set the input pins for the joystick
  pinMode(A0, INPUT_PULLUP);
  pinMode(A1, INPUT_PULLUP);
  pinMode(A2, INPUT_PULLUP);
  pinMode(A3, INPUT_PULLUP);
}
