//------------------------------------------------------------------------------------------------------------------------
// credit

// Hesitant Metis (the only dev :<)
// XTronical (basic template)

//#include <AnimatedGIF.h>
#include "FS.h"
#include "SD_MMC.h"
#include <string>
#include "esp_dsp.h"
//#include <cmath>

#include "animation.h"
#include "icon.h"
#include "ada_font_5x7.h"

#include "esp_heap_caps.h"
#include <FreeRTOS.h>

#include "SPI.h"
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TFT_eSPI.h>


#include "driver/i2s.h"  // Library of I2S routines, comes with ESP32 standard install


#define version "v.0.9.0c"  //testing putting 3 screen mode in 1 task // later note: made some optimistation and its actually more reponsisve now, still wanna try it though// later note: nope working on inner song most og the time
/*

note: crash will somehow reset the active time

some chages to the song index, added img to make the img get process first before the text scroll

added wait skip when using skip or back button

changes to the counting time

small change to the config file, changing spi speed from 40mhz to 240mhz, if it doesnt crash, what ever

noticing that thing are running slowr with EQ 

buffer incease to 15Kb


*/

//idea, it would be smart to use some of those touch pad ic to the button
String Version = version;

#define Back_pin 46
#define Skip_pin 45
#define PP_pin 35
#define SM_pin 47
#define RW_pin 42  //temp pin
#define UP_pin 41
#define DOWN_pin 40


//#define DISPLAY_WIDTH 240
//#define DISPLAY_HEIGHT 240

#define TFT_CS 15
#define TFT_RST 14  // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC 10

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define I2S_DOUT 5  // i2S Data out oin
#define I2S_BCLK 6  // Bit clock
#define I2S_LRC 7   // Left/Right clock, also known as Frame clock or word select
#define I2S_NUM 0   // i2s port number


int clk = 2;
int cmd = 1;
int d0 = 4;


bool convert = 1;

TFT_eSPI tft = TFT_eSPI();

TFT_eSprite sprite = TFT_eSprite(&tft);
//TFT_eSprite small_menu_sprite = TFT_eSprite(&tft);
TFT_eSprite small_songs_menu = TFT_eSprite(&tft);
TFT_eSprite art120_sprite = TFT_eSprite(&tft);
//TFT_eSprite wave_sprite = TFT_eSprite(&tft);

//TFT_eSprite inner_songs = TFT_eSprite(&tft);

TFT_eSprite inner_songs_page1 = TFT_eSprite(&tft);
TFT_eSprite inner_songs_page2 = TFT_eSprite(&tft);

TFT_eSprite ani_sprite = TFT_eSprite(&tft);

TFT_eSprite sd_insert_ani_sprite = TFT_eSprite(&tft);
TFT_eSprite sd_read_ani_sprite = TFT_eSprite(&tft);


#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//AnimatedGIF gif;


#define NUM_BYTES_TO_READ_FROM_FILE 1024 * 12  // How many bytes to read from wav file at a time
#define NUM_BYTES_TO_READ_REVERSE 1024 * 12

//  I2S configuration
static const i2s_config_t i2s_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = 44100,  // Note, this will be changed later
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
  .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,  // high interrupt priority
  .dma_buf_count = 8,                        // 8 buffers
  .dma_buf_len = 64,                         // 64 bytes per buffer, so 8K of buffer space
  .use_apll = 0,
  .tx_desc_auto_clear = true,
  .fixed_mclk = -1
};


static const i2s_pin_config_t pin_config = {
  .bck_io_num = I2S_BCLK,           // The bit clock connectiom, goes to pin 27 of ESP32
  .ws_io_num = I2S_LRC,             // Word select, also known as word select or left right clock
  .data_out_num = I2S_DOUT,         // Data out from the ESP32, connect to DIN on 38357A
  .data_in_num = I2S_PIN_NO_CHANGE  // we are not interested in I2S data into the ESP32
};

struct WavHeader_Struct {
  //   RIFF Section
  char RIFFSectionID[4];  // Letters "RIFF"
  uint32_t Size;          // Size of entire file less 8
  char RiffFormat[4];     // Letters "WAVE"

  //   Format Section
  char FormatSectionID[4];  // letters "fmt"
  uint32_t FormatSize;      // Size of format section less 8
  uint16_t FormatID;        // 1=uncompressed PCM
  uint16_t NumChannels;     // 1=mono,2=stereo
  uint32_t SampleRate;      // 44100, 16000, 8000 etc.
  uint32_t ByteRate;        // =SampleRate * Channels * (BitsPerSample/8)
  uint16_t BlockAlign;      // =Channels * (BitsPerSample/8)
  uint16_t BitsPerSample;   // 8,16,24 or 32

  // Data Section
  char DataSectionID[4];  // The letters "data"
  uint32_t DataSize;      // Size of the data that follows
} WavHeader;
//------------------------------------------------------------------------------------------------------------------------
#pragma pack(push, 2)  //the default is reading 4 byte, using this make it reading size to be more presice
struct bmp_header_S {
  //just taking the basic
  char BM[2];  // BM 0

  uint32_t FileSize;  //skip 4 byte
  uint32_t Reserved;  //skip 4 byte

  uint32_t ImgDataStart;  // max at 54 byte 10
  uint32_t HeaderSize;    // max at 40 byte 14
  uint32_t Width;         //18
  uint32_t Height;        //22

  uint16_t ColorPlanes;  //skip 2 byte

  uint16_t colortye;           //28
  uint32_t CompressionMethod;  //30
  uint32_t RawImgSize;         //34

  // the who care section
  uint32_t XPixelsPerMeter;
  uint32_t YPixelsPerMeter;
  uint32_t ColorsUsed;
  uint32_t ImportantColors;

} bmpheader;

#pragma pack(pop)

struct Songname_Struct {
  String songsnameS[1000];
} Songname;

struct Biquad {
  float b0, b1, b2;
  float a1, a2;

  float z1 = 0.0;
  float z2 = 0.0;
};

Biquad BLeft[9];
Biquad BRight[9];

Biquad RBLeft[9];
Biquad RBRight[9];

struct hann_w {
  float hann_win[NUM_BYTES_TO_READ_FROM_FILE];
};
hann_w hann;

//------------------------------------------------------------------------------------------------------------------------

//  Global Variables/objects

//File f;  // for gif

File WavFile;  // Object for root of SD card directory
File BMPIMG;
File BMPIMG120;
File songsatroot;  //must be in public
File song;

static const i2s_port_t i2s_num = I2S_NUM_0;  // i2s port number

//------------------------------------------------------------------------------------------------------------------------
File logread;
File check;
File SDinfo;
File activetime;
File EQFile;
File autoupdate;

TaskHandle_t wavedraw;
//TaskHandle_t audioread;
TaskHandle_t taskdrawoled;
//TaskHandle_t taskdrawartcover;
TaskHandle_t taskartcover;
TaskHandle_t tasknoartcover;
TaskHandle_t taskreadingwavfile;
TaskHandle_t taskplaybackspeed;
TaskHandle_t taskdraw120art;
TaskHandle_t taskmenu;
TaskHandle_t taskcounttime;
TaskHandle_t taskdrawsprite;
TaskHandle_t taskEQdelta;
//TaskHandle_t taskmenu;


//getting things
QueueHandle_t waveform_queue;
QueueHandle_t byteredfortime;
QueueHandle_t getsongname;
QueueHandle_t getplaystate;

//sending
QueueHandle_t sendsample;
QueueHandle_t Rsendsample;

//encounter some weird problem with vTaskSuspend and ressume

//QueueHandle_t for telling the task when to run
QueueHandle_t run_draw_wave;
QueueHandle_t run_draw_cover_art;
QueueHandle_t run_draw_no_cover_art;
QueueHandle_t allow_run_draw_cover_art;
QueueHandle_t allow_run_draw_wave;

//wave drawing // must be here for the queue
int8_t zoom = 1;
int waveget = 240 / zoom;

bool notify = false;

unsigned long sec_get = 0;
unsigned long* P_sec_get = &sec_get;


//#define TFT_MAD_BGR 0x00
void setup() {
  // gainset();
  pinMode(4, INPUT_PULLUP);

  pinMode(PP_pin, INPUT_PULLDOWN);    // p/p
  pinMode(SM_pin, INPUT_PULLDOWN);    //SM
  pinMode(Back_pin, INPUT_PULLDOWN);  //back
  pinMode(Skip_pin, INPUT_PULLDOWN);  // skip
  pinMode(RW_pin, INPUT_PULLDOWN);    //temporary as rewind
  pinMode(41, INPUT_PULLDOWN);        // temp up
  pinMode(40, INPUT_PULLDOWN);        // temp down
                                      // pinMode(39, INPUT_PULLDOWN);
  // pinMode(38, INPUT_PULLDOWN);
  // pinMode(37, INPUT_PULLDOWN);
  // pinMode(36, INPUT_PULLDOWN);
  // pinMode(34, INPUT_PULLDOWN);
  // pinMode(33, INPUT_PULLDOWN);
  // pinMode(21, INPUT_PULLDOWN);

  Serial.begin(115200);  // Used for info/debug

  // temperature_sensor_config_t temp_sensor = {
  //   .range_min = -10,
  //   .range_max = 80,
  // };

  // ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor, &temp_handle));

  // ESP_ERROR_CHECK(temperature_sensor_enable(temp_handle));


  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  //$$\text{Array Index} = (\text{ASCII Value} - \mathbf{32}) \times \mathbf{5}$$
  display.display();
  display.clearDisplay();
  logooled();
  display.display();
  display.setTextWrap(0);
  //-------------------------------------------------------
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
  i2s_set_pin(i2s_num, &pin_config);
  //-------------------------------------------------------------------------------
  if (!SD_MMC.setPins(clk, cmd, d0)) {
    Serial.println("Pin change failed!");
    //return;
  }
  if (!SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_HIGHSPEED)) {
    Serial.println("Card Mount Failed");
    //return;
  }
  //-------------------------------------------------------
  // create file

  EQFile = SD_MMC.open("/.EQ_save.txt", FILE_READ);
  // File file = SD_MMC.open("/test.txt", FILE_WRITE);
  EQFile.close();

  String time;
  activetime = SD_MMC.open("/.ActiveTime.txt", FILE_READ);  // must be here due to other file bmp file reading function block it from reading, i could be wrong but iam tired

  while (1) {
    time = activetime.readStringUntil('\n');
    Serial.println(time);
    if (time.substring(0, 2) == "T:") {
      time = time.substring(2, time.length());
      *P_sec_get = time.toInt();
      break;
    }
    if (activetime.available() == false) {
      *P_sec_get = 0;
      break;
    }
  }
  //---------------------------------------------------------------------------

  //SPI.setFrequency(16000000L);  //if there are display problem, change this maybe//really doesnt seem to cahnge anything
  tft.init();
  //tft.setSwapBytes(true);
  //tft.setSPISpeed(80000000L);  // speed to high affect the draw function
  tft.fillScreen(0);
  tft.setRotation(0);
  tft.setTextWrap(0);




  sprite.createSprite(240, 240);
  // small_menu_sprite.createSprite(10, 10);
  small_songs_menu.createSprite(101, 88);  //bound for max at 17 characters

  //wave_sprite.createSprite(240, 60);  //bound for max at 17 characters

  art120_sprite.createSprite(120, 120);  //reduce from 120x120 to hide the inperfect

  sd_read_ani_sprite.createSprite(30, 20);

  //inner_songs.createSprite(240, 208);

  inner_songs_page1.createSprite(108, 208);
  inner_songs_page2.createSprite(108, 208);

  small_songs_menu.setTextWrap(0);

  sprite.setSwapBytes(true);
  art120_sprite.setSwapBytes(true);
  ani_sprite.setSwapBytes(1);
  //inner_songs.setSwapBytes(1);
  //inner_songs.setTextWrap(0);


  inner_songs_page1.setSwapBytes(1);
  inner_songs_page2.setSwapBytes(1);

  inner_songs_page1.setTextWrap(0);
  inner_songs_page2.setTextWrap(0);

  intro();
  EQinit();




  //-----------------------------------------------
  //getting things
  waveform_queue = xQueueCreate(waveget, sizeof(int16_t));
  byteredfortime = xQueueCreate(1, sizeof(uint32_t));
  getsongname = xQueueCreate(2, sizeof(String));
  getplaystate = xQueueCreate(1, sizeof(bool));
  sendsample = xQueueCreate(1, sizeof(byte[NUM_BYTES_TO_READ_FROM_FILE]));
  Rsendsample = xQueueCreate(1, sizeof(byte[NUM_BYTES_TO_READ_REVERSE]));

  //controll run task
  run_draw_wave = xQueueCreate(1, sizeof(bool));
  run_draw_cover_art = xQueueCreate(1, sizeof(bool));
  run_draw_no_cover_art = xQueueCreate(1, sizeof(bool));
  allow_run_draw_cover_art = xQueueCreate(1, sizeof(bool));
  allow_run_draw_wave = xQueueCreate(1, sizeof(bool));


  xTaskCreatePinnedToCore(ReadFileTask, "ReadFileTask", 10000, NULL, 15, &taskreadingwavfile, 0);

  xTaskCreatePinnedToCore(artcover, "artcover", 60000, NULL, 14, &taskartcover, 0);

  xTaskCreatePinnedToCore(coverart120, "coverart120", 22000, NULL, 9, &taskdraw120art, 0);

  xTaskCreatePinnedToCore(drawSpritetask, "drawSpritetask", 20000, NULL, 8, &taskdrawsprite, 0);

  xTaskCreatePinnedToCore(drawoled, "drawoled", 5000, (void*)&WavHeader, 8, &taskdrawoled, 0);
  xTaskCreatePinnedToCore(PlayBackSpeed, "PlayBackSpeed", 5000, NULL, 8, &taskplaybackspeed, 0);

  xTaskCreatePinnedToCore(noartcover, "noartcover", 5000, NULL, 5, &tasknoartcover, 0);
  xTaskCreatePinnedToCore(waveform, "drawwaveform", 10000, NULL, 9, &wavedraw, 0);

  xTaskCreatePinnedToCore(counttime, "counttime", 5000, NULL, 4, &taskcounttime, 0);

  xTaskCreatePinnedToCore(main_menu, "main_menu", 10000, NULL, 7, &taskmenu, 0);

  xTaskCreatePinnedToCore(EQslider, "EQ", 5000, NULL, 5, &taskEQdelta, 0);

  xTaskNotifyGive(taskcounttime);





  Serial.print("RAM lelf: ");
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_EXEC));

  Serial.print("PSRAM lelf: ");
  Serial.println(ESP.getFreePsram());

  tft.fillScreen(0);

  //for (int i = 0; i < NUM_BYTES_TO_READ_FROM_FILE; i++) {
  // hann_window(hann);
  //  }
}


bool flag = true;

int playindex = 142 - 1;

//pointer section
//========================

float* P_Gain[9];

uint16_t SongAmount = 0;
uint16_t* P_SongAmount = &SongAmount;

uint32_t sdusedsize = 0;
uint32_t* P_sdusedsize = &sdusedsize;

uint16_t selected_song_index = playindex;
uint16_t* P_selected_song_index = &selected_song_index;

uint16_t songs_menu_index = songs_menu_index;
uint16_t* P_songs_menu_index = &songs_menu_index;

unsigned long timerun_sec = 0;
unsigned long* P_timerun_sec = &timerun_sec;

unsigned long timeout_millis = 0;
unsigned long* P_timeout_millis = &timeout_millis;

unsigned long bytetoread = 0;
unsigned long* P_bytetoread = &bytetoread;

int8_t screen_state = 0;
int8_t* P_screen_state = &screen_state;

int8_t processing_band = 0;
int8_t* P_processing_band = &processing_band;

uint8_t vol_state = 9;  // from 1 to 10
uint8_t* P_vol_state = &vol_state;

float gainsum = 0;
float* P_gainSum = &gainsum;

float screen_mode = 0;
float* P_screen_mode = &screen_mode;  // this is a function controlled version

//==== notification

bool reverse_yes = 0;  //only revrese when the sample is done playing
bool* P_reverse_yes = &reverse_yes;

bool normal_yes = 1;  //only normal playback when the sample is done playing
bool* P_normal_yes = &normal_yes;


bool reverse_data_R = 0;  //only revrese when the sample is done playing
bool* P_reverse_data_R = &reverse_data_R;

bool normal_data_R = 0;  //only normal playback when the sample is done playing
bool* P_normal_data_R = &normal_data_R;


bool Aupdate = 0;  //auto update
bool* P_Aupdate = &Aupdate;

bool song_changed = false;
bool* P_song_changed = &song_changed;

bool menu_selected_song = false;
bool* P_menu_selected_song = &menu_selected_song;

bool all_menu_reset = false;
bool* P_all_menu_reset = &all_menu_reset;

bool menu_black = false;
bool* P_menu_black = &menu_black;

bool menu_done_draw = true;
bool* P_menu_done_draw = &menu_done_draw;

bool update_done = false;
bool* P_update_done = &update_done;

bool songs_menu_song_change = true;
bool* P_songs_menu_song_change = &songs_menu_song_change;

bool new_song = false;
bool* P_new_song = &new_song;

bool playstate = true;
bool* P_playstate = &playstate;

bool draw_wave_done = true;
bool* P_draw_wave_done = &draw_wave_done;

bool EQdelta_no_run_while_audio_processing = false;  //true == pause
bool* P_EQdelta_no_run_while_audio_processing = &EQdelta_no_run_while_audio_processing;

bool no_art_draw_while_wave_run = true;
bool* P_no_art_draw_while_wave_run = &no_art_draw_while_wave_run;

bool song_complete = false;  //song_complete is for when a song run to it end
bool* P_song_complete = &song_complete;

bool change_song_notice = false;  //change_song_notice is for skip/bavk
bool* P_change_song_notice = &change_song_notice;

bool art120done = false;
bool* P_art120done = &art120done;

bool changing_song = false;
bool* P_changing_song = &changing_song;

//ool skip_back_available = false;
//ool* P_skip_back_available = &skip_back_available;
//



//===========================
unsigned long snapshot1;  // for getting time
unsigned long snapshot2;

//song speed and vol
uint8_t Speed = 1;
uint8_t* P_Speed = &Speed;
float vol = 0.7;  //max 1

bool change_screen = false;

long wait1 = 0;
long wait2 = 0;

bool change_flag = true;

bool flagred = false;

bool force_change = false;

bool playpause = true;

int8_t art_type = 0;

bool one = false;  // this is for the drawing part mainly for artcover and noartcover make them only draw once and reset when switching to new screen




void loop() {
  // static byte clearsample[NUM_BYTES_TO_READ_FROM_FILE];

  static bool skip_wait = false;
  static bool hold_skip = 0;
  static bool hold_back = 0;
  static bool hold_menu_song_change = 0;



  //bool drawwavedone;
  static uint8_t wait = 0;
  // Serial.print("*P_reverse_data_R ");
  // Serial.println(*P_reverse_data_R);
  //
  // Serial.print("*P_normal_data_R ");
  // Serial.println(*P_normal_data_R);



  if (flag == false) {
    *P_playstate = PlayPause();
    // xQueueSend(getplaystate, &playpause, 0);
    if (*P_playstate == false) {  // play and pause button controll the playwav

      if (*P_reverse_yes == true && *P_normal_data_R == false) {

        ReversePlayback();

      } else if (*P_normal_yes == true && *P_reverse_data_R == false) {

        PlayWav();

        if (*P_song_complete == true) {
          *P_song_complete = false;
          flag = true;
          wait1 = millis();
          one = true;
        }
      }
    } else {
      xTaskNotifyGive(taskcounttime);
    }
  }

  // xTaskNotifyGive(wavedraw);

  wait2 = millis();

  //========================
  if (*P_screen_state != 0) {

    if (backbutton() == true || hold_back == true) {  // back and skip song// here so it ouwld reset the song time
      hold_back = true;
      *P_changing_song = true;

      // art_type = openingbmp(Songname.songsnameS[playindex - 1]);

      // Serial.println("waiting to skip");
      if (*P_playstate == true || *P_normal_data_R == false) {
        //Serial.println("skip");
        playindex -= 2;
        *P_change_song_notice = true;
        *P_song_changed = true;
        flag = true;
        skip_wait = true;
        hold_back = false;
      }
    }

    if (skipbutton() == true || hold_skip == true) {
      hold_skip = true;
      *P_changing_song = true;

      // art_type = openingbmp(Songname.songsnameS[playindex + 1]);

      if (*P_playstate == true || *P_normal_data_R == false) {
        flag = true;
        *P_change_song_notice = true;
        *P_song_changed = true;
        skip_wait = true;
        hold_skip = false;
      }
    }
  }

  if (*P_menu_selected_song == true || hold_menu_song_change == true) {
    hold_menu_song_change = true;
    *P_changing_song = true;
    // art_type = openingbmp(Songname.songsnameS[*P_selected_song_index]);
    if (*P_playstate == true || *P_normal_data_R == false) {
      playindex = (*P_selected_song_index) - 1;
      *P_song_changed = true;
      *P_change_song_notice = true;
      *P_menu_selected_song = false;
      flag = true;
      skip_wait = true;
      hold_menu_song_change = false;
      //;
    }
  }

  if (*P_update_done == true) {
    *P_update_done = false;
    playindex -= 1;
    *P_song_changed = true;
    *P_change_song_notice = true;
    *P_menu_selected_song = false;
    flag = true;
    skip_wait = true;
  }
  //==============================

  //Serial.println();

  if (ScreenMode(10) == 2) {
    xTaskNotifyGive(wavedraw);

    *P_no_art_draw_while_wave_run = true;
    one = false;
    *P_all_menu_reset = true;
    *P_menu_black = true;

    // wait = 0;
  } else if (ScreenMode(10) == 1) {
    *P_no_art_draw_while_wave_run = false;
    if (*P_song_changed == false) {  // prevent it from running  xTaskNotifyGiv twice

      if (*P_draw_wave_done == true && *P_menu_done_draw == true) {  // wait for the wave drawing to be done then draw artcover

        if (one == false) {

          if (timeout_artcover(50) == true) {
            if (art_type == 1) {
              xTaskNotifyGive(tasknoartcover);

              one = true;

            } else if (art_type == 2) {
              //sprite.pushSprite(0, 0);
              xTaskNotifyGive(taskartcover);
              one = true;
            }
          }
        }

        wait = 0;
      }

      if (one == false) {  //odd, drawing only work when this is here, v0.2.6b
        wait++;
      }
    }
  } else if (ScreenMode(10) == 0) {
    one = false;

    if (*P_draw_wave_done == true) {
      xTaskNotifyGive(taskmenu);
    }
  }
  //========================




  //put a delay of 1s or 2s after done playing using milis// sometime queue reading can be a liitlr slow to catch up to the actuall song time
  // delay for 1s as for the msaking to work and also for not play imidiatly for artistic reason
  if (wait2 >= wait1 + 1000 || skip_wait == true) {
    if (flag == true) {
      *P_new_song = true;
      skip_wait = false;




      if (playindex > *P_SongAmount - 1) {  // restart1 prevent reading above songamountnum
        playindex = 0;
      }

      if (playindex < 0) {  // prevent going below 0
        playindex = *P_SongAmount - 1;
      }


      Serial.print("playing ");
      Serial.print(Songname.songsnameS[playindex]);
      Serial.print(" ");
      Serial.println(playindex);

      xQueueSend(getsongname, &Songname.songsnameS[playindex], 0);



      WavFile = SD_MMC.open(Songname.songsnameS[playindex]);  // Open the wav file
      art_type = openingbmp(Songname.songsnameS[playindex]);
      //if ( == true) {
      //  //printBMPHeader (&bmpheader);
      //}

      if (WavFile == false) {
        Serial.println("Could not open this file");
        flag = true;
      } else {
        WavFile.read((byte*)&WavHeader, 44);  // Read in the WAV header, which is first 44 bytes of the file.
                                              // We have to typecast to bytes for the "read" function
        //DumpWAVHeader(&WavHeader);                                      // Dump the header data to serial, optional!
        if (ValidWavData(&WavHeader))                                   // optional if your sure the WAV file will be valid.
          i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * Speed);  //set sample rate
        flag = false;
        *P_processing_band = 20;  //Processing for every new song// could optimise by only process if the sample rate change
        xTaskNotifyGive(taskEQdelta);
        *P_changing_song = false;
      }
      // Serial.println(EQslider());
      if (playindex > *P_SongAmount - 1) {  // restart2 // dont know exactlly but it need two
      } else {
        playindex++;
      }
      //*P_selected_song_index = index;

      xTaskNotifyGive(taskreadingwavfile);
      //init once for now
    }
  }

  xTaskNotifyGive(taskdrawoled);
}


void waveform(void* pvParameters) {
  int16_t currentSample = 0;
  int16_t arr[1];
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (uxQueueMessagesWaiting(waveform_queue) == waveget) {  //only run when there is enough data

      for (int i = -zoom; i <= 241; i += zoom) {
        if (xQueueReceive(waveform_queue, &currentSample, 0) == pdPASS) {
        }

        float posamp = currentSample / 550;


        tft.fillRect(i + 1, 0, zoom, 240, 0);  //draw clearing line // must be one step ahead
        tft.drawFastHLine(i, 120, 1, 0x9492);  // draw mid line

        arr[1] = posamp;
        tft.drawLine(i, arr[0] + 120, i + zoom, arr[1] + 120, RGBcolorBGR(convert, 0xfd00));  // draw connecting line from point
        arr[0] = arr[1];
        *P_draw_wave_done = false;
      }
    }
    //wave_sprite.pushSprite(0, 60);

    //if (*P_screen_state == 0) {
    //  tft.fillScreen(0);
    //  // if (timeout_wave(1000) == true) {
    //  *P_draw_wave_done = true;
    //  //}
    //} else {
    *P_draw_wave_done = true;
    // }
    ////if()

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}


//===============
void drawoled(void* pvParameters) {

  uint32_t bytetime = 10;
  String name;
  String nameog;
  String namecheck;
  String nameartis;
  String namesong;
  int strL;

  int rolling;
  int rolling2;

  bool playback_state;


  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    int count = 0;
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setTextColor(1);


    if (xQueueReceive(byteredfortime, &bytetime, pdMS_TO_TICKS(10)) == pdPASS) {
    }
    if (xQueueReceive(getsongname, &nameog, pdMS_TO_TICKS(10)) == pdPASS) {
    }
    // if (xQueueReceive(getplaystate, &playback_state, pdMS_TO_TICKS(10)) == pdPASS) {
    // }
    //song name processing part
    //------------------------------------------------
    if (namecheck != nameog) {
      namecheck = nameog;  // check for new name, prevent it from runing all the time
      name = namecheck;

      rolling = 0;  // must reset or if two song with long name run in sequence the next song name will disappear
      rolling2 = 0;

      while (count <= name.length()) {
        if (name.charAt(0) == '/') {
          name = name.substring(1, name.length());
        }
        if (name.charAt(name.length() - 4) == '.' && name.charAt(name.length() - 3) == 'w' && name.charAt(name.length() - 2) == 'a' && name.charAt(name.length() - 1) == 'v') {
          name = name.substring(0, name.length() - 4);
        }
        if (name.charAt(count) == '-') {
          if (name.charAt(count - 1) == ' ') {
            nameartis = name.substring(0, count - 1);
          } else {
            nameartis = name.substring(0, count);
          }

          if (name.charAt(count + 1) == ' ') {
            namesong = name.substring(count + 2);
          } else {
            namesong = name.substring(count + 1);
          }

          count = name.length();
        }
        count++;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }
    //------------------------------------------------

    //displaying parts

    if (namesong == "dU5k M3Igb VkgN Wsxbg") {
      //Serial.println("glitching");

      switch (random(0, 4)) {
        case 0:
          display.startscrollleft(0, 100);
          break;
        case 1:
          display.startscrolldiagleft(0, 100);
          break;
        case 2:
          display.startscrollright(0, 100);
          break;
        case 3:
          display.startscrolldiagright(0, 100);
          break;
      }
    } else {
      display.stopscroll();
    }

    //display.startscrollleft(0, 20); cool glitch effect if use this wrongly

    //display song name
    int pxstrlen = 5 * nameartis.length() + nameartis.length() - 1;  // calculating the string pixel lenght, at size 1: 5x7px
    //Serial.println(nameartis.length());
    if (nameartis.length() > 21) {
      display.setCursor(rolling, 2);
      display.println(nameartis);
      display.setCursor(rolling + pxstrlen + 20, 2);
      display.println(nameartis);
      rolling--;
      if (rolling + pxstrlen + 20 == 0) {  // scrolls text if longer than 21 character
        rolling = 0;
      }
    } else {
      display.setCursor(64 - (pxstrlen / 2), 2);  //center the name
      display.println(nameartis);
    }

    int pxstrlen2 = 5 * namesong.length() + namesong.length() - 1;
    if (namesong.length() > 21) {
      display.setCursor(rolling2, 22);
      display.println(namesong);
      display.setCursor(rolling2 + pxstrlen2 + 20, 22);
      display.println(namesong);
      rolling2--;
      if (rolling2 + pxstrlen2 + 20 == 0) {
        rolling2 = 0;
      }
    } else {
      display.setCursor(64 - (pxstrlen2 / 2), 22);  // center the name
      display.println(namesong);
    }

    // display time of a song
    float DataSize = WavHeader.DataSize;
    float ByteRate = WavHeader.ByteRate;
    float time = DataSize / ByteRate;
    playtime(time, 98, 52);  //runtime
    float times = float(bytetime) / DataSize * time;
    playtime(times, 1, 52);  // running time

    display.drawFastHLine(2, 41, times * (125.0 / time), 1);  //moving bar
    display.drawFastHLine(0, 42, 128, 1);
    display.drawFastHLine(2, 43, times * (125.0 / time), 1);

    display.drawFastVLine(0, 37, 11, 1);
    display.drawFastVLine(1, 38, 9, 1);

    display.drawFastVLine(127, 37, 11, 1);
    display.drawFastVLine(126, 38, 9, 1);


    //display icon
    if (digitalRead(RW_pin) == HIGH) {
      display.drawBitmap(56, 50, reverse_icon, 11, 11, 1);
    } else {
      if (*P_playstate == false) {
        display.drawBitmap(63 - 3, 50, pause_icon, 6, 11, 1);
      } else {
        display.stopscroll();
        display.drawBitmap(63 - 3, 50, play_icon, 6, 11, 1);
      }
    }

    display.drawBitmap(74, 50, skip_icon, 12, 11, 1);
    display.drawBitmap(40, 50, back_icon, 12, 11, 1);



    display.display();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
void playtime(int time, int x, int y) {
  uint8_t sec = time % 60;
  uint8_t min = (time / 60) % 60;
  uint8_t hr = (time / 3600) % 24;

  if (hr > 0) {
    display.setCursor(x - 6 * 2, y);
    display.printf("%d:", hr);
  }

  display.setCursor(x, y);
  if (min < 10) {
    display.printf("0%d:", min);
  } else {

    display.print(min);
    display.print(":");
  }

  display.setCursor(3 * 5 + 2 + x, y);
  if (sec < 10) {
    display.printf("0%d", sec);
  } else {
    display.print(sec);
  }
}
void logooled() {
  display.drawBitmap(16, 24, metislogooled, 96, 16, 1);
  display.setTextSize(1);
  display.setTextColor(1);

  display.setCursor(12, 43);
  display.println("Music Player");

  if (Version.length() > 7) {
    display.setCursor(69, 53);
  } else {
    display.setCursor(75, 53);
  }
  display.println(Version);
}
//===============

//===== main i2s data processing and writing flow section
void ReadFileTask(void* pvParameters) {
  static byte Sample[NUM_BYTES_TO_READ_FROM_FILE];
  static byte RSample[NUM_BYTES_TO_READ_REVERSE];
  //

  //byte* Sample = NULL;
  //byte* RSample = NULL;


  static uint32_t BytesReadSoFar = 0;  // Number of bytes read from file so far
  uint16_t BytesToRead;                // Number of bytes to read from the file
  uint16_t i;                          // loop counter
  int16_t SignedSample;                // Single Signed Sample
  static long seek_to_byte = 0;

  int end_sample_index;
  int16_t Startptpos;
  int16_t Endptpos;

  while (1) {

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // xTaskNotifyGive(taskEQdelta);
    *P_EQdelta_no_run_while_audio_processing = true;


    //Serial.print("RAM lelf: ");
    //Serial.println(heap_caps_get_free_size(MALLOC_CAP_EXEC));

    // Sample = (byte*)heap_caps_malloc(NUM_BYTES_TO_READ_FROM_FILE, MALLOC_CAP_SPIRAM);
    // RSample = (byte*)heap_caps_malloc(NUM_BYTES_TO_READ_REVERSE, MALLOC_CAP_SPIRAM);

    //EQslider();


    if (*P_reverse_yes == true) {  //reverse reading when high
                                   //Serial.println("process");
                                   // for reading reverse while

      *P_reverse_data_R = true;

      BytesReadSoFar -= NUM_BYTES_TO_READ_REVERSE;

      seek_to_byte = BytesReadSoFar;
      if (seek_to_byte <= 44) {
        seek_to_byte = 44;
      }
      //Serial.print("seek_to_byte: ");
      //Serial.println(seek_to_byte);

      WavFile.seek(seek_to_byte);                        // to read backward it need to find the seek postion which is the current byte postion - how many byte it need to read
      WavFile.read(RSample, NUM_BYTES_TO_READ_REVERSE);  // reading forward is the fastes way to get data

      // swaping position// must be before EQ to avoid poping
      //---------------------------------
      int16_t* data_ptr = (int16_t*)RSample;
      const int NUM_SAMPLES = NUM_BYTES_TO_READ_REVERSE / 2;

      for (int s = 0; s < NUM_SAMPLES / 2; s++) {  // revese process
        Startptpos = data_ptr[s];
        data_ptr[s] = data_ptr[NUM_SAMPLES - 1 - s];
        data_ptr[NUM_SAMPLES - 1 - s] = Startptpos;
      }

      EQ(RSample, NUM_BYTES_TO_READ_REVERSE, 1);

      xQueueSend(byteredfortime, &seek_to_byte, 0);

      BytesReadSoFar = seek_to_byte;  //transfer over
      //*P_bytetoread = NUM_BYTES_TO_READ_REVERSE;

      xQueueSend(Rsendsample, &RSample, 0);
      //Serial.println("done");
      // Serial.println("done reverse");

      // memset(RSample, 0, NUM_BYTES_TO_READ_REVERSE);
      // heap_caps_free(RSample);


    } else if (*P_normal_yes == true) {
      //Serial.println("process");
      *P_normal_data_R = true;

      if (*P_song_changed == true) {  // if song change is detected this will reset the reading// this is for skip/bakc
        //*P_song_complete = true;
        //BytesToRead = NUM_BYTES_TO_READ_FROM_FILE + 1000; // remove due to double skip
        BytesReadSoFar = 0;
        *P_song_changed = false;
        //memset(Sample, 0, NUM_BYTES_TO_READ_FROM_FILE);
      }

      if (BytesReadSoFar + NUM_BYTES_TO_READ_FROM_FILE > WavHeader.DataSize)  // If next read goes past the end then adjust the
        BytesToRead = WavHeader.DataSize - BytesReadSoFar;                    // amount to read to whatever is remaining to read

      else
        BytesToRead = NUM_BYTES_TO_READ_FROM_FILE;  // Default to max to read

      xQueueSend(byteredfortime, &BytesReadSoFar, 0);
      //byte to read determent how many byte to to be red into the sample
      WavFile.read(Sample, BytesToRead);
      // Read in the bytes from the file

      EQ(Sample, BytesToRead, 0);

      BytesReadSoFar += BytesToRead;  // Update the total bytes red in so far
                                      //Serial.print("BytesReadSoFar: ");
                                      //Serial.println(BytesReadSoFar);


      if (BytesReadSoFar >= WavHeader.DataSize)  // Have we read in all the data?
      {
        // Serial.println("done play");
        *P_song_complete = true;
        // BytesToRead = NUM_BYTES_TO_READ_FROM_FILE + 1000;  //just return unsual number for changing or reset song condition
        BytesReadSoFar = 0;  // Clear to no bytes read in so far
      }
      *P_bytetoread = BytesToRead;

      //if (*P_changing_song == true) {
      //  memset(Sample, 0, NUM_BYTES_TO_READ_FROM_FILE);
      //}

      xQueueSend(sendsample, &Sample, 0);
      // Serial.println("done");
      // Serial.println("done normal");

      // memset(Sample, 0, NUM_BYTES_TO_READ_FROM_FILE);
      // heap_caps_free(Sample);
    }

    *P_EQdelta_no_run_while_audio_processing = false;

    //xQueueReset(sendsample);
  }
}
void PlayWav() {
  static byte Samples[NUM_BYTES_TO_READ_FROM_FILE];
  static uint16_t BytesRead = 0;
  static bool switching = false;

  *P_reverse_yes = false;
  //*P_skip_back_available = false;

  // let the ReadFileTask() process the next section while the song is playing
  // when empty get the next section
  //

  /// Serial.print("Samples1: ");
  /// Serial.println(uxQueueMessagesWaiting(sendsample));

  if (FillI2SBuffer(Samples, BytesRead) == false) {
    //Serial.println("write");

    if (digitalRead(RW_pin) != HIGH) {

      if (switching == false) {  // reading once per
        xTaskNotifyGive(taskreadingwavfile);
        switching = true;
      }
    }  //else {
       //Serial.print("on reverse ");
       //Serial.println(*P_normal_data_R);
    //}

  } else {
    // Serial.println("empty");

    // when the skip/back button is press, only allows them to skip when the buffer is empty
    // memset sample to all 0 just in case just already have the next sextion ready

    //same for reverse button pressing



    // *P_skip_back_available = true;

    switching = false;
    if (xQueueReceive(sendsample, &Samples, pdMS_TO_TICKS(0)) == pdPASS) {
    }

    if (digitalRead(RW_pin) == HIGH && *P_normal_data_R == false) {
      *P_normal_yes = false;
      *P_reverse_yes = true;
      // memset(Samples, 0, NUM_BYTES_TO_READ_FROM_FILE);
    }

    if (*P_changing_song == true) {
      memset(Samples, 0, NUM_BYTES_TO_READ_FROM_FILE);
    }


    *P_normal_data_R = false;
    BytesRead = *P_bytetoread;
  }

  // Serial.print("Samples2: ");
  // Serial.println(uxQueueMessagesWaiting(sendsample));



  // return *P_song_complete;
}
void ReversePlayback() {
  static byte RSamples[NUM_BYTES_TO_READ_REVERSE];
  static uint16_t BytesRead = 0;
  static bool switching = false;
  static bool once = false;

  *P_normal_yes = false;

  // let the ReadFileTask() process the next section while the song is playing
  // when empty get the next section
  // repeat

  //Serial.print("RSamples1: ");
  //Serial.println(uxQueueMessagesWaiting(Rsendsample));



  if (FillI2SBuffer(RSamples, BytesRead) == false) {
    //Serial.println("rwrite");
    if (digitalRead(RW_pin) != LOW) {
      if (switching == false) {
        xTaskNotifyGive(taskreadingwavfile);
        switching = true;
      }
    } else {
      Serial.print("on reverse ");
      Serial.println(*P_reverse_data_R);
    }

  } else {
    //Serial.println("rempty");
    switching = false;
    // when the RW button is press, only allows them to rewind when the buffer is empty
    // memset sample to all 0 just in case just already have the next sextion ready


    if (xQueueReceive(Rsendsample, &RSamples, pdMS_TO_TICKS(0)) == pdPASS) {
    }

    if (digitalRead(RW_pin) == LOW && *P_reverse_data_R == false) {  // block normal playback from getting data
      *P_normal_yes = true;
      *P_reverse_yes = false;
      memset(RSamples, 0, NUM_BYTES_TO_READ_REVERSE);
    }
    *P_reverse_data_R = false;

    BytesRead = NUM_BYTES_TO_READ_REVERSE;
  }

  //Serial.print("RSamples2: ");
  //Serial.println(uxQueueMessagesWaiting(Rsendsample));
}
bool FillI2SBuffer(byte* Samples, uint16_t BytesInBuffer) {  // fully explain
  // Writes bytes to buffer, returns true if all bytes sent else false, keeps track itself of how many left
  // to write, so just keep calling this routine until returns true to know they've all been written, then
  // you can re-fill the buffer

  size_t BytesWritten;            // Returned by the I2S write routine,
  static uint16_t BufferIdx = 0;  // Current pos of buffer to output next
  uint8_t* DataPtr;               // Point to next data to send to I2S
  uint16_t BytesToSend;           // Number of bytes to send to I2S
  //static bool mask = false;
  //static int count = 0;

  if (BytesInBuffer == 0) {
    return true;
  }

  //*P_skip_back_available = false;

  DataPtr = (Samples) + BufferIdx;                             // Set address to next byte in buffer to send out
  BytesToSend = BytesInBuffer - BufferIdx;                     //BytesToSend is the remaining byte to send, it tell write how many bytes left to expect
  i2s_write(i2s_num, DataPtr, BytesToSend, &BytesWritten, 1);  // Send the bytes, wait 1 RTOS tick to complete
  BufferIdx += BytesWritten;                                   // this set the index cursor to the next staring point


  //BytesInBuffer is use to tell the write how many byte is left
  //Samples, is use as a array "decay" to a pointer, it has all the data just in a pointer packet
  // BufferIdx, this set the starting point for the write,
  // the write return how much it has red as BytesWritten and adding it to the BufferIdx and repeat
  // increasue by number of bytes actually written

  if (BufferIdx >= BytesInBuffer) {
    // sent out all bytes in buffer, reset and return true to indicate this
    BufferIdx = 0;
    return true;  // true when fully drain
  } else
    return false;  //false to stop filling when not all data are drained
}
//=================

//=========== Get wave sata section
bool ValidWavData(WavHeader_Struct* Wav) {

  if (memcmp(Wav->RIFFSectionID, "RIFF", 4) != 0) {
    Serial.print("Invalid data - Not RIFF format");
    return false;
  }
  if (memcmp(Wav->RiffFormat, "WAVE", 4) != 0) {
    Serial.print("Invalid data - Not Wave file");
    return false;
  }
  if (memcmp(Wav->FormatSectionID, "fmt", 3) != 0) {
    Serial.print("Invalid data - No format section found");
    return false;
  }
  if (memcmp(Wav->DataSectionID, "data", 4) != 0) {
    Serial.print("Invalid data - data section not found");
    return false;
  }
  if (Wav->FormatID != 1) {
    Serial.print("Invalid data - format Id must be 1");
    return false;
  }
  if (Wav->FormatSize != 16) {
    Serial.print("Invalid data - format section size must be 16.");
    return false;
  }
  if ((Wav->NumChannels != 1) & (Wav->NumChannels != 2)) {
    Serial.print("Invalid data - only mono or stereo permitted.");
    return false;
  }
  if (Wav->SampleRate > 48000) {
    Serial.print("Invalid data - Sample rate cannot be greater than 48000");
    return false;
  }
  if ((Wav->BitsPerSample != 8) & (Wav->BitsPerSample != 16)) {
    Serial.print("Invalid data - Only 8 or 16 bits per sample permitted.");
    return false;
  }
  return true;
}
void DumpWAVHeader(WavHeader_Struct* Wav) {
  if (memcmp(Wav->RIFFSectionID, "RIFF", 4) != 0) {
    Serial.print("Not a RIFF format file - ");
    PrintData(Wav->RIFFSectionID, 4);
    return;
  }
  if (memcmp(Wav->RiffFormat, "WAVE", 4) != 0) {
    Serial.print("Not a WAVE file - ");
    PrintData(Wav->RiffFormat, 4);
    return;
  }
  if (memcmp(Wav->FormatSectionID, "fmt", 3) != 0) {
    Serial.print("fmt ID not present - ");
    PrintData(Wav->FormatSectionID, 3);
    return;
  }
  if (memcmp(Wav->DataSectionID, "data", 4) != 0) {
    Serial.print("data ID not present - ");
    PrintData(Wav->DataSectionID, 4);
    return;
  }
  // All looks good, dump the data
  Serial.print("Total size :");
  Serial.println(Wav->Size);
  Serial.print("Format section size :");
  Serial.println(Wav->FormatSize);
  Serial.print("Wave format :");
  Serial.println(Wav->FormatID);
  Serial.print("Channels :");
  Serial.println(Wav->NumChannels);
  Serial.print("Sample Rate :");
  Serial.println(Wav->SampleRate);
  Serial.print("Byte Rate :");
  Serial.println(Wav->ByteRate);
  Serial.print("Block Align :");
  Serial.println(Wav->BlockAlign);
  Serial.print("Bits Per Sample :");
  Serial.println(Wav->BitsPerSample);
  Serial.print("Data Size :");
  Serial.println(Wav->DataSize);
}
void PrintData(const char* Data, uint8_t NumBytes) {
  for (uint8_t i = 0; i < NumBytes; i++)
    Serial.print(Data[i]);
  Serial.println();
}
//==============


const char* num_to_str(int number) {
  static char str[16];
  snprintf(str, sizeof(str), "%d", number);
  return str;
}

//================ 240x240 Art cover section
int8_t openingbmp(String songname) {
  songname = songname.substring(0, songname.length() - 4);
  songname.concat(".bmp");
  Serial.println(songname);

  activetime.close();

  BMPIMG = SD_MMC.open(songname, FILE_READ);  // Open the wav file

  if (BMPIMG == false) {
    Serial.println("Could not open this file");
    if (*P_no_art_draw_while_wave_run == false) {
      //sprite.fillSprite(TFT_BLACK);
      xTaskNotifyGive(tasknoartcover);
      *P_no_art_draw_while_wave_run == true;
    }
    //
    return 1;
  } else {

    BMPIMG.read((byte*)&bmpheader, 54);

    if (*P_no_art_draw_while_wave_run == false) {
      //sprite.fillSprite(TFT_BLACK);
      xTaskNotifyGive(taskartcover);
      *P_no_art_draw_while_wave_run == true;
    }
    //
    return 2;
  }
}
void printBMPHeader(bmp_header_S* bmp) {

  if (memcmp(bmp->BM, "BM", 2) == 0) {
    Serial.println("This is .bmp");
  } else {
    Serial.println("This is not .bmp");
  }

  Serial.print("bmp->FileSize ");
  Serial.println(bmp->FileSize);

  Serial.print("bmp->ImgDataStart ");
  Serial.println(bmp->ImgDataStart);

  Serial.print("bmp->Height ");
  Serial.println(bmp->Height);

  Serial.print("bmp->Width ");
  Serial.println(bmp->Width);

  Serial.print("bmp->RawImgSize ");
  Serial.println(bmp->RawImgSize);


  Serial.print("bmp->colortye ");
  Serial.println(bmp->colortye);

  //xQueueSend(getBMPcolortype, &bmp->colortye, 0);  // using queue send instead of just passing the struct to the task
}
//no 7 9 50... all number must divide into whole number
const uint8_t rows = 120;
const uint pixel_to_read = 240 * rows;
const uint pixel_chunk = 3 * pixel_to_read;
int shiftdown = -rows;  //239
unsigned long bytered = 0;
//note bmp first pixel 0,0 is at the bottom left
//it read 720 byte mean 240px and cover art are 240x240 so doesnt need to check for remain left
//172800
bool rgb888_to_rgb565_to_tft_rows_PSRAM() {  //converter

  if (*P_new_song == true) {

    int8_t byte1;
    int8_t byte2;
    int8_t byte3;
    // uint16_t* data = NULL;
    BMPIMG.seek(54);

    byte* PixelSamples = NULL;
    PixelSamples = (byte*)heap_caps_malloc(pixel_chunk, MALLOC_CAP_SPIRAM);

    //data = (byte*)heap_caps_malloc(240 * 240 * 2, MALLOC_CAP_SPIRAM);
    for (int run = 0; run < 240 / rows; run++) {  // reading twice

      BMPIMG.read(PixelSamples, pixel_chunk);

      shiftdown += rows;
      int x;
      uint16_t rgb565[pixel_to_read];

      for (int i = 0; i < pixel_to_read; i++) {  // converting rgb888_to_rgb565
        x = i * 3;
        byte1 = PixelSamples[x + 2] >> 3;
        byte2 = PixelSamples[x + 1] >> 2;
        byte3 = PixelSamples[x] >> 3;
        rgb565[i] = (byte3 << 11 | byte2 << 5 | byte1 << 0);
      }

      sprite.pushImage(0, shiftdown, 240, rows, rgb565);
    }

    shiftdown = -rows;  // reset
    memset(PixelSamples, 0, pixel_chunk);

    heap_caps_free(PixelSamples);
    PixelSamples = NULL;
    BMPIMG.close();
    sprite.pushSprite(0, 0);

  } else {
    Serial.println("already have it");
    sprite.pushSprite(0, 0);
    BMPIMG.close();
  }

  *P_new_song = false;
  return false;
}
void artcover(void* pvParameters) {
  // bool run_draw_cover_art_FLAG = false;
  // bool allow_run_draw_cover_art_FLAG = false;
  // uint8_t madctl = ST77XX_MADCTL_MY;
  // uint8_t command_byte = 0x36;
  //uint8_t numdatabyte = 1;
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);


    tft.setRotation(2);
    tft.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
    tft.writedata(TFT_MAD_MY);     // command for mirroring Y // for multi rows
    //sprite.fillSprite(TFT_BLACK);
    rgb888_to_rgb565_to_tft_rows_PSRAM();
    tft.setRotation(0);
  }
  //try use tft espi for the sprite push function for instance dispaly
  //very good performace but for some reason the display art will have pale blue band appear and disapear on them, notice able when lool closely
}
int8_t pxle = 30;
int8_t nocartcover_x = 120;
int8_t nocartcover_y = 120 + 5;
void noartcover(void* pvParameters) {
  bool nocoverart_run_flag = false;
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    //if (nocoverart_run_flag == true) {
    //nocoverart_run_flag = false;
    tft.fillScreen(0);
    //Serial.println("drawing no art cover");
    tft.drawFastHLine(3, 3, pxle, 0xd000);  // up right
    tft.drawFastVLine(3, 3, pxle, 0xd000);

    tft.drawFastHLine(239 - 3 - pxle, 3, pxle, 0xd000);  // up left
    tft.drawFastVLine(239 - 3, 3, pxle, 0xd000);

    tft.drawFastHLine(3, 239 - 3, pxle, 0xd000);  // down right
    tft.drawFastVLine(3, 239 - 3 - pxle, pxle, 0xd000);

    tft.drawFastHLine(239 - 3 - pxle, 239 - 3, pxle, 0xd000);  // down left
    tft.drawFastVLine(239 - 3, 239 - 3 - pxle, pxle, 0xd000);

    // tft.drawFastHLine(0, 120, 240, 0xd000);
    // tft.drawFastVLine(120, 0, 240, 0xd000);

    tft.setCursor(nocartcover_x - 53 / 2, nocartcover_y - 7 / 2);
    tft.setTextSize(1);
    tft.setTextColor(RGBcolorBGR(convert, 0xd000));
    tft.println("240 x 240");  //53px
    tft.setCursor(nocartcover_x - 53 / 2, nocartcover_y - 10 - 7 / 2);

    tft.println("ART COVER");  //53px
    //}


    //vTaskSuspend(NULL);

    // vTaskDelay(pdMS_TO_TICKS(100));
  }
}
//=================


void snapshot_time(unsigned long snap1, unsigned long snap2) {
  float time;
  time = (snap2 - snap1) / 1000000.0;
  //Serial.println(snap2);
  //Serial.println(snap1);
  Serial.print("TIME TOOK: ");
  Serial.print(time, 5);
  Serial.println("s");
}

//output the right side
String manip_song_name(String name) {
  //bool dash = false;
  name = name.substring(0, name.length() - 4);  //remove .wave
  for (uint8_t i = 0; i < name.length(); i++) {
    if (name.charAt(i) == '-') {  //taking the right half

      if (name.charAt(i + 1) == ' ') {
        if (name.charAt(i + 2) == ' ') {
          name = name.substring(i + 3, name.length());
        } else {
          name = name.substring(i + 2, name.length());
        }

        //i// = 0;
        // dash = true;
        // break;
        return name;

      } else {
        name = name.substring(i + 1, name.length());
        // i = 0;
        // dash = true;
        // break;
        return name;
      }
    }
    // if (name.charAt(i) == '(' && dash == true) {  //remove note in title
    //
    //   if (name.charAt(i - 1) == ' ') {
    //     name = name.substring(0, i - 1);
    //     //i = 0;
    //   } else {
    //     name = name.substring(0, i);
    //     // i = 0;
    //   }
    // }
  }
}





void underline(int x, int y) {
  tft.drawFastHLine(27, 107, 59, RGBcolorBGR(convert, 0xD56D));
  tft.drawFastHLine(25, 109, 63, RGBcolorBGR(convert, 0xD56D));
}

//delete later
void testbuttoninput() {
  // if (digitalRead(35) == HIGH) {
  //   Serial.println("Play and pause");
  // }
  // if (digitalRead(47) == HIGH) {
  //   Serial.println("Screen Mode");
  // }
  // if (digitalRead(46) == HIGH) {
  //   Serial.println("Back");
  // }
  // if (digitalRead(45) == HIGH) {
  //   Serial.println("Skip");
  // }
  // if (digitalRead(42) == HIGH) {
  //   Serial.println("Rewind");
  // }
  if (digitalRead(41) == HIGH) {
    Serial.println("up");
  }
  if (digitalRead(40) == HIGH) {
    Serial.println("down");
  }
}



//============ buttons section
int8_t ScreenMode(uint8_t force_set) {
  static int8_t mode = 0;  //0 menu // 1 artcover// 2 wave
  //force_set at 10 mean no forcing
  uint16_t debounceTime = 300;
  static bool lastStableState = 0;  // stable debounced state
  static bool lastRawState = 0;     // last raw reading
  static unsigned long lastChangeTime = 0;
  bool output;

  bool rawState = digitalRead(SM_pin);

  if (rawState != true) {
    if (output == false) {
      if ((millis() - lastChangeTime) < debounceTime) {
        //Serial.println("in time");
        return mode;
      }
    }
  }


  if (rawState != lastStableState) {
    lastRawState = rawState;  // set to the new change aka press
    lastStableState = rawState;
    lastChangeTime = millis();  //set time
  }

  if (lastRawState == 1) {  // if press

    lastRawState = 0;  //set to be 0 for next time
    output = true;

  } else {

    output = false;  // next time come out put to false
  }


  if (output == true) {

    mode++;

    if (mode > 2) {
      mode = 0;
    }
    if (mode < 0) {
      mode = 2;
    }

    *P_screen_state = mode;
    *P_screen_mode = mode;
  }

  if (force_set == 10) {
    return mode;
  } else {
    mode = force_set;
    return mode;
  }
}
bool PlayPause() {
  bool latch = *P_playstate;  // latch but need to consider the play state since it wont be the only one controlling it
  bool static flip_flop = false;
  //latch
  //touch, display, must keep displaying when hold
  //detect taking off
  //detect touch again, display off, must keep off when hold

  if (digitalRead(PP_pin) == HIGH && flip_flop == false) {  //diplay when touch or hold
    latch = true;
    delay(100);
  }
  if (digitalRead(PP_pin) == LOW && latch == true) {  // check for taking off
    flip_flop = true;
  }
  if (digitalRead(PP_pin) == HIGH && flip_flop == true) {  // turn off when touched once more
    latch = false;
    //delay(50);
  }
  if (digitalRead(PP_pin) == LOW && latch == false) {  //check for turning off take off
    flip_flop = false;
  }

  return latch;
}
//output TRUE if press
bool skipbutton() {
  // did use AI but mine is better!!!
  uint16_t debounceTime = 200;
  static bool lastStableState = 0;  // stable debounced state
  static bool lastRawState = 0;     // last raw reading
  static unsigned long lastChangeTime = 0;
  bool output;

  bool rawState = digitalRead(Skip_pin);

  if (rawState != true) {
    if (output == false) {
      if ((millis() - lastChangeTime) < debounceTime) {
        //Serial.println("in time");
        return false;
      }
    }
  }


  if (rawState != lastStableState) {
    lastRawState = rawState;  // set to the new change aka press
    lastStableState = rawState;
    lastChangeTime = millis();  //set time
  }

  if (lastRawState == 1) {  // if press

    lastRawState = 0;  //set to be 0 for next time
    output = true;

  } else {

    output = false;  // next time come out put to false
  }

  //Serial.println(output);


  return output;
}
//output TRUE if press
bool backbutton() {
  uint16_t debounceTime = 200;
  static bool lastStableState = 0;  // stable debounced state
  static bool lastRawState = 0;     // last raw reading
  static unsigned long lastChangeTime = 0;
  bool output;

  bool rawState = digitalRead(Back_pin);

  if (rawState != true) {
    if (output == false) {
      if ((millis() - lastChangeTime) < debounceTime) {
        //Serial.println("in time");
        return false;
      }
    }
  }

  if (rawState != lastStableState) {
    lastRawState = rawState;  // set to the new change aka press
    lastStableState = rawState;
    lastChangeTime = millis();  //set time
  }

  if (lastRawState == 1) {  // if press

    lastRawState = 0;  //set to be 0 for next time
    output = true;

  } else {

    output = false;  // next time come out put to false
  }


  return output;
}
void PlayBackSpeed(void* pvParameters) {
  bool once = false;
  while (1) {
    if (*P_reverse_yes == true && *P_reverse_data_R == false) {  // for reverse
      if (once == false) {
        i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * 1);
        once = true;
      }
    } else if (*P_normal_yes == true && *P_normal_data_R == false) {
      if (once == true) {
        i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * 1);
        once = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
//================





//======== Upadating song section
void Updating_songs(int16_t x, int16_t y) {  //update song file name from sd card  to the txt file
  uint32_t total_used_size = 0;

  File log;
  File Songamountlog;

  int16_t counting_song = 0;

  songsatroot = SD_MMC.open("/");

  Serial.println("rewriting song name");
  log = SD_MMC.open("/.Log_song.txt", FILE_WRITE);  //erase and repopulate
  song = songsatroot.openNextFile();

  while (song) {  // looking for file

    const char* cname = song.name();
    String name = cname;

    total_used_size = (song.size() / (1024.0 * 1024.0)) + total_used_size;  // calting the total used size

    //=============
    float used_space = total_used_size / 1024.0;
    float total_space = SD_MMC.totalBytes() / 1024.0 / 1024.0 / 1024.0;
    float free_space = total_space - used_space;
    //===============

    // Serial.println(cname);
    // Serial.println(song.size());

    if (name.charAt(name.length() - 4) == '.' && name.charAt(name.length() - 3) == 'w' && name.charAt(name.length() - 2) == 'a' && name.charAt(name.length() - 1) == 'v') {
      counting_song++;

      tft.setCursor(x, y);
      tft.println(counting_song);

      log.printf("/%s\n", cname);
    }
    song = songsatroot.openNextFile();

    //====================

    if (timeout(50) == true) {
      sd_read_ani_sprite.setSwapBytes(1);

      static uint8_t frame = 0;
      const uint16_t* framePointer = sdcard_read_ani1[frame];

      sd_read_ani_sprite.pushImage(0, 0, 30, 20, framePointer);
      sd_read_ani_sprite.pushSprite(4, y + 13);

      frame++;
      if (frame == 7) {
        frame = 0;
      }
    }


    tft.setTextColor(RGBcolorBGR(convert, 0xbdf7), 0);
    tft.setCursor(46 - 6, y + 14);
    tft.print(used_space, 1);  // used space
    tft.println(" GB");

    tft.setCursor(190 + 6, y + 14);
    tft.print(total_space, 1);  // total space
    tft.println(" GB");

    tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
    tft.setCursor(51 + 6 * 5, y + 14 + 12);
    tft.print((100 - (free_space / total_space) * 100), 1);  // free space
    tft.print("% / ");
    tft.print(free_space, 1);
    tft.printf("GB Free  ");


    tft.fillRect(82 + 6, y + 14, 100 - (free_space / total_space) * 100, 7, RGBcolorBGR(convert, 0xd508));
    tft.fillRect(80 + 6, y + 14, 2, 7, RGBcolorBGR(convert, 0xd508));
    tft.fillRect(183 + 6, y + 14, 2, 7, RGBcolorBGR(convert, 0xd508));

    //==================


    // song = songsatroot.openNextFile();
  }

  total_used_size = total_used_size;  // size in MB

  Songamountlog = SD_MMC.open("/.SDinfo.txt", FILE_WRITE);  //open /.SDinfo.txt to log data like used size and amount of song
  *P_SongAmount = counting_song;
  Songamountlog.printf("SA:%s\n", num_to_str(counting_song));
  Songamountlog.printf("SDUS:%s\n", num_to_str(total_used_size));  //write into the FIle


  Songamountlog.close();
  song.close();
  log.close();
}
void Updating_songs_EX(int16_t x, int16_t y) {  //update song file name from sd card  to the txt file


  WavFile.close();
  BMPIMG.close();
  BMPIMG120.close();
  songsatroot.close();
  song.close();
  logread.close();
  check.close();
  SDinfo.close();
  activetime.close();
  autoupdate.close();

  *P_playstate = true;

  uint32_t total_used_size = 0;

  static int8_t move = 0;

  tft.fillRect(46 - 6, 113, 240, 25, 0);

  File log;
  File Songamountlog;

  int16_t counting_song = 0;

  songsatroot = SD_MMC.open("/");

  Serial.println("rewriting song name");
  log = SD_MMC.open("/.Log_song.txt", FILE_WRITE);  //erase and repopulate
  song = songsatroot.openNextFile();

  while (song) {  // looking for file

    const char* cname = song.name();
    String name = cname;

    total_used_size = (song.size() / (1024.0 * 1024.0)) + total_used_size;  // calting the total used size

    float used_space = total_used_size / 1024.0;
    float total_space = SD_MMC.totalBytes() / 1024.0 / 1024.0 / 1024.0;
    float free_space = total_space - used_space;
    Serial.println(name);

    // Serial.println(cname);
    // Serial.println(song.size());

    if (name.charAt(name.length() - 4) == '.' && name.charAt(name.length() - 3) == 'w' && name.charAt(name.length() - 2) == 'a' && name.charAt(name.length() - 1) == 'v') {
      counting_song++;

      tft.setCursor(x, y);
      tft.println(counting_song);


      log.printf("/%s\n", cname);
    }

    if (timeout(50) == true) {
      sd_read_ani_sprite.setSwapBytes(1);

      static uint8_t frame = 0;
      const uint16_t* framePointer = sdcard_read_ani1[frame];

      sd_read_ani_sprite.pushImage(0, 0, 30, 20, framePointer);
      sd_read_ani_sprite.pushSprite(4, 112);

      frame++;
      if (frame == 7) {
        frame = 0;
      }
    }


    tft.setTextColor(RGBcolorBGR(convert, 0xbdf7), 0);
    tft.setCursor(46 - 6, 113);
    tft.print(used_space, 1);  // used space
    tft.println(" GB");

    tft.setCursor(190 + 6, 113);
    tft.print(total_space, 1);  // total space
    tft.println(" GB");

    tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
    tft.setCursor(51 + 6 * 5, 125);
    tft.print((100 - (free_space / total_space) * 100), 1);  // free space
    tft.print("% / ");
    tft.print(free_space, 1);
    tft.printf("GB Free  ");


    tft.fillRect(82 + 6, 113, 100 - (free_space / total_space) * 100, 7, RGBcolorBGR(convert, 0xd508));
    tft.fillRect(80 + 6, 113, 2, 7, RGBcolorBGR(convert, 0xd508));
    tft.fillRect(183 + 6, 113, 2, 7, RGBcolorBGR(convert, 0xd508));
    song = songsatroot.openNextFile();

    if (counting_song % 50 == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  total_used_size = total_used_size;  // size in MB

  Songamountlog = SD_MMC.open("/.SDinfo.txt", FILE_WRITE);  //open /.SDinfo.txt to log data like used size and amount of song
  *P_SongAmount = counting_song;
  Songamountlog.printf("SA:%s\n", num_to_str(counting_song));
  Songamountlog.printf("SDUS:%s\n", num_to_str(total_used_size));  //write into the FIle


  Songamountlog.close();
  song.close();
  log.close();

  *P_update_done = true;
}
void Update_songs_name(Songname_Struct& sa) {  //update song to Songname_Struct

  logread = SD_MMC.open("/.Log_song.txt", FILE_READ);
  SDinfo = SD_MMC.open("/.SDinfo.txt", FILE_READ);


  String name;
  String sdinfo;
  String songamount;
  String SDsize;

  while (true) {
    sdinfo = SDinfo.readStringUntil('\n');
    Serial.println(sdinfo);
    if (sdinfo.substring(0, 3) == "SA:") {
      songamount = sdinfo.substring(3, sdinfo.length());
    }
    if (sdinfo.substring(0, 5) == "SDUS:") {
      SDsize = sdinfo.substring(5, sdinfo.length());
    }
    if (SDinfo.available() == false) {
      break;
    }
  }

  *P_sdusedsize = SDsize.toInt();
  *P_SongAmount = songamount.toInt();

  for (int i = 0; i < *P_SongAmount; i++) {
    name = logread.readStringUntil('\n');
    sa.songsnameS[i] = name;
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.println(name);
  }

  logread.close();
  SDinfo.close();
}
//============

//=============== Equalizer section
float bandFreqs[9] = { 60, 150, 400, 1000, 2400, 6000, 12000, 14000, 16000 };
float bandQ[9] = { 1.2, 1.3, 1.3, 1.3, 1.4, 1.6, 1.6, 1.6, 1.8 };
float Gain[9] = { -0, -0, -10, -5, -5, -5, -15, -8, -20 };
//float Gain[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // sum = 9 vol 1 x=9 y=1
//float Gain[9] = { -20, -20, -20, -20, -20, -20, -20, -20, -20 };  // sum = 0.9 vol 5 x=0.9 y =5
void EQ(byte* Sample, uint16_t length, bool playbacktype) {  // EQ and volume controll

  static int16_t Left;
  static int16_t Right;
  int16_t* Sample16 = (int16_t*)Sample;
  length = length / 2;

  //using y=mx+b;
  static float x = 9;
  static float y = 0.7;
  static float x1 = 0.9;
  static float y1 = 1;
  //float

  static float m = (y - y1) / (x - x1);
  static float b = y1 - m * x1;

  float cal_vol = m * (*P_gainSum) + b;
  float state_vol_scale = cal_vol / 9.0;
  float main_vol = (*P_vol_state) * state_vol_scale;

  for (uint16_t i = 0; i < length; i += 2) {
    // taking L R from sample 16bit each channel array goes like L R L R L R L R L R

    Left = Sample16[i];       // 0 to 1
    Right = Sample16[i + 1];  // 2 to 3

    float L = Left / 32768.0;
    float R = Right / 32768.0;

    for (uint8_t bi = 0; bi < 9; bi++) {  // take 2byte and perfroming 9 bands
      L = biquad_process(BLeft[bi], L);
      R = biquad_process(BRight[bi], R);
    }


    Left = (L * 32767.0) * main_vol;
    Right = (R * 32767.0) * main_vol;


    if (i % (length / waveget - 1) == 0) {
      xQueueSend(waveform_queue, &Left, 0);
    }

    Sample16[i] = Left;       // 0 to 1
    Sample16[i + 1] = Right;  // 2 to 3
  }

  // last_playback_state = playbacktype;  // set it back so this only run when the state change
}
float biquad_process(Biquad& B, float x) {
  float y = B.b0 * x + B.z1;
  B.z1 = B.b1 * x - B.a1 * y + B.z2;
  B.z2 = B.b2 * x - B.a2 * y;
  return y;
}
void EQinit() {
  File EQW;
  String freq[9];
  Serial.println("EQ READING");

  EQFile = SD_MMC.open("/.EQ_save.txt", FILE_READ);  //erase and repopulate


  if (EQFile.available()) {
    Serial.println("load saved EQ");
    for (uint8_t i = 0; i < 9; i++) {
      freq[i] = EQFile.readStringUntil('\n');
      //Serial.println(freq[i]);
    }
    for (uint8_t i = 0; i < 9; i++) {
      P_Gain[i] = new float;
      uint8_t toint = freq[i].toInt();
      float freqf = toint / 10.0 * -1;
      *P_Gain[i] = freqf;
      // Serial.println(*P_Gain[i]);
    }

  } else {
    EQW = SD_MMC.open("/.EQ_save.txt", FILE_WRITE);  //erase and repopulate
    Serial.println("default EQ");
    for (uint8_t i = 0; i < 9; i++) {
      P_Gain[i] = new float;
      *P_Gain[i] = Gain[i];

      int8_t gainint = Gain[i] * -1;

      EQW.printf("%d\n", gainint);
    }
    EQW.close();
  }
  EQFile.close();
}
void EQslider(void* pvParameters) {

  while (1) {
    float gainSum = 0;

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    uint8_t band = *P_processing_band;

    if (*P_processing_band == 20) {
      for (uint8_t i = 0; i < 9; i++) {
        EQ_Setup(BLeft[i], bandFreqs[i], bandQ[i], *P_Gain[i], WavHeader.SampleRate);
        EQ_Setup(BRight[i], bandFreqs[i], bandQ[i], *P_Gain[i], WavHeader.SampleRate);
        gainSum += pow(10.0, *P_Gain[i] / 20.0);
      }
      *P_processing_band = 30;
    } else {
      EQ_Setup(BLeft[band], bandFreqs[band], bandQ[band], *P_Gain[band], WavHeader.SampleRate);
      EQ_Setup(BRight[band], bandFreqs[band], bandQ[band], *P_Gain[band], WavHeader.SampleRate);
      for (uint8_t i = 0; i < 9; i++) {
        gainSum += pow(10.0, *P_Gain[i] / 20.0);
      }
    }

    *P_gainSum = gainSum;

    //Serial.println(gainSum);
    *P_EQdelta_no_run_while_audio_processing = true;
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
void EQ_Setup(Biquad& B, float f0, float Q, float gainDB, float Fs) {
  float A = powf(10.0f, gainDB / 40.0f);
  float w0 = 2.0f * M_PI * f0 / Fs;
  float alpha = sinf(w0) / (2.0f * Q);
  float cw0 = cosf(w0);

  // Raw coefficients
  float b0 = 1.0f + alpha * A;
  float b1 = -2.0f * cw0;
  float b2 = 1.0f - alpha * A;
  float a0 = 1.0f + alpha / A;
  float a1 = -2.0f * cw0;
  float a2 = 1.0f - alpha / A;

  // Normalize
  B.b0 = b0 / a0;
  B.b1 = b1 / a0;
  B.b2 = b2 / a0;
  B.a1 = a1 / a0;
  B.a2 = a2 / a0;

  // Reset filter state (optional)
  B.z1 = 0;
  B.z2 = 0;
}
//===========================

//================
void intro() {

  bool update_song;
  String AUPDATE;
  autoupdate = SD_MMC.open("/.AutoUpdate.txt", FILE_READ);

  if (autoupdate.available() == false) {
    update_song = false;
  } else {
    AUPDATE = autoupdate.readStringUntil('\n');
    update_song = AUPDATE.toInt();
  }
  // Serial.print("AUPDATE ");
  // Serial.println(AUPDATE);




  // Serial.println(update_song);
  *P_Aupdate = update_song;



  uint32_t chipId = 0;
  bool loop = true;
  int8_t x = 3;
  int8_t y = 26;
  int8_t S = 10;
  //Serial.println(RGBcolorBGR(convert, 0xfd00));
  tft.setTextColor(RGBcolorBGR(convert, 0xfd00));
  tft.pushImage(3, 3, 96, 16, metislogo);
  tft.drawBitmap(111, 3, mp, 36, 16, RGBcolorBGR(convert, 0xfd00));
  tft.setCursor(151, 12);
  tft.println(Version);
  tft.drawFastHLine(0, 22, 240, RGBcolorBGR(convert, 0xfd00));
  tft.drawFastHLine(0, 239, 240, RGBcolorBGR(convert, 0xfd00));

  tft.setCursor(x, y);
  tft.printf("%s Rev %d:\n", ESP.getChipModel(), ESP.getChipRevision());

  tft.setCursor(x, y + 7 + 3);
  if (ESP.getChipCores() == 2) {
    tft.println("Dual cores");
  } else {
    tft.println("Mono core");
  }
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  tft.setCursor(x, y + S * 2);
  tft.print("ID: ");

  tft.setCursor(x, y + S * 3);
  tft.printf("FLASH:");

  tft.setCursor(x, y + S * 4);
  tft.printf("APP:");

  tft.setCursor(x, y + S * 5);
  tft.printf("CPU CLOCK:");

  tft.setCursor(x, y + S * 6);
  tft.printf("RAM:");

  tft.setCursor(x, y + S * 7);
  tft.print("PSRAM: ");

  tft.setCursor(x, y + S * 9);
  tft.print("SD CARD: ");

  tft.setCursor(x, y + S * 10);
  tft.print("TYPE: ");

  tft.setCursor(x, y + S * 11);
  tft.print("SIZE: ");


  tft.setCursor(x, y + S * 13);
  tft.print("Automaticly Updating Songs: ");
  tft.setTextColor(RGBcolorBGR(convert, 0xffff));
  if (update_song == true) {
    tft.print("TRUE");
  } else {
    tft.print("FALSE");
  }
  tft.setTextColor(RGBcolorBGR(convert, 0xfd00));
  tft.setCursor(x, y + S * 14);
  tft.println("Updating:");

  tft.setCursor(x, y + S * 15);
  tft.println("Total Songs:");


  //======================
  tft.setTextColor(RGBcolorBGR(convert, 0xffff));
  tft.setCursor(x + 77, y + S * 2);
  tft.println(chipId);

  tft.setCursor(x + 77, y + S * 3);
  tft.print(ESP.getFlashChipSize() / 1024.0 / 1024.0, 0);
  tft.println(" MB");

  tft.setCursor(x + 77, y + S * 4);
  tft.print((ESP.getSketchSize() + ESP.getFreeSketchSpace()) / 1024.0 / 1024.0, 0);
  tft.println(" MB");


  tft.setCursor(x + 77, y + S * 5);
  tft.printf("%d MHz\n", ESP.getCpuFreqMHz());


  tft.setCursor(x + 77, y + S * 6);
  tft.print(heap_caps_get_total_size(MALLOC_CAP_EXEC) / 1024.0, 0);
  tft.println(" KB");

  tft.setCursor(x + 77, y + S * 7);
  tft.print(ESP.getPsramSize() / 1024.0 / 1024.0, 0);
  tft.println(" MB");

  tft.setCursor(x + 77, y + S * 9);

  sd_insert_ani_sprite.createSprite(66, 24);
  sd_insert_ani_sprite.setSwapBytes(1);

  if (!SD_MMC.begin()) {
    tft.setTextColor(RGBcolorBGR(convert, 0xf800));
    tft.print("NOT FOUND!");
    tft.setCursor(x + 77, y + S * 10);
    tft.print("INSERT YOUR SD CARD");
    tft.setCursor(x + 77, y + 10 * 11);
    tft.print("AND RESTART");




    int8_t fdelay[27] = { 10, 7, 15, 10, 5, 10, 10, 20, 10, 20, 10, 5, 50, 5, 5, 15, 5, 5, 10, 10, 2, 10, 10, 10, 10 };

    while (1) {
      for (int8_t frame = 0; frame < 25; frame++) {

        const uint16_t* framePointer = sdcard_insert_ani1[frame];

        //sd_insert_ani_sprite.pushImage(0, 0, 66, 24, framePointer);  //remove if there isnt enough ram space
        //sd_insert_ani_sprite.pushSprite(240 - 66 + 25, y + 90);

        Serial.print("RAM lelf: ");
        Serial.print((heap_caps_get_free_size(MALLOC_CAP_EXEC)) / 1024.0);
        Serial.println("kb");

        //Serial.println("ghf");
        delay((fdelay[frame] * 10));
      }
    }

  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0x2724));
    tft.println("OK");
  }
  tft.setTextColor(RGBcolorBGR(convert, 0xffff));

  const uint16_t* framePointer = sdcard_insert_ani1[14];

  // sd_insert_ani_sprite.pushImage(0, 0, 66, 24, framePointer);  //remove if there isnt enough ram space
  // sd_insert_ani_sprite.pushSprite(240 - 66 + 25, y + 90);

  sd_insert_ani_sprite.deleteSprite();

  //tft.printf("Card Size: %dMB\n", SD_MMC.totalBytes() / 1024.0 / 1024.0 / 1024.0, 0);
  tft.setCursor(x + 77, y + S * 10);
  tft.setTextColor(RGBcolorBGR(convert, 0xfd00));
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_MMC) {
    tft.println("MMC");
  } else if (cardType == CARD_SD) {
    tft.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    tft.println("SDHC");
  } else {
    tft.println("UNKNOWN");
  }
  tft.setTextColor(RGBcolorBGR(convert, 0xffff));

  tft.setCursor(x + 77, y + S * 11);
  for (uint8_t size = 0; size < 11; size++) {
    if (SD_MMC.totalBytes() / 1024.0 / 1024.0 / 1024.0 <= pow(2, size)) {
      tft.print(pow(2, size), 0);
      tft.print(" GB");
      break;
    }
  }

  tft.setCursor(x + 77, y + S * 14);
  tft.setTextColor(RGBcolorBGR(convert, 0xffff), 0);
  if (*P_Aupdate == 1) {

    tft.print("YES");
    Updating_songs(x + 77, y + S * 15);

    tft.setCursor(x + 77, y + S * 14);
    tft.print("DONE!");

  } else {

    tft.print("NO");
  }
  tft.print(" / NOW READING");
  Update_songs_name(Songname);

  tft.setCursor(x + 77, y + S * 15);

  tft.print(*P_SongAmount);

  delay(1000);
}
void main_menu(void* pvParameters) {
  static int8_t index = 0;
  static bool song_once = false;
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);


    if (*P_menu_black == true) {
      tft.fillScreen(0);
      *P_menu_black = false;
    }

    if (*P_all_menu_reset == true) {
      song_once = false;
      *P_all_menu_reset = false;
    }

    *P_menu_done_draw = false;

    if (digitalRead(UP_pin) == HIGH) {
      index--;
      delay(100);
    }
    if (digitalRead(DOWN_pin) == HIGH) {
      index++;
      delay(100);
    }


    if (index < 0) {
      index = 3;
    }
    if (index > 3) {
      index = 0;
    }

    switch (index) {
      case 0:
        song_once = false;
        tft.fillRect(116, 0, 124, 114, 0);
        //once = true;
        tft.fillRect(118, 118, 120, 120, 0);
        break;
      case 1:

        if (skipbutton() == HIGH) {
          //delay(100);
          if (*P_playstate == true) {
            while (1) {
              Songs(0);

              vTaskDelay(pdMS_TO_TICKS(10));

              if (backbutton() == HIGH || ScreenMode(10) == 1) {
                song_once = false;
                break;
              }
            }
          } else {
            tft.fillScreen(0);
            while (1) {
              Songs_inner(1);
              if (skipbutton() == HIGH) {
                while (1) {
                  Songs_inner(0);
                  vTaskDelay(pdMS_TO_TICKS(10));
                  if (backbutton() == HIGH || ScreenMode(10) == 1) {
                    song_once = false;
                    break;
                  }
                }
              }
              vTaskDelay(pdMS_TO_TICKS(1));
              if (backbutton() == HIGH) {
                small_songs_menu.fillSprite(0);
                tft.fillScreen(0);
                song_once = false;
                break;
              }
              if (ScreenMode(10) == 1) {
                song_once = false;
                break;
              }
            }
          }
        }

        if (song_once == false) {     // timeout so the img can be display properly
          if (ScreenMode(10) != 1) {  //skiping this when switching

            // Serial.println("song 1");
            if (Songs(1) == true) {
              //Serial.println("song 1 dis");
              song_once = true;
            }
          }
        }
        break;
      case 2:
        song_once = false;
        tft.fillRect(116, 0, 124, 114, 0);
        break;
      case 3:
        tft.fillRect(116, 0, 124, 114, 0);

        if (skipbutton() == HIGH) {
          delay(100);
          tft.fillScreen(0);

          while (1) {
            setting();
            vTaskDelay(pdMS_TO_TICKS(1));
            if (backbutton() == HIGH) {
              delay(100);
              tft.fillScreen(0);
              break;
            }
          }
        }


        break;
    }
    // to skip the rest
    if (ScreenMode(10) != 1) {

      if (index == 0) {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));
      }

      tft.setTextSize(2);
      tft.setTextWrap(false);
      tft.setCursor(22, 37);
      tft.print("Albums");


      if (index == 1) {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));
      }
      tft.setCursor(28, 90);
      tft.print("Songs");


      if (index == 2) {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));
      }
      tft.setCursor(4, 144);
      tft.print("Playlists");


      if (index == 3) {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));
      }
      tft.setCursor(10, 198);
      tft.print("Settings");


      tft.drawLine(115, 0, 115, 240, RGBcolorBGR(convert, 0xDD08));

      tft.drawLine(114, 13, 0, 13, RGBcolorBGR(convert, 0xFD00));

      tft.drawFastHLine(115, 115, 240 - 115, RGBcolorBGR(convert, 0xFD00));
      // noartcover120();

      tft.setTextColor(RGBcolorBGR(convert, 0xFFFF));  // batt percentage
      tft.setTextSize(1);
      tft.setCursor(8, 3);
      tft.print("100%");

      tft.setCursor(45, 3);
      tft.println("Est:4h 32m");

      underline(1, 1);
    }

    *P_menu_done_draw = false;

    *P_menu_done_draw = true;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
//=====================

//==============
//++ or -- => start to end
uint16_t colortrans(int16_t increment, uint16_t length, uint16_t colorS, uint16_t colorE) {

  uint8_t sred = (colorS & 0xF800) >> 11;
  uint8_t sgreen = (colorS & 0x07E0) >> 5;
  uint8_t sblue = (colorS & 0x001F);

  uint8_t ered = (colorE & 0xF800) >> 11;
  uint8_t egreen = (colorE & 0x07E0) >> 5;
  uint8_t eblue = (colorE & 0x001F);

  uint16_t output;

  float deltaR = (sred - ered) / float(length);
  float deltaG = (sgreen - egreen) / float(length);
  float deltaB = (sblue - eblue) / float(length);

  uint8_t R;
  uint8_t G;
  uint8_t B;

  //31-(31-23 /20)*incre;23
  //23-(23-31 /20)*incre;31

  R = sred - float(deltaR * abs(increment));
  G = sgreen - float(deltaG * abs(increment));
  B = sblue - float(deltaB * abs(increment));

  output = (R) << 11 | (G) << 5 | B << 0;
  return output;
}
//increment by itself
//may delete later
uint16_t selfcolortrans(uint16_t increment_by, uint16_t colorS, uint16_t colorE) {

  static int16_t increment = 0;
  int16_t length = 1000;

  uint8_t sred = (colorS & 0xF800) >> 11;
  uint8_t sgreen = (colorS & 0x07E0) >> 5;
  uint8_t sblue = (colorS & 0x001F);

  uint8_t ered = (colorE & 0xF800) >> 11;
  uint8_t egreen = (colorE & 0x07E0) >> 5;
  uint8_t eblue = (colorE & 0x001F);

  uint16_t output;

  float deltaR = (sred - ered) / float(length);
  float deltaG = (sgreen - egreen) / float(length);
  float deltaB = (sblue - eblue) / float(length);

  uint8_t R;
  uint8_t G;
  uint8_t B;

  //31-(31-23 /20)*incre;23
  //23-(23-31 /20)*incre;31

  R = sred - float(deltaR * abs(increment));
  G = sgreen - float(deltaG * abs(increment));
  B = sblue - float(deltaB * abs(increment));

  output = (R) << 11 | (G) << 5 | B << 0;
  return output;
}
//===============




//====================================
//argu 1 = display, 0 = controll
bool Songs(bool display) {

  static int16_t index_song = -30;  // -30 to get it to sync with the perset index //at 131
  static int16_t index_img;         // to enable the img to be process at the same time as the text is scrolling
  static int text_y = -20;

  static int pxstrlen2 = 0;
  static float scrolls[7];
  static bool scoll_end[7] = { 0, 0, 0, 0, 0, 0, 0 };

  static bool S_up = false;  // heh, sup
  static bool S_down = false;
  static int8_t img_type = 0;

  static int16_t w = 0;
  static int16_t trans = 0;
  static bool pushed = false;

  static bool view_push_once = false;

  String name;
  String S_SongAmount;
  String S_index;

  //========= display song index and total songs

  if (index_song == -30) {
    *P_songs_menu_index = index_song = *P_selected_song_index + 1;  //131
    index_img = *P_selected_song_index;
  }

  S_SongAmount = num_to_str(*P_SongAmount);
  S_index = num_to_str(index_song);

  tft.setTextColor(RGBcolorBGR(convert, 0x39c7), 0);
  tft.setCursor(234 - (S_SongAmount.length() * 5 + S_SongAmount.length()), 1);
  tft.printf("/%d", *P_SongAmount);

  tft.setTextColor(RGBcolorBGR(convert, 0x6b4d), 0);
  tft.fillRect(229 - (S_SongAmount.length() * 5 + S_SongAmount.length()) - (S_index.length() * 5 + S_index.length()), 1, 6, 7, 0);

  tft.setCursor(234 - (S_SongAmount.length() * 5 + S_SongAmount.length()) - (S_index.length() * 5 + S_index.length()), 1);
  tft.print(index_song);

  index_song = *P_songs_menu_index;  // here to sync it with the inner songs list and reverse
  index_img = index_song - 1;        // here to sync it with the inner songs list and reverse

  //==============
  if (display == false) {
    //view_push_once = false;;

    if (skipbutton() == true) {  //a song is selected
      *P_menu_selected_song = true;
      *P_change_song_notice = true;
      *P_song_changed = true;
      *P_selected_song_index = index_song;  // the song selected always in the 4th place due to how index read
      *P_playstate = false;
    }


    if (index_song != *P_SongAmount) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        if (timeout(100) == true) {
          index_img++;
          S_down = true;
          *P_songs_menu_song_change = true;
        }
      }
    }

    if (index_song != 1) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        if (timeout(100) == true) {
          *P_songs_menu_song_change = true;
          index_img--;
          S_up = true;
        }
      }
    }

    if (S_up == true) {  // puting this here so the img dispaly can look more respondsive
      img_type = openingbmp120(Songname.songsnameS[index_img]);
    }

    if (S_down == true) {
      img_type = openingbmp120(Songname.songsnameS[index_img]);
    }


    for (float q = 0; q < 20; q += 0.5) {
      if (S_up == true) {
        w = q * 1;
        trans = q * 1;
      }
      if (S_down == true) {
        w = q * -1;
        trans = q * -1;
      }

      for (int i = 0; i < 7; i++) {  /// this one for when controlling this menu


        if (i + index_song - 4 < 0) {  // prevent it from reading out of bound
          name = " ";
        } else if (i + index_song - 4 > *P_SongAmount - 1) {
          name = " ";
        } else {
          name = manip_song_name(Songname.songsnameS[i + index_song - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
        }

        pxstrlen2 = 5 * name.length() + name.length() - 1;

        switch (i) {
          case 0:
            text_y = -20 + w;
            //Serial.println(w);
            small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;
          case 1:
            text_y = 0 + w;

            if (S_up == false && S_down == false) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            } else if (S_down == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black
            }
            break;
          case 2:
            text_y = 20 * 1 + w;

            if (S_up == false && S_down == false) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            } else if (S_down == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray
            }
            break;
          case 3:
            text_y = 20 * 2 + w;

            if (S_up == false && S_down == false) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, 0xFD85), 0);
            } else {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xFD85, 0xbdf7)), 0);
            }
            break;
          case 4:
            text_y = 20 * 3 + w;

            if (S_up == false && S_down == false) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray

            } else if (S_down == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            }
            break;
          case 5:
            text_y = 20 * 4 + w;
            if (S_up == false && S_down == false) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black

            } else if (S_down == true) {
              small_songs_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            }

            break;
          case 6:
            text_y = 20 * 5 + w;

            small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;

          default:
            break;
        }

        small_songs_menu.drawFastHLine(0, text_y - 1, 101, 0);
        small_songs_menu.drawFastHLine(0, text_y + 8, 101, 0);
        small_songs_menu.drawFastHLine(0, text_y - 2, 101, 0);
        small_songs_menu.drawFastHLine(0, text_y + 9, 101, 0);


        if (name.length() > 17) {

          if (scrolls[i] <= 101 - pxstrlen2 - 10) {
            scoll_end[i] = true;
          }
          if ((scrolls[i] >= 0 + 10)) {
            scoll_end[i] = false;
          }

          if (i == 3) {  // restricted to the selected
            if (scoll_end[i] == false) {
              scrolls[i] = (scrolls[i] - .1 - (1 - 17.0 / name.length()) / 2.5);  // scale by string lenght
            } else {
              scrolls[i] = (scrolls[i] + .5 + (1 - 17.0 / name.length()) / 2.0);

              // scrolls[i] = (scrolls[i] + speed + (1 - max length / name.length()) / scale); // higer scale is slower
            }
          }

          small_songs_menu.drawFastVLine(scrolls[i] - 1, text_y, 7, 1);
          small_songs_menu.setCursor(scrolls[i], text_y);



        } else {

          scrolls[i] = 0;
          small_songs_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
        }

        small_songs_menu.print(name);
      }

      small_songs_menu.pushSprite(126, 14);

      if (S_up == false && S_down == false) {
        break;
      }

      //delay(100);
    }


    if (S_up == true) {  // puting this here so the img dispaly can looke more respondsive
      index_song--;
    }
    if (S_down == true) {
      index_song++;
    }


    if (*P_art120done == true) {
      if (img_type == 2) {
        tft.setRotation(2);
        art120_sprite.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
        art120_sprite.writedata(TFT_MAD_MY);
        art120_sprite.pushSprite(118, 2);
        tft.setRotation(0);
      }
      *P_art120done = false;
      //pushed = true;
    }

    if (S_down == true || S_up == true) {
      if (img_type == 1) {
        art120_sprite.fillSprite(0);
        art120_sprite.pushSprite(118, 118);
        noartcover120();
      }

      for (int8_t a = 0; a < 7; a++) {
        scrolls[a] = 0;
        scoll_end[a] = 1;
      }
    }

    //reset

    S_up = false;
    S_down = false;
    text_y = -20;
    w = 0;


    *P_songs_menu_song_change = false;
    *P_songs_menu_index = index_song;
  } else {
    // if (view_push_once == false) {
    for (int i = 0; i < 7; i++) {  /// this one for when controlling this menu

      if (i + index_song - 4 < 0) {  // prevent it from reading out of bound
        name = " ";
      } else if (i + index_song - 4 > *P_SongAmount - 1) {
        name = " ";
      } else {
        name = manip_song_name(Songname.songsnameS[i + index_song - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index
      }

      pxstrlen2 = 5 * name.length() + name.length() - 1;

      switch (i) {
        case 0:
          text_y = -20 + w;
          small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 1:
          text_y = 0 + w;
          small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 2:
          text_y = 20 * 1 + w;
          small_songs_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 3:
          text_y = 20 * 2 + w;
          small_songs_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 4:
          text_y = 20 * 3 + w;
          small_songs_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 5:
          text_y = 20 * 4 + w;
          small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 6:
          text_y = 20 * 5 + w;
          small_songs_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        default:
          break;
      }

      if (name.length() > 17) {
        small_songs_menu.setCursor(0, text_y);
      } else {
        small_songs_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
      }

      small_songs_menu.print(name);
    }
    view_push_once = true;
  }



  small_songs_menu.pushSprite(126, 14);

  img_type = openingbmp120(Songname.songsnameS[index_img]);

  if (img_type == 1) {
    art120_sprite.fillSprite(0);
    art120_sprite.pushSprite(118, 118);
    noartcover120();
  }
  pushed = false;

  if (*P_art120done == true) {
    if (img_type == 2) {
      tft.setRotation(2);
      art120_sprite.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
      art120_sprite.writedata(TFT_MAD_MY);
      art120_sprite.pushSprite(118, 2);
      tft.setRotation(0);

      Serial.println("push");
    }
    *P_art120done = false;
    pushed = true;
  }
  //}

  // if (display == 1) {
  return pushed;
  //  } else {
  //   return 0;
  // }
}
void Songs_inner(bool display) {  // display full song without cover art



  // likely to crash to spinlock_acquire spinlock.h:122 (result == core_id || result == SPINLOCK_FREE) if you stay for too long

  // this is the 22 songs display version, splt screen

  //make 2 window

  // the up and down to move between page

  // skip to move into selection

  // the up and down for selecting

  // skip to selected

  // back to go back into page mode

  static uint16_t Songs = *P_SongAmount;
  static int text_y = 0;
  static uint16_t pxstrlen_song_page1 = 0;
  static uint16_t pxstrlen_song_page2 = 0;

  //static bool reset_

  static uint16_t page_index = 0;
  static uint16_t song_index = 0;
  //static bool leftover = false;

  static int16_t staring_song_index = 0 * 20;  // -30 to get it to sync with the perset index //at 131

  static float scrolls[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  static bool scoll_end[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  static uint16_t sprite_width = 107;

  //String name;
  String S_index;

  String song_name_page1;
  String song_name_page2;

  uint8_t spacing = 7 + 15;

  int16_t selected_song_color = 0xFD85;
  int16_t page_text1_color = 0xc658;
  int16_t page_text2_color = 0x8c71;


  //0xd508 light gold

  //0xFD85 bright gold

  //0x069f light blue

  S_index = num_to_str(staring_song_index);

  tft.setTextColor(RGBcolorBGR(convert, 0xbdf7));
  tft.setCursor(1, 14);
  tft.print("///////////////////");

  tft.drawFastHLine(0, 13, 120, RGBcolorBGR(convert, 0xd508));
  tft.drawFastHLine(0, 20, 240, RGBcolorBGR(convert, 0xd508));
  tft.drawFastHLine(0, 21, 240, RGBcolorBGR(convert, 0xd508));
  tft.drawFastVLine(120, 0, 20, RGBcolorBGR(convert, 0xd508));
  tft.drawFastVLine(120, 0, 19, RGBcolorBGR(convert, 0xd508));

  tft.setCursor(123, 2);
  tft.setTextSize(2);
  tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
  tft.println("Songs");
  tft.fillRect(119, 20, 2, 220, RGBcolorBGR(convert, 0xd508));

  tft.setTextColor(RGBcolorBGR(convert, 0xffff), 0);
  tft.setTextSize(1);
  tft.setCursor(208 - 6 - (S_index.length() * 5 + S_index.length()), 9);
  tft.printf(" %d", staring_song_index);
  tft.setCursor(208, 9);
  if (staring_song_index + 20 >= Songs) {
    tft.printf(" - %d ", Songs);
  } else {
    tft.printf(" - %d ", staring_song_index + 20);
  }

  // if (Songs % 20 != 0) {
  //   leftover = true;
  // } else {
  //   leftover = false;
  // }

  if (display == true) {
    tft.setCursor(0, 0);
    tft.println("page");

    if (page_index != Songs / 20) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        page_index++;
        delay(100);
        inner_songs_page1.fillSprite(0);
        inner_songs_page2.fillSprite(0);
      }
    }

    if (page_index != 0) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        page_index--;
        delay(100);
        inner_songs_page1.fillSprite(0);
        inner_songs_page2.fillSprite(0);
      }
    }

    staring_song_index = page_index * 20;
    song_index = staring_song_index;


    for (int i = 0; i < 10; i++) {  /// this one for when controlling this menu

      if (i % 2 == 0) {
        inner_songs_page1.setTextColor(page_text1_color, 0);  // zebra pattern to make it easy to read
        inner_songs_page2.setTextColor(page_text2_color, 0);
      } else {
        inner_songs_page1.setTextColor(page_text2_color, 0);
        inner_songs_page2.setTextColor(page_text1_color, 0);
      }

      if (i + staring_song_index >= Songs) {
        song_name_page1 = "";

      } else {
        song_name_page1 = Songname.songsnameS[i + staring_song_index];  //at index, ex: at 131 - 4 + i  so it start at above the index
        song_name_page1 = manip_song_name(song_name_page1);
      }

      if (i + staring_song_index + 10 >= Songs) {
        song_name_page2 = "";
      } else {
        song_name_page2 = Songname.songsnameS[i + staring_song_index + 10];  //at index, ex: at 131 - 4 + i  so it start at above the index
        song_name_page2 = manip_song_name(song_name_page2);
      }
      pxstrlen_song_page1 = 5 * song_name_page1.length() + song_name_page1.length() - 1;
      pxstrlen_song_page2 = 5 * song_name_page2.length() + song_name_page2.length() - 1;

      text_y = spacing * i;

      scrolls[i] = 0;
      inner_songs_page1.setCursor(54 - (pxstrlen_song_page1) / 2, text_y);
      inner_songs_page2.setCursor(54 - (pxstrlen_song_page2) / 2, text_y);

      if (song_name_page1.length() > 18) {

        inner_songs_page1.drawFastVLine(scrolls[i] - 1, text_y, 7, 0);
        inner_songs_page1.setCursor(0, text_y);
      }


      if (song_name_page2.length() > 18) {

        inner_songs_page2.drawFastVLine(scrolls[i] - 1, text_y, 7, 0);
        inner_songs_page2.setCursor(0, text_y);
      }


      inner_songs_page1.println(song_name_page1);
      inner_songs_page2.println(song_name_page2);
    }

  } else {
    tft.setCursor(0, 0);
    tft.println("selection");

    if (skipbutton() == true) {  //a song is selected
      *P_songs_menu_index = song_index + 1;
      *P_menu_selected_song = true;
      *P_change_song_notice = true;
      *P_song_changed = true;
      *P_selected_song_index = song_index + 1;  // the song selected always in the 4th place due to how index read
      *P_playstate = false;
      *P_songs_menu_song_change = true;
    }

    if (song_index != Songs - 1) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        song_index++;
        delay(100);
        inner_songs_page1.fillSprite(0);
        inner_songs_page2.fillSprite(0);
      }
    }

    if (song_index != 0) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        song_index--;
        delay(100);
        inner_songs_page1.fillSprite(0);
        inner_songs_page2.fillSprite(0);
      }
    }

    if (song_index > staring_song_index + 19) {  // change page when going over or under staring_song_index
      page_index++;
      inner_songs_page1.fillSprite(0);
      inner_songs_page2.fillSprite(0);
    } else if (song_index < staring_song_index) {
      page_index--;
      inner_songs_page1.fillSprite(0);
      inner_songs_page2.fillSprite(0);
    }

    staring_song_index = page_index * 20;



    for (int i = 0; i < 10; i++) {  /// this one for when controlling this menu
                                    // display 2 page at the same time

      if (i % 2 == 0) {  // zebra pattern to make it easy to read
        inner_songs_page1.setTextColor(page_text1_color, 0);
        inner_songs_page2.setTextColor(page_text2_color, 0);
      } else {
        inner_songs_page1.setTextColor(page_text2_color, 0);
        inner_songs_page2.setTextColor(page_text1_color, 0);
      }

      if (i + staring_song_index >= Songs) {  // set outside of bound string to blank
        song_name_page1 = "";
      } else {
        song_name_page1 = Songname.songsnameS[i + staring_song_index];
        song_name_page1 = manip_song_name(song_name_page1);
      }

      if (i + staring_song_index + 10 >= Songs) {  // set outside of bound string to blank
        song_name_page2 = "";
      } else {
        song_name_page2 = Songname.songsnameS[i + staring_song_index + 10];
        song_name_page2 = manip_song_name(song_name_page2);
      }


      if (i + staring_song_index == song_index) {  // highlight the selected text
        inner_songs_page1.setTextColor(RGBcolorBGR(convert, selected_song_color), 0);
      } else if (i + staring_song_index + 10 == song_index) {
        inner_songs_page2.setTextColor(RGBcolorBGR(convert, selected_song_color), 0);
      }


      pxstrlen_song_page1 = 5 * song_name_page1.length() + song_name_page1.length() - 1;
      pxstrlen_song_page2 = 5 * song_name_page2.length() + song_name_page2.length() - 1;

      text_y = spacing * i;

      inner_songs_page1.setCursor(54 - (pxstrlen_song_page1) / 2, text_y);
      inner_songs_page2.setCursor(54 - (pxstrlen_song_page2) / 2, text_y);

      if (song_name_page1.length() > 18) {

        if (scrolls[i] <= sprite_width - pxstrlen_song_page1 - 10) {  // only scroll as far as the length be behinde
          scoll_end[i] = true;
        }
        if ((scrolls[i] >= 0 + 10)) {
          scoll_end[i] = false;
        }

        if (i + staring_song_index == song_index) {  // restricted to the selected
          if (scoll_end[i] == false) {
            scrolls[i] = (scrolls[i] - .24 - (1 - 18.0 / song_name_page1.length()) / 2.0);  // scale by string lenght
          } else {
            scrolls[i] = (scrolls[i] + .24 + (1 - 18.0 / song_name_page1.length()) / 2.0);
          }
          // scrolls[i] = (scrolls[i] + speed + (1 - max length / name.length()) / scale); // higer scale is slower
        }

        inner_songs_page1.drawFastVLine(scrolls[i] - 1, text_y, 7, 0);
        inner_songs_page1.setCursor(scrolls[i], text_y);
      } else {
        scrolls[i] = 0;
      }


      if (song_name_page2.length() > 18) {
        if (scrolls[i + 10] <= sprite_width - pxstrlen_song_page2 - 10) {  // only scroll as far as the length be behinde - over shoot
          scoll_end[i + 10] = true;
        }
        if ((scrolls[i + 10] >= 0 + 10)) {
          scoll_end[i + 10] = false;
        }

        if (i + staring_song_index + 10 == song_index) {  // restricted to the selected
          if (scoll_end[i + 10] == false) {
            scrolls[i + 10] = (scrolls[i + 10] - .24 - (1 - 18.0 / song_name_page2.length()) / 2.0);  // scale by string lenght
          } else {
            scrolls[i + 10] = (scrolls[i + 10] + .24 + (1 - 18.0 / song_name_page2.length()) / 2.0);
          }
        }

        inner_songs_page2.drawFastVLine(scrolls[i + 10] - 1, text_y, 7, 0);
        inner_songs_page2.setCursor(scrolls[i + 10], text_y);
      } else {
        scrolls[i + 10] = 0;
      }

      inner_songs_page1.println(song_name_page1);
      inner_songs_page2.println(song_name_page2);
    }
  }

  //if (display == 0) {

  //  Serial.println(song_index - staring_song_index);

  //  if (song_index - staring_song_index >= 10) {
  //    Serial.println("page 2");
  //    inner_songs_page2.pushSprite(127, 28);
  //  } else {
  //    Serial.println("page 1");
  //    inner_songs_page1.pushSprite(6, 28);
  //  }

  //} else {
  inner_songs_page1.pushSprite(6, 28);
  inner_songs_page2.pushSprite(127, 28);
  // }
}
void Album(bool display) {
}
void PlayList(bool display) {
}
//====================================

//==========
void setting() {

  static int8_t index = 0;
  static bool audio_once = false;

  if (digitalRead(UP_pin) == HIGH) {
    index--;
    delay(100);
    tft.fillScreen(0);
  }
  if (digitalRead(DOWN_pin) == HIGH) {
    index++;
    delay(100);
    tft.fillScreen(0);
  }

  tft.setTextColor(RGBcolorBGR(convert, 0xbdf7));
  tft.setCursor(1, 14);
  tft.print("///////////////////");

  tft.setTextSize(2);
  tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
  tft.setCursor(130, 2);
  tft.println("Settings");


  tft.setTextSize(1);

  tft.drawFastVLine(115, 0, 20, RGBcolorBGR(convert, 0xFD85));
  tft.drawFastHLine(0, 14, 114, RGBcolorBGR(convert, 0xFD85));
  tft.drawFastHLine(0, 20, 240, RGBcolorBGR(convert, 0xFD85));

  tft.drawFastHLine(0, 38, 240, RGBcolorBGR(convert, 0xBDF7));
  tft.drawFastHLine(0, 43, 240, RGBcolorBGR(convert, 0x73AE));



  //==================changing color when index hit
  if (index != 0) {
    tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
  }
  tft.setCursor(4, 26);
  tft.println("Interface");


  if (index != 1) {
    tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
  }
  tft.setCursor(67, 26);
  tft.println("Audio");


  if (index != 2) {
    tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
  }
  tft.setCursor(106, 26);
  tft.println("Stats");


  if (index != 3) {
    tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
  }
  tft.setCursor(145, 26);
  tft.println("BluT");


  if (index != 4) {
    tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
  }
  tft.setCursor(200, 26);
  tft.println("Abouts");
  //=================

  switch (index) {
    case 0:
      //tft.fillScreen(0);
      Interface(1);
      audio_once = false;
      break;
    case 1:

      if (digitalRead(Skip_pin) == HIGH) {
        delay(100);
        while (1) {
          Audio(0);
          if (backbutton() == HIGH) {
            delay(100);
            break;
          }
        }
      } else {
        if (audio_once == false) {
          Audio(1);
          audio_once = true;
        }
      }
      break;

    case 2:
      audio_once = false;
      if (digitalRead(Skip_pin) == HIGH) {
        delay(100);
        while (1) {
          Stats(0);
          if (digitalRead(Back_pin) == HIGH) {
            delay(100);
            break;
          }
        }
      }
      Stats(1);


      break;

    case 3:

      break;

    case 4:

      break;
  }
}
void Interface(bool display) {

  //get save data from txt file

  tft.setTextSize(1);

  tft.setCursor(5, 49);
  tft.setTextColor(RGBcolorBGR(convert, 0xef7d));
  tft.println("Disk:");

  tft.setCursor(5, 102);
  tft.println("Touch Pads:");


  tft.setTextColor(RGBcolorBGR(convert, 0x73ae));
  tft.setCursor(5, 61);
  tft.println("Increment:");

  tft.setCursor(5, 73);
  tft.println("Rotation Per Sec:");

  tft.setCursor(5, 85);
  tft.println("Direction of Rotaion:");

  tft.setCursor(5, 114);
  tft.println("Fast Scorll lv1:");

  tft.setCursor(5, 126);
  tft.println("Fast Scorll lv2:");

  tft.setCursor(5, 138);
  tft.println("Fast Scorll lv3:");


  tft.setTextColor(RGBcolorBGR(convert, 0x73ae));

  tft.setCursor(141, 138);
  tft.println("<      >");

  //tft.setCursor(141, 138);


  tft.setCursor(141, 61);
  tft.println("<      >");

  tft.setCursor(141, 73);
  tft.println("<      >");

  tft.setCursor(141, 85);
  tft.println("<      >");

  tft.setCursor(141, 114);
  tft.println("<      >");

  tft.setCursor(141, 126);
  tft.println("<      >");

  tft.setCursor(141, 138);
  tft.println("<      >");



  tft.setCursor(141, 61);
  tft.println("15");

  tft.setCursor(141, 73);
  tft.println("15");

  tft.setCursor(141, 85);
  tft.println("15");

  tft.setCursor(141, 114);
  tft.println("15");

  tft.setCursor(141, 126);
  tft.println("15");

  tft.setCursor(141, 138);
  tft.println("15");

  vTaskDelay(pdMS_TO_TICKS(1));
}
void Audio(bool display) {

  static int freq_index = 0;
  static bool once = false;
  float pixel_scale = 82.0 / 20.0;  // 80 pixels high / 20 level (going from 0 to -20)
  static uint16_t freq_pos[9];
  String freq_band_name[9];
  static bool eq_display_once = false;
  static bool update = false;

  // set up a notification
  // if (once == false) {
  String freq;
  int x_pos_sum = 40 - 22;
  bool last_number_length = 0;  // 1 = to 3 or more // 0 = to 2

  static bool draw_once = false;


  int8_t index = 0;

  if (digitalRead(UP_pin) == HIGH) {
    index--;
    //draw_once = false;
    delay(200);
  }

  if (digitalRead(DOWN_pin) == HIGH) {
    index++;
    // draw_once = false;
    delay(200);
  }



  tft.setCursor(5, 49);
  tft.setTextColor(RGBcolorBGR(convert, 0xef7d));
  tft.println("L/R Balance:");

  tft.setCursor(5, 96);
  tft.println("Equalizer:");


  //0xd508 for selected
  tft.setTextColor(RGBcolorBGR(convert, 0x73ae));
  tft.setCursor(5, 61);
  tft.println("Left");
  tft.drawFastHLine(93, 64, 100, RGBcolorBGR(convert, 0xbdf7));
  tft.setCursor(86, 61);
  tft.println("<                     >");


  tft.setCursor(5, 73);
  tft.println("Right");
  tft.drawFastHLine(93, 76, 100, RGBcolorBGR(convert, 0xbdf7));
  tft.setCursor(86, 73);
  tft.println("<                     >");

  //for displaying EQ
  // if (eq_display_once == false) {
  for (int8_t i = 0; i < 9; i++) {
    if (i == 0) {
      for (int q = 0; q < 9; q++) {
        tft.drawFastHLine(32 + i * 22, float(125.0 + 0.3 + (q * 9.111) + q), 5, RGBcolorBGR(convert, 0xFD85));
      }
    } else {
      for (int q = 0; q < 9; q++) {
        tft.drawFastHLine(32 + 9 + i * 22, float(125 + (q * 9.111) + q), 2, RGBcolorBGR(convert, 0xFD85));
      }
    }

    tft.fillRect(45 + i * 22, 122, 3, 3, RGBcolorBGR(convert, 0xFD85));
    tft.fillRect(45 + i * 22, 207, 3, 3, RGBcolorBGR(convert, 0xFD85));


    tft.fillRect(45 + i * 22, 125, 3, 82, RGBcolorBGR(convert, 0x73ae));
    tft.fillRect(45 + i * 22, 125, 3, float((*P_Gain[i] * -1.0) * pixel_scale), RGBcolorBGR(convert, 0));

    //=== for auto cetnering to bar
    freq = num_to_str(bandFreqs[i]);
    if (freq.length() == 4) {
      freq = freq.substring(0, 1);
      freq.concat("k");
    } else if (freq.length() == 5) {
      freq = freq.substring(0, 2);
      freq.concat("k");
    }

    if (freq.length() == 2) {
      if (last_number_length == 1) {
        x_pos_sum += 25;  // last 3 now 2
      } else {
        x_pos_sum += 22;  // last 2 now 2
      }
      last_number_length = 0;
    } else if (freq.length() == 3) {

      if (last_number_length == 0) {
        x_pos_sum += 19;  // last 2 now 3
      } else {
        x_pos_sum += 22;  // last 3 now 3
      }
      last_number_length = 1;
    }
    //=================================
    freq_pos[i] = x_pos_sum;
    freq_band_name[i] = freq;
    tft.setCursor(x_pos_sum, 214);
    tft.println(freq);
  }
  //  eq_display_once = true;
  //}


  tft.drawFastVLine(34, 124, 84, RGBcolorBGR(convert, 0xFD85));
  tft.drawFastVLine(234, 122, 91, RGBcolorBGR(convert, 0xFD85));

  tft.setCursor(7, 122);
  tft.println("10dB");

  tft.setCursor(12, 165 - 3);
  tft.println("0dB");

  tft.setCursor(1, 204);
  tft.println("-10dB");

  if (display == 0) {
    // eq_display_once = false;
    if (skipbutton() == true) {  // enter the EQ

      draw_once = false;
      while (1) {

        if (digitalRead(UP_pin) == HIGH) {
          freq_index--;
          draw_once = false;
          delay(100);
        }

        if (digitalRead(DOWN_pin) == HIGH) {
          freq_index++;
          draw_once = false;
          delay(100);
        }

        // add constrain later

        if (draw_once == false) {  // redraw index to gray
          for (int i = 0; i < 9; i++) {
            tft.fillRect(45 + i * 22, 125, 3, 82, RGBcolorBGR(convert, 0x73ae));
            tft.fillRect(45 + i * 22, 125, 3, float((*P_Gain[i] * -1.0) * pixel_scale), RGBcolorBGR(convert, 0));

            if (i == freq_index) {
              //Serial.println("draw gold");
              tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  //change color for selected freq
            } else {
              tft.setTextColor(RGBcolorBGR(convert, 0x73ae));
            }
            tft.setCursor(freq_pos[i], 214);
            // Serial.println(freq_pos[i]);
            tft.println(freq_band_name[i]);

            if (update == true) {
              //Serial.println("update");
              File EQW;
              EQW = SD_MMC.open("/.EQ_save.txt", FILE_WRITE);  //erase and repopulate
                                                               // Serial.println("default EQ");
              for (uint8_t i = 0; i < 9; i++) {
                uint8_t gainint = *P_Gain[i] * -10;
                EQW.printf("%d\n", gainint);
              }
              EQW.close();
              update = false;
            }

            // Serial.print(*P_Gain[i] * -10);
            // Serial.print("  ");
            // Serial.println(*P_Gain[i]);
          }
          draw_once = true;
        }


        if (skipbutton() == true) {  // enter band
          update = true;
          tft.setTextColor(RGBcolorBGR(convert, 0x73ae));
          tft.setCursor(freq_pos[freq_index], 214);
          tft.println(freq_band_name[freq_index]);

          draw_once = false;
          *P_processing_band = freq_index;
          float Fgain = *P_Gain[freq_index];

          while (1) {

            if (*P_EQdelta_no_run_while_audio_processing == false) {
              xTaskNotifyGive(taskEQdelta);
            }

            if (digitalRead(UP_pin) == HIGH) {
              Fgain += 0.5;
              draw_once = false;
              delay(100);
            }
            if (digitalRead(DOWN_pin) == HIGH) {
              Fgain -= 0.5;
              draw_once = false;
              delay(100);
            }

            if (Fgain > 0.0) {
              Fgain = 0;
            }
            if (Fgain < -20.0) {
              Fgain = -20.0;
            }

            if (draw_once == false) {
              tft.fillRect(45 + freq_index * 22, 125, 3, 82, RGBcolorBGR(convert, 0xFD85));
              tft.fillRect(45 + freq_index * 22, 125, 3, float((Fgain * -1.0) * pixel_scale), RGBcolorBGR(convert, 0));
              *P_Gain[freq_index] = Fgain;
              draw_once = true;
            }

            vTaskDelay(pdMS_TO_TICKS(20));

            if (backbutton() == true) {
              draw_once = false;
              break;
            }
          }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
        if (backbutton() == true) {
          draw_once = false;
          break;
        }
      }
    }

  } else {
  }



  vTaskDelay(pdMS_TO_TICKS(20));
}
void Stats(bool display) {
  int8_t temp_celsius = temperatureRead();

  static int8_t index = 0;

  float used_space = *P_sdusedsize / 1024.0;
  float total_space = SD_MMC.totalBytes() / 1024.0 / 1024.0 / 1024.0;
  float free_space = total_space - used_space;

  //=========
  tft.setCursor(5, 49);
  tft.setTextColor(RGBcolorBGR(convert, 0xef7d));
  tft.println("Active time:");

  tft.setCursor(5, 65);
  tft.println("CPU Temp:");

  tft.setCursor(5, 102);
  tft.println("SD CARD:");

  tft.setCursor(5, 142);
  tft.println("Total Songs:");
  //=============

  //==============
  tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
  display_active_time();

  tft.setCursor(80, 65);
  tft.print(temp_celsius);
  tft.print(" C");

  if (temp_celsius > 70) {
    tft.setTextColor(RGBcolorBGR(convert, 0xf227));
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0x4208));
  }
  tft.setCursor(8, 77);
  tft.println("Over Heating");

  if (temp_celsius <= 30) {
    tft.setTextColor(RGBcolorBGR(convert, 0x24be));
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0x4208));
  }
  tft.setCursor(11, 85);
  tft.println("Cold");

  if (temp_celsius > 30 && temp_celsius <= 70) {
    tft.setTextColor(RGBcolorBGR(convert, 0x4d6a));
  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0x4208));
  }
  tft.setCursor(41, 85);
  tft.println("Normal");
  //==========

  //======= sdcard // when updating must stop clsoe all other
  tft.setSwapBytes(1);
  tft.pushImage(4, 112, 30, 20, sdcard_icon);
  tft.setSwapBytes(0);

  tft.setTextColor(RGBcolorBGR(convert, 0xbdf7));
  tft.setCursor(46 - 6, 113);
  tft.print(used_space, 1);  // used space
  tft.println(" GB");

  tft.setCursor(190 + 6, 113);
  tft.print(total_space, 1);  // total space
  tft.println(" GB");

  tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
  tft.setCursor(51 + 6 * 5, 125);
  tft.print((100 - (free_space / total_space) * 100), 1);  // free space
  tft.print("% / ");
  tft.print(free_space, 1);
  tft.printf(" GB Free");


  tft.fillRect(82 + 6, 113, 100 - (free_space / total_space) * 100, 7, RGBcolorBGR(convert, 0xd508));
  tft.fillRect(80 + 6, 113, 2, 7, RGBcolorBGR(convert, 0xd508));
  tft.fillRect(183 + 6, 113, 2, 7, RGBcolorBGR(convert, 0xd508));
  //=======

  tft.setTextColor(RGBcolorBGR(convert, 0xd508));
  tft.setCursor(83, 142);
  tft.println(*P_SongAmount);


  if (display == 0) {
    if (digitalRead(DOWN_pin) == HIGH) {
      if (timeout(100) == true) {
        index++;
      }
    }

    if (digitalRead(UP_pin) == HIGH) {
      if (timeout(100) == true) {
        index--;
      }
    }

    if (index > 1) {
      index = 0;
    }
    if (index < 0) {
      index = 1;
    }


    if (index == 0) {
      tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
      if (digitalRead(Skip_pin) == HIGH) {
        tft.fillRect(83, 142, 40, 7, 0);

        tft.setCursor(14, 154);
        tft.println("UPDATING ENTRIES...");

        Updating_songs_EX(83, 142);


        tft.setCursor(14, 154);
        tft.println("SAVING TO RAM      ");

        Update_songs_name(Songname);

        tft.setCursor(14, 154);
        tft.println("DONE!              ");

        delay(1000);
      }
    } else {
      tft.setTextColor(RGBcolorBGR(convert, 0x4208));
    }
    tft.setCursor(14, 154);
    tft.println("UPDATE ALL");

    if (index == 1) {
      tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
      tft.setCursor(88, 166);

      if (*P_Aupdate == false) {
        tft.println("N");
      } else {
        tft.println("Y");
      }

      if (digitalRead(Skip_pin) == HIGH) {

        autoupdate = SD_MMC.open("/.AutoUpdate.txt", FILE_WRITE);

        if (*P_Aupdate == false) {
          *P_Aupdate = true;
          delay(100);
        } else {
          *P_Aupdate = false;
          delay(100);
        }
        autoupdate.printf("%d\n", *P_Aupdate);
        autoupdate.close();
      }

    } else {
      tft.setTextColor(RGBcolorBGR(convert, 0x4208));
    }

    tft.setCursor(14, 166);
    tft.println("Auto UPDATE:");


  } else {

    tft.setTextColor(RGBcolorBGR(convert, 0x4208));
    tft.setCursor(14, 154);
    tft.println("UPDATE ALL");

    tft.setTextColor(RGBcolorBGR(convert, 0x4208));
    tft.setCursor(14, 166);
    tft.println("Auto UPDATE:");

    tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
    tft.setCursor(88, 166);

    if (*P_Aupdate == false) {
      tft.println("N");
    } else {
      tft.println("Y");
    }
  }

  vTaskDelay(pdMS_TO_TICKS(1));
}
void BleT(bool display) {
  vTaskDelay(pdMS_TO_TICKS(1));
}
//==========

uint16_t RGBcolorBGR(bool convert, uint16_t color) {
  if (convert == false) {
    return color;
  } else {
    uint16_t bgr565_color =
      (color & 0x07E0) |        // 1. Keep the Green bits (0000 0111 1110 0000)
      (color & 0xF800) >> 11 |  // 2. Move Red (F800) to the Blue position (001F)
      (color & 0x001F) << 11;   // 3. Move Blue (001F) to the Red position (F800)
    return bgr565_color;
  }
}

//========================
//true = run, false = not run // activate == run now
bool timeout(int timeouttime) {

  static unsigned long lastTime = 0;
  static bool firstRun = true;

  unsigned long now = millis();

  // First trigger happens instantly
  if (firstRun) {
    firstRun = false;
    lastTime = now;
    return 1;  // anything non-zero means "trigger"
  }

  // Wait for timeout
  if (now - lastTime >= timeouttime) {
    lastTime = now;
    return true;  // return time (usually small)
  }

  return 0;  // still waiting
}
//activate wait timeout then allow
bool timeoutlater(int timeouttime) {

  static unsigned long time1 = 0;
  static unsigned long time2 = 1;

  if (time2 == 0) {
    time2 = millis();
  }

  time1 = millis();

  if (time1 > time2 + timeouttime) {
    time2 = 0;
    // Serial.println("timeout true");
    return false;
  } else {
    //Serial.println("timeout false");
    return true;
  }
}
//any thing long than 800ms should be use as a seperrate function
bool timeout_wave(int timeouttime) {

  static unsigned long time1 = 0;
  static unsigned long time2 = 0;

  if (time2 == 0) {
    time2 = millis();
  }

  time1 = millis();

  Serial.println("===========");
  Serial.println(time2);
  Serial.println(time1);

  if (time1 > time2 + timeouttime) {
    time2 = 0;
    // Serial.println("timeout true");
    return true;
  } else {
    //Serial.println("timeout false");
    return false;
  }
}
bool timeout_artcover(int timeouttime) {

  static unsigned long time1 = 0;
  static unsigned long time2 = 0;

  if (time2 == 0) {
    time2 = millis();
  }

  time1 = millis();

  if (time1 > time2 + timeouttime) {
    time2 = 0;
    // Serial.println("timeout true");
    return true;
  } else {
    //Serial.println("timeout false");
    return false;
  }
}
//=================



//=============
//work by removing half and half
void coverart120(void* pvParameters) {
  const uint8_t rows = 60;
  const int pixel_to_read = 240 * rows * 2;  // reading from a 240x240
  const int pixel_chunk = 3 * pixel_to_read;
  int shiftdown = -rows;  //239
  //long bytered = 0;
  int countpixel = -1;
  //int countR = 1;

  byte* PixelSamples = NULL;
  int8_t byte1;
  int8_t byte2;
  int8_t byte3;
  BMPIMG120.seek(54);
  int count = -2;

  // int couting_px = 0;

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    //*P_songs_menu_song_change = true;

    if (*P_songs_menu_song_change == true) {
      PixelSamples = (byte*)heap_caps_malloc(pixel_chunk, MALLOC_CAP_SPIRAM);

      for (int run = 0; run < 2; run++) {
        BMPIMG120.read(PixelSamples, pixel_chunk);

        shiftdown += rows;
        int x;
        uint16_t rgb565[120 * 60];


        for (int i = 0; i < pixel_to_read; i += 2) {  // converting rgb888_to_rgb565
          x = i * 3;

          count += 2;

          countpixel++;

          byte1 = PixelSamples[x + 2] >> 3;  //RGB
          byte2 = PixelSamples[x + 1] >> 2;  //G
          byte3 = PixelSamples[x] >> 3;      //B

          rgb565[countpixel] = (byte3 << 11 | byte2 << 5 | byte1 << 0);


          if (count > 236) {
            if ((i + 2 / 240) % 2 == 0) {

              i += 240;
            }
            count = -2;
          }
        }

        countpixel = -1;

        art120_sprite.pushImage(0, shiftdown, 120, 60, rgb565);
      }
      BMPIMG120.close();

      shiftdown = -rows;  // reset

      heap_caps_free(PixelSamples);
      PixelSamples = NULL;
      *P_art120done = true;
      *P_songs_menu_song_change = false;
      // art120_sprite.pushSprite(118, 118);
    } else {
      // art120_sprite.pushSprite(118, 118);
      Serial.println("old art");
      *P_art120done = true;
      BMPIMG120.close();
    }
  }
}
//return if artcover exist or not, 1 = no, 2 = yes
int8_t openingbmp120(String songname) {
  songname = songname.substring(0, songname.length() - 4);
  songname.concat(".bmp");

  activetime.close();
  //Serial.println(songname);

  BMPIMG120 = SD_MMC.open(songname, FILE_READ);  // Open the wav file

  if (BMPIMG120 == false) {

    return 1;
  } else {
    BMPIMG120.read((byte*)&bmpheader, 54);

    // Serial.print("draw120");
    xTaskNotifyGive(taskdraw120art);
    return 2;
  }
}
void noartcover120() {
  // art120_sprite.fillSprite(0);

  //tft.fillRect(118, 118, 120, 120, 0);

  tft.drawFastHLine(118, 118, 14, RGBcolorBGR(convert, 0xd000));
  tft.drawFastVLine(118, 118, 14, RGBcolorBGR(convert, 0xd000));

  tft.drawFastHLine(223, 118, 14, RGBcolorBGR(convert, 0xd000));
  tft.drawFastVLine(237, 118, 14, RGBcolorBGR(convert, 0xd000));

  tft.drawFastVLine(118, 223, 14, RGBcolorBGR(convert, 0xd000));
  tft.drawFastHLine(118, 237, 14, RGBcolorBGR(convert, 0xd000));

  tft.drawFastHLine(223, 237, 14, RGBcolorBGR(convert, 0xd000));
  tft.drawFastVLine(237, 223, 14, RGBcolorBGR(convert, 0xd000));

  tft.setTextColor(RGBcolorBGR(convert, 0xd000));
  tft.setTextSize(1);
  tft.setCursor(128, 171);
  tft.println("120x120 Cover Art");
}
//====================

uint16_t presetcolor(String colorname) {
  //0xd508 light gold

  //0xFD85 bright gold

  return 1;
}

//===========
void counttime(void* pvParameters) {
  // unsigned long timerun_sec;
  //unsigned long sec_get;

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    activetime = SD_MMC.open("/.ActiveTime.txt", FILE_WRITE);
    *P_timerun_sec = *P_sec_get + (millis() / 1000);
    activetime.printf("T:%d\n", *P_timerun_sec);
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}
void display_active_time() {
  uint8_t sec = 0;
  uint8_t min = 0;
  uint8_t hr = 0;
  uint16_t day = 0;

  sec = *P_timerun_sec % 60;
  min = (*P_timerun_sec / 60) % 60;
  hr = (*P_timerun_sec / 3600) % 24;
  day = *P_timerun_sec / 86400;

  tft.setCursor(104 - 12, 48);
  tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
  tft.setTextSize(1);
  tft.print(day);
  tft.print(" days");

  tft.setCursor(159, 48);
  if (hr < 10) {
    tft.printf("0%d:", hr);
  } else {
    tft.printf("%d:", hr);
  }

  if (min < 10) {
    tft.printf("0%d:", min);
  } else {
    tft.printf("%d:", min);
  }

  if (sec < 10) {
    tft.printf("0%d", sec);
  } else {
    tft.printf("%d", sec);
  }
}
//=========

//==================pointers only use for sprite
uint16_t frame_width = 0;
uint16_t* P_frame_width = &frame_width;

uint16_t frame_height = 0;
uint16_t* P_frame_height = &frame_height;

uint16_t sprite_x = 0;
uint16_t* P_sprite_x = &sprite_x;

uint16_t sprite_y = 0;
uint16_t* P_sprite_y = &sprite_y;

uint16_t frame = 0;
uint16_t* P_frame = &frame;

bool done_animate = 0;
bool* P_done_animate = &done_animate;
//============
void bmpsprite(String Sprite_name, uint16_t frame_height, uint16_t frame_width, int16_t x, int16_t y, bool loop) {

  *P_frame_width = frame_width;
  *P_frame_height = frame_height;
  *P_sprite_x = x;
  *P_sprite_y = y;

  //==================== open sprite file
  Sprite_name.concat(".bmp");  //auto add .bmp

  BMPIMG = SD_MMC.open(Sprite_name, FILE_READ);  // Open the sprite file

  if (BMPIMG == false) {
    Serial.println("cant find file");

    return;
  } else {
    Serial.println("yep it exist");

    BMPIMG.read((byte*)&bmpheader, 54);
  }
  // delay(3000);
  //============


  uint16_t tottal_witdth = bmpheader.Width;  //66
  uint16_t total_height = bmpheader.Height;  //648

  uint16_t frame = total_height / frame_height;  //27
  *P_frame = frame;
  uint16_t framedelay[frame];  //1 = 10ms

  // ani_sprite.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
  // ani_sprite.writedata(TFT_MAD_MY);

  ani_sprite.createSprite(frame_width, frame_height);

  ani_sprite.fillSprite(2124);


  //=================
  BMPIMG.seek(54);
  *P_done_animate = false;
  xTaskNotifyGive(taskdrawsprite);

  while (!*P_done_animate) {
    Serial.print(*P_done_animate);
    if (*P_done_animate == true) {
      break;
    }
  }

  Serial.println(*P_done_animate);
  tft.setRotation(0);
  //===================
}
void drawSpritetask(void* pvParameters) {

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    int8_t byte1;
    int8_t byte2;
    int8_t byte3;
    uint8_t padding = ((4 - ((*P_frame_width * 3) % 4)) % 4);                             //padding byte
    unsigned long byte_per_frame = (*P_frame_height) * ((*P_frame_width * 3) + padding);  // asumming that we are reading 24bit color 4752 byte
    unsigned long pixels_to_read = (*P_frame_height) * (*P_frame_width);
    uint16_t rgb565[pixels_to_read];

    byte* SpriteSamples = NULL;



    for (uint8_t frame = 0; frame < *P_frame; frame++) {

      SpriteSamples = (byte*)heap_caps_malloc(byte_per_frame, MALLOC_CAP_SPIRAM);

      unsigned long sk = 0;  //sk have no meaning, it suppose to be skip but that make no sense
      uint16_t sk_count = 0;

      BMPIMG.read(SpriteSamples, byte_per_frame);

      for (int i = 0; i < pixels_to_read; i++) {  // converting rgb888_to_rgb565

        byte1 = SpriteSamples[sk + 2] >> 3;
        byte2 = SpriteSamples[sk + 1] >> 2;
        byte3 = SpriteSamples[sk] >> 3;

        sk += 3;
        sk_count++;

        if (sk_count == *P_frame_width) {
          sk += 2;
          sk_count = 0;
        }

        rgb565[i] = (byte3 << 11 | byte2 << 5 | byte1 << 0);
      }

      ani_sprite.pushImage(0, 0, *P_frame_width, *P_frame_height, rgb565);
      ani_sprite.pushSprite(*P_sprite_x, *P_sprite_y);

      memset(SpriteSamples, 0, byte_per_frame);

      SpriteSamples = NULL;

      vTaskDelay(pdMS_TO_TICKS(100));
    }

    *P_done_animate = true;
  }
}
//===================

uint16_t colorbrightness(int16_t colorS, int8_t brightness) {

  uint8_t red = (colorS & 0xF800) >> 11;
  uint8_t green = (colorS & 0x07E0) >> 5;
  uint8_t blue = (colorS & 0x001F);

  uint16_t output;

  uint8_t R = red / brightness;
  uint8_t G = green / brightness;
  uint8_t B = blue / brightness;

  output = (R) << 11 | (G) << 5 | B << 0;
  return output;
}
