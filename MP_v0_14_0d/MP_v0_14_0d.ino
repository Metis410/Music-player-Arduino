//------------------------------------------------------------------------------------------------------------------------
// credit
// Hesitant Metis
// XTronical (basic template)
// All the lib i use

#include "animation.h"
#include "icon.h"

#include "FS.h"
#include "SD_MMC.h"
#include <string>

#include "esp_heap_caps.h"
#include <FreeRTOS.h>

#include "SPI.h"
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AS5600.h>

#include <TFT_eSPI.h>

#include "driver/i2s.h"  // Library of I2S routines, comes with ESP32 standard install

//should test putting 3 screen mode in 1 task// later note: made some optimistation and its actually more reponsisve now, still wanna try it though// later note: nope working on inner song most og the time
#define version "v.0.13.0d"
/*

optimize drawing for 120 art cover// reduce the amount???//needed???

optimize drawing for inner menu// optimize the song selection part there is noo need draw content of the whole page for every sleection movoment, just do it for 2 right next to it (cant be cues using sprite)

optimize drawing for oled(nah)

need to add some ways to stop wave draw when art is processing, right now its good but who know what crash might come up

Since esp32s3 doesnt have classic bluetooth, will need a esp32 wroom to act as a BleT transmitstor

**general optimization**
**Find crash**

//should add cute 120x120 art to the blank setting space. should add nessesary info onto the top blank space
optimize the reading fucntion so 1000 String array wont be needed, use about 100 to 500 is pretty good
100 to 150bytes space for each song title, this will make it easy to index and seek
reading 250 song in the log_song file took 0.19s including seek() 
*/

//should make a program to covert all 240x240 to 120 for easy and fast display

//processing 240 art took about 0.35s from opening to push -> about 0.4s
//processing 240 art into 120 took about 0.15s from opening to push -> about 0.4s // need optimize
//processing 120 art took about 0.05s

// for large song list, over 1000 songs// ok this things may not build to have more than 1000, due to not having a search funtion// i guess every thing take up space
// have multiple file of around 500 to 1000 songs name

String Version = version;

#define Back_pin 46
#define Skip_pin 45
#define PP_pin 35
#define SM_pin 47
#define RW_pin 42  //temp pin
#define UP_pin 41
#define DOWN_pin 40

#define TFT_CS 15
#define TFT_RST 14  // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC 10

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define I2S_DOUT 5  // i2S Data out oin
#define I2S_BCLK 6  // Bit clock
#define I2S_LRC 7   // Left/Right clock, also known as Frame clock or word select
#define I2S_NUM 0   // i2s port number

#define SD_clk 2  //sd mmc pin
#define SD_cmd 1  //sd mmc pin
#define SD_d0 4   //sd mmc pin

#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS -1
#define TFT_DC 10
#define TFT_RST 14

bool convert = 1;

Adafruit_AS5600 as5600;
TFT_eSPI tft = TFT_eSPI();

TFT_eSprite sprite = TFT_eSprite(&tft);
//TFT_eSprite wave = TFT_eSprite(&tft); //terrible
TFT_eSprite setting_qi = TFT_eSprite(&tft);
TFT_eSprite art120_sprite = TFT_eSprite(&tft);
TFT_eSprite art120_sprite_AL = TFT_eSprite(&tft);
TFT_eSprite art120_sprite_PL = TFT_eSprite(&tft);

TFT_eSprite banner = TFT_eSprite(&tft);
TFT_eSprite profile = TFT_eSprite(&tft);

TFT_eSprite small_songs_menu = TFT_eSprite(&tft);
TFT_eSprite small_Album_menu = TFT_eSprite(&tft);
TFT_eSprite small_playlist_menu = TFT_eSprite(&tft);

TFT_eSprite inner_songs_page1 = TFT_eSprite(&tft);
TFT_eSprite inner_songs_page2 = TFT_eSprite(&tft);

TFT_eSprite ani_sprite = TFT_eSprite(&tft);

TFT_eSprite sd_insert_ani_sprite = TFT_eSprite(&tft);
TFT_eSprite sd_read_ani_sprite = TFT_eSprite(&tft);


#define OLED_RESET -1        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUM_BYTES_TO_READ_FROM_FILE 1024 * 20  // How many bytes to read from wav file at a time
#define NUM_BYTES_TO_READ_REVERSE 1024 * 13

#define Albums_artis_limit 100     // avg user, i think
#define Albums_limit 120           // per artis. may be to big, though including EP
#define Songs_per_Album_limit 100  // plenty of room

#define Playlists_limit 100           // avg user, i think
#define Songs_per_Playlist_limit 100  // if avg song is 3min then its about 5h

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

struct Artis_Folder_Struct {  // artis folder
  String Artis_Folder_path[Albums_artis_limit];
} Artis_Folder_List;
struct AlbumList_Struct {  // ablbum file for that artis folder
  String Album_name[Albums_limit];
} AlbumList;
struct Album_Struct {  // song in that album
  String Song_name[Songs_per_Album_limit];
};  // AlbumS;
Album_Struct Playing_list;
Album_Struct AlbumB;
//one for playing
//one for browsing while playing or not

struct PlayList_list_Struct {
  String Playlist_name[Playlists_limit];
} PlayList_list;

struct PlayList_Struct {
  String Song_name[Songs_per_Playlist_limit];
};  //PlayListS;
//PlayList_Struct PlayListP;
PlayList_Struct PlayListB;
//one for playing
//one for browsing while playing or not

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

struct setting_preview_info_Struct {
  float sd_precent_used_space;
  float remaining_space;
  uint16_t albums;
  uint16_t songs;
  uint8_t playlists;
} setting_preview_info;

//------------------------------------------------------------------------------------------------------------------------
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
//File autoupdate;

TaskHandle_t wavedraw;
TaskHandle_t taskdrawoled;
TaskHandle_t taskartcover;
TaskHandle_t tasknoartcover;
TaskHandle_t taskreadingwavfile;
TaskHandle_t taskplaybackspeed;
TaskHandle_t taskdraw120art;
TaskHandle_t taskmenu;
TaskHandle_t task_report;
TaskHandle_t tasksavetime;
TaskHandle_t taskdrawsprite;
TaskHandle_t taskEQdelta;
TaskHandle_t taskdiskcontroll;

//getting things
QueueHandle_t waveform_queue;
QueueHandle_t byteredfortime;
QueueHandle_t getsongname;
QueueHandle_t getalbumname;
QueueHandle_t getplaystate;

//sending
QueueHandle_t sendsample;
QueueHandle_t Rsendsample;

QueueHandle_t run_draw_wave;
QueueHandle_t run_draw_cover_art;
QueueHandle_t run_draw_no_cover_art;
QueueHandle_t allow_run_draw_cover_art;
QueueHandle_t allow_run_draw_wave;

//wave drawing // must be here for the queue 6 36     5 42
int8_t sample_chunk = 4;
int8_t zoom = 3;
int waveget = (240 / zoom) + 1;
//int data_point_spacing = 38;
int data_point_spacing = (NUM_BYTES_TO_READ_FROM_FILE / (waveget * sample_chunk)) / 2;  // the number controll how many point of data to get, in short: it controll the zoom, higher its the more it zoom
int* P_data_point_spacing = &data_point_spacing;


//int data_point_spacing = NUM_BYTES_TO_READ_FROM_FILE / waveget;  // the number controll how many point of data to get, in short: it controll the zoom


uint64_t sec_get = 0;
uint64_t* P_sec_get = &sec_get;


void setup() {
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
  Wire.begin(8, 9);      //sda//scl
  // SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN); // already in the eSPI TFT user setup

  //===========================================
  if (!as5600.begin()) {
    Serial.println("Could not find AS5600 sensor, check wiring!");
    while (1)
      delay(10);
  } else {
    Serial.println("AS5600 found!");
  }

  as5600.enableWatchdog(false);
  // Normal (high) power mode
  as5600.setPowerMode(AS5600_POWER_MODE_NOM);
  // No Hysteresis
  as5600.setHysteresis(AS5600_HYSTERESIS_OFF);

  // analog output
  as5600.setOutputStage(AS5600_OUTPUT_STAGE_ANALOG_FULL);

  // OR can do pwm!
  // as5600.setOutputStage(AS5600_OUTPUT_STAGE_DIGITAL_PWM);
  // as5600.setPWMFreq(AS5600_PWM_FREQ_920HZ);

  // setup filters
  as5600.setSlowFilter(AS5600_SLOW_FILTER_16X);
  as5600.setFastFilterThresh(AS5600_FAST_FILTER_THRESH_SLOW_ONLY);

  // Reset position settings to defaults
  as5600.setZPosition(0);
  as5600.setMPosition(4095);
  as5600.setMaxAngle(4095);

  Serial.println("Waiting for magnet detection...");
  //===========================================


  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }

  display.display();
  display.clearDisplay();
  logooled();
  display.display();
  display.setTextWrap(0);
  //-------------------------------------------------------
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
  i2s_set_pin(i2s_num, &pin_config);
  //-------------------------------------------------------------------------------
  if (!SD_MMC.setPins(SD_clk, SD_cmd, SD_d0)) {
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

  uint64_t val = 0;
  if (SD_MMC.exists("/.ActiveTime.txt")) {

    activetime = SD_MMC.open("/.ActiveTime.txt", FILE_READ);  // must be here due to other file bmp file reading function block it from reading, i could be wrong but iam tired
    activetime.read((uint8_t*)&val, 8);
    *P_sec_get = val;
    Serial.print("T:");
    Serial.println(*P_sec_get);
  } else {
    SD_MMC.open("/.ActiveTime.txt", FILE_WRITE);
    *P_sec_get = 0;
    Serial.println("new time file");
  }

  activetime = SD_MMC.open("/.ActiveTime.txt", "r+");

  //---------------------------------------------------------------------------

  //SPI.setFrequency(16000000L);  //if there are display problem, change this maybe//really doesnt seem to cahnge anything
  tft.init();
  //tft.setSwapBytes(true);
  //tft.setSPISpeed(80000000L);  // speed to high affect the draw function//nahhh
  tft.fillScreen(0);
  tft.setRotation(0);
  tft.setTextWrap(0);


  sprite.createSprite(240, 240);
  small_songs_menu.createSprite(101, 88);  //bound for max at 17 characters
  small_Album_menu.createSprite(101, 88);
  small_playlist_menu.createSprite(101, 88);
  art120_sprite.createSprite(120, 120);
  art120_sprite_AL.createSprite(120, 120);
  art120_sprite_PL.createSprite(120, 120);
  sd_read_ani_sprite.createSprite(30, 20);
  inner_songs_page1.createSprite(108, 208);
  inner_songs_page2.createSprite(108, 208);
  setting_qi.createSprite(122, 68);

  //wave.createSprite(240, 240);


  banner.createSprite(122, 40);
  profile.createSprite(120, 120);

  inner_songs_page1.setTextWrap(0);
  inner_songs_page2.setTextWrap(0);
  small_songs_menu.setTextWrap(0);
  small_Album_menu.setTextWrap(0);
  small_playlist_menu.setTextWrap(0);
  art120_sprite_AL.setTextWrap(0);
  art120_sprite_PL.setTextWrap(0);
  setting_qi.setTextWrap(0);

  sprite.setSwapBytes(true);
  art120_sprite.setSwapBytes(true);
  ani_sprite.setSwapBytes(1);
  art120_sprite_AL.setSwapBytes(true);
  art120_sprite_PL.setSwapBytes(true);
  inner_songs_page1.setSwapBytes(1);
  inner_songs_page2.setSwapBytes(1);
  banner.setSwapBytes(1);
  profile.setSwapBytes(1);

  //banner.pushImage(0, 0, 122, 40, banner_img);
  //profile.pushImage(0, 0, 120, 120, profile_img);


  load_profile_img();
  load_banner_img();

  intro();
  EQinit();

  getting_LRB();

  playlist_save_read(PlayList_list);

  //-----------------------------------------------
  //getting things
  waveform_queue = xQueueCreate(sample_chunk, sizeof(int16_t[waveget]));

  //waveform_queue = xQueueCreate(waveget, sizeof(int16_t));

  byteredfortime = xQueueCreate(1, sizeof(uint32_t));
  getsongname = xQueueCreate(1, sizeof(String));
  getalbumname = xQueueCreate(1, sizeof(String));
  getplaystate = xQueueCreate(1, sizeof(bool));
  sendsample = xQueueCreate(1, sizeof(byte[NUM_BYTES_TO_READ_FROM_FILE]));
  Rsendsample = xQueueCreate(1, sizeof(byte[NUM_BYTES_TO_READ_REVERSE]));

  //controll run task
  run_draw_wave = xQueueCreate(1, sizeof(bool));
  run_draw_cover_art = xQueueCreate(1, sizeof(bool));
  run_draw_no_cover_art = xQueueCreate(1, sizeof(bool));
  allow_run_draw_cover_art = xQueueCreate(1, sizeof(bool));
  allow_run_draw_wave = xQueueCreate(1, sizeof(bool));


  // xTaskCreatePinnedToCore(ReadFileTask, "ReadFileTask", 12000, NULL, 16, &taskreadingwavfile, 0);
  xTaskCreatePinnedToCore(ReadFileTask, "ReadFileTask", 12000, NULL, 16, &taskreadingwavfile, 0);

  // xTaskCreatePinnedToCore(artcover, "artcover", 20000, NULL, 13, &taskartcover, 0);
  xTaskCreatePinnedToCore(artcover, "artcover", 20000, NULL, 12, &taskartcover, 0);

  //xTaskCreatePinnedToCore(coverart120, "coverart120", 22000, NULL, 8, &taskdraw120art, 0);
  xTaskCreatePinnedToCore(coverart120, "coverart120", 22000, NULL, 10, &taskdraw120art, 0);

  xTaskCreatePinnedToCore(drawoled, "drawoled", 10000, NULL, 4, &taskdrawoled, 0);

  xTaskCreatePinnedToCore(main_menu, "main_menu", 12000, NULL, 3, &taskmenu, 0);
  xTaskCreatePinnedToCore(waveform, "drawwaveform", 12000, NULL, 3, &wavedraw, 0);

  xTaskCreatePinnedToCore(PlayBackSpeed, "PlayBackSpeed", 3000, NULL, 3, &taskplaybackspeed, 0);
  xTaskCreatePinnedToCore(disk_audio_controll, "disk_audio_controll", 10000, NULL, 3, &taskdiskcontroll, 0);

  xTaskCreatePinnedToCore(EQslider, "EQ", 5000, NULL, 3, &taskEQdelta, 0);
  xTaskCreatePinnedToCore(report, "report", 5000, NULL, 3, &task_report, 0);

  xTaskCreatePinnedToCore(f_count_time, "tasksavetime", 5000, NULL, 3, &tasksavetime, 0);

  xTaskCreatePinnedToCore(drawSpritetask, "drawSpritetask", 5000, NULL, 2, &taskdrawsprite, 0);  // not using for now else it stack size should be 20k




  Serial.print("RAM lelf: ");
  Serial.println(heap_caps_get_free_size(MALLOC_CAP_EXEC));

  Serial.print("PSRAM lelf: ");
  Serial.println(ESP.getFreePsram());

  //Serial.println("reading artis");
  Artists_Album_read();
  //pre process
  openingbmp120(Artis_Folder_List.Artis_Folder_path[0], 0, 0, 0);
  delay(100);  //0.05ms of prcoessing
  openingbmp120(PlayList_list.Playlist_name[0], 2, 0, 0);
  delay(300);  //0.16s of processing

  tft.fillScreen(0);
}



bool flag = true;

int playindex = 224 - 1;

//pointer section
//========================

float* P_Gain[9];

uint16_t SongAmount = 0;
uint16_t* P_SongAmount = &SongAmount;

uint16_t total_albums = 0;
uint16_t* P_total_albums = &total_albums;

String ALBUM_name;
String* P_ALBUM_name = &ALBUM_name;

String SONG_name;
String* P_SONG_name = &SONG_name;

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

//===
int8_t LR_balance_vol = 0;
int8_t* P_LR_balance_vol = &LR_balance_vol;

uint8_t menu_type = 0;  // this is for img displaying for albums songs playlists
uint8_t* P_menu_type = &menu_type;

//==================================================================================== notification
bool reverse_yes = 0;  //only revrese when the sample is done playing
bool* P_reverse_yes = &reverse_yes;

bool normal_yes = true;  //only normal playback when the sample is done playing
bool* P_normal_yes = &normal_yes;

bool wait_for_new_audio_data = false;  //only normal playback when the sample is done playing
bool* P_wait_for_new_audio_data = &wait_for_new_audio_data;

bool reverse_data_R = 0;  //only revrese when the sample is done playing
bool* P_reverse_data_R = &reverse_data_R;

bool normal_data_R = 0;  //only normal playback when the sample is done playing
bool* P_normal_data_R = &normal_data_R;

bool Aupdate = 0;  //auto update
bool* P_Aupdate = &Aupdate;

bool song_changed = false;  //mainly for reseting BytesReadSoFar to 0 and letting artcover to only run once
bool* P_song_changed = &song_changed;

bool menu_selected_song = false;  // mainly used for menu selected, at v0_9_5d only one if() use it
bool* P_menu_selected_song = &menu_selected_song;

bool all_menu_reset = false;
bool* P_all_menu_reset = &all_menu_reset;

bool menu_black = true;
bool* P_menu_black = &menu_black;

bool menu_done_draw = true;
bool* P_menu_done_draw = &menu_done_draw;

bool update_done = false;
bool* P_update_done = &update_done;

bool if_updating_index = false;
bool* P_if_updating_index = &if_updating_index;

bool reverse_spin = false;
bool* P_reverse_spin = &reverse_spin;

//====
bool songs_menu_song_change = true;
bool* P_songs_menu_song_change = &songs_menu_song_change;
bool songs_menu_song_change_AL = true;
bool* P_songs_menu_song_change_AL = &songs_menu_song_change_AL;
bool songs_menu_song_change_PL = true;
bool* P_songs_menu_song_change_PL = &songs_menu_song_change_PL;
//====

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

bool change_song_notice = false;  //change_song_notice is for skip/bavk // seeing right now at v0_9_5d there are no condition relie on this
bool* P_change_song_notice = &change_song_notice;

bool art120done = false;
bool* P_art120done = &art120done;

bool changing_song = false;
bool* P_changing_song = &changing_song;

bool main_cover_art_done = true;  // this is for runing main coverart only let them run only when the art is done
bool* P_main_cover_art_done = &main_cover_art_done;

bool album_SELECTED_PLAYING = false;  // check weather the selected album is playing, this is for if for browsing other album while the current is playing
bool* P_album_SELECTED_PLAYING = &album_SELECTED_PLAYING;

bool album_song_SELECTED = false;  // for letting the other the loop() to know when to switch over to Playing_list[]
bool* P_album_song_SELECTED = &album_song_SELECTED;

bool playlist_song_SELECTED = false;  // for letting the other the loop() to know when to switch over to Playing_list[]
bool* P_playlist_song_SELECTED = &playlist_song_SELECTED;

uint8_t album_song_INDEX = 0;  // get index of selected song
uint8_t* P_playing_list_song_INDEX = &album_song_INDEX;

uint8_t album_song_AMOUNT = 0;  // get the amount of song of the cuurent album
uint8_t* P_playing_list_song_AMOUNT = &album_song_AMOUNT;

uint8_t albums_INDEX = 0;  // get the amount of song of the cuurent album
uint8_t* P_albums_INDEX = &albums_INDEX;

bool save_songs_art_state = false;
bool* P_save_songs_art_state = &save_songs_art_state;

bool save_album_art_state = false;
bool* P_save_album_art_state = &save_album_art_state;

bool save_playlis_art_state = false;
bool* P_save_playlis_art_state = &save_playlis_art_state;

bool playlist_song_INDEX = false;
bool* P_playlist_song_INDEX = &playlist_song_INDEX;

bool wait_for_art_to_be_done = false;
bool* P_wait_for_art_to_be_done = &wait_for_art_to_be_done;

// this is for checking catchup on processing and buffer running
bool buffer_done = false;
bool* P_buffer_done = &buffer_done;
bool processing = false;
bool* P_processing = &processing;

bool change_song_request = false;
bool* P_change_song_request = &change_song_request;

bool change_song_request_respond = false;
bool* P_change_song_request_respond = &change_song_request_respond;

bool normal_play_run = true;
bool* P_normal_play_run = &normal_play_run;

bool reverse_play_run = false;
bool* P_reverse_play_run = &reverse_play_run;

bool reverse_play_processing = false;  // these are mainly for if some how play back run but without any procssing notice before hand
bool* P_reverse_play_processing = &reverse_play_processing;
bool normal_play_processing = false;
bool* P_normal_play_processing = &normal_play_processing;

bool was_at_R_N = true;            // was at reverse == true,  was at normal == false;
bool* P_was_at_R_N = &was_at_R_N;  // this is mainly for getting new data instead of reading old data

//this is for checking time between the buffer and the processing, mainly checking for the time spacing
unsigned long buffer_time;
unsigned long* P_buffer_time = &buffer_time;
unsigned long processing_time;
unsigned long* P_processing_time = &processing_time;

bool oled_run = true;
bool* P_oled_run = &oled_run;

bool inner_menu_song_selected_notice = true;
bool* P_inner_menu_song_selected_notice = &inner_menu_song_selected_notice;

String Album_name = " ";
String* P_Album_name = &Album_name;

bool Album_art_general = false;  // this is for when the album name is EPs or the same name as the artis
bool* P_Album_art_general = &Album_art_general;

uint8_t artis_amount = 0;
uint8_t* P_artis_amount = &artis_amount;

uint8_t playlist_amount = 0;
uint8_t* P_playlist_amount = &playlist_amount;

unsigned long snapshot1 = 0;
unsigned long* P_snapshot1 = &snapshot1;

unsigned long snapshot2 = 0;
unsigned long* P_snapshot2 = &snapshot2;

//===========================


//unsigned long snapshot1;  // for getting time
//unsigned long snapshot2;

//song speed and vol
uint8_t Speed = 1;
uint8_t* P_Speed = &Speed;
float vol = 0.7;  //max 1

bool change_screen = false;

unsigned long wait1 = 0;
unsigned long wait2 = 0;

unsigned long wait_oled = 0;
unsigned long wait_oled2 = 0;

bool change_flag = true;

bool flagred = false;

bool force_change = false;

bool playpause = true;

int8_t art_type = 0;

bool one = false;  // this is for the drawing part mainly for artcover and noartcover make them only draw once and reset when switching to new screen

//int16_t testing_wave_draw = 0;
//int16_t* P_testing_wave_draw = &testing_wave_draw;

void loop() {
  static bool skip_wait = false;
  static bool hold_skip = 0;
  static bool hold_back = 0;
  static bool hold_menu_song_change = 0;
  static uint16_t song_amount = *P_SongAmount;
  static bool display_art_once = false;
  static uint8_t wait = 0;
  static bool oled_run = true;
  static bool playlist = 0;
  static bool album = 0;

  if (flag == false) {
    if (*P_oled_run == true) {  // block it while oled not runing = to artcover not done processing
      *P_playstate = PlayPause();
    }
    if (*P_playstate == false) {  // play == false and pause button controll the playwav
      if (*P_reverse_play_run == true) {
        ReversePlayback();
      } else if (*P_normal_play_run == true) {
        PlayWav();
        if (*P_song_complete == true) {
          *P_song_complete = false;
          flag = true;
          wait1 = millis();
          one = true;
          oled_run = false;
        }
      }
    } else {
      //f_count_time();
    }
  }

  wait2 = millis();

  //========================
  if (*P_screen_state != 0) {  // no back and skip song while in menu
    if (backbutton() == true || hold_back == true) {
      hold_back = true;
      *P_change_song_request = true;

      if (*P_playstate == true || *P_change_song_request_respond == true) {
        *P_change_song_request = false;
        playindex -= 2;
        *P_change_song_notice = true;
        *P_song_changed = true;
        flag = true;
        skip_wait = true;
        hold_back = false;
        *P_wait_for_new_audio_data = true;
      }
    }

    if (skipbutton() == true || hold_skip == true) {
      hold_skip = true;
      *P_change_song_request = true;

      if (*P_playstate == true || *P_change_song_request_respond == true) {
        *P_change_song_request = false;
        flag = true;
        *P_change_song_notice = true;
        *P_song_changed = true;
        skip_wait = true;
        hold_skip = false;
        *P_wait_for_new_audio_data = true;
      }
    }
  }
  if (*P_menu_selected_song == true || hold_menu_song_change == true) {  //menu song change
    hold_menu_song_change = true;
    *P_change_song_request = true;

    if (*P_playstate == true || *P_change_song_request_respond == true) {
      *P_change_song_request = false;
      playindex = (*P_selected_song_index) - 1;
      song_amount = *P_SongAmount;
      *P_song_changed = true;

      *P_change_song_notice = true;

      *P_menu_selected_song = false;
      flag = true;
      skip_wait = true;
      hold_menu_song_change = false;
      *P_album_SELECTED_PLAYING = false;
      *P_wait_for_new_audio_data = true;
    }
  }
  if (*P_album_song_SELECTED == true || *P_playlist_song_SELECTED == true) {  // album/playlist song change
    *P_change_song_request = true;

    if (*P_playstate == true || *P_change_song_request_respond == true) {
      *P_change_song_request = false;
      playindex = *P_playing_list_song_INDEX;
      song_amount = *P_playing_list_song_AMOUNT;

      //could have just used one
      if (*P_album_song_SELECTED == true) {
        for (int8_t i = 0; i < song_amount; i++) {
          Playing_list.Song_name[i] = AlbumB.Song_name[i];
        }
        playlist = false;
        album = true;
      }
      if (*P_playlist_song_SELECTED == true) {
        for (int8_t i = 0; i < song_amount; i++) {
          Playing_list.Song_name[i] = PlayListB.Song_name[i];
        }
        playlist = true;
        album = false;
      }

      *P_album_song_SELECTED = false;
      *P_playlist_song_SELECTED = false;
      *P_song_changed = true;
      *P_album_SELECTED_PLAYING = true;

      display_art_once = false;
      flag = true;
      skip_wait = true;
      *P_wait_for_new_audio_data = true;
    }
  }
  if (*P_update_done == true) {  //let other run when when updating is done
    *P_update_done = false;
    playindex -= 1;
    *P_song_changed = true;
    *P_menu_selected_song = false;
    flag = true;
    skip_wait = true;
    *P_wait_for_new_audio_data = true;
  }
  //==============================
  if (ScreenMode(10) == 2) {
    //onetime short delay
    xTaskNotifyGive(wavedraw);
    *P_no_art_draw_while_wave_run = true;
    one = false;
    *P_all_menu_reset = true;
    *P_menu_black = true;
    oled_run = true;

  } else if (ScreenMode(10) == 1) {
    *P_no_art_draw_while_wave_run = false;
    if (*P_song_changed == false) {  // prevent it from running  xTaskNotifyGiv twice
                                     // delay(5);
      //xQueueReset(waveform_queue);
      if (*P_draw_wave_done == true && *P_menu_done_draw == true) {  // wait for the wave drawing to be done then draw artcover
        if (one == false) {
          if (timeout_artcover(50) == true) {
            if (art_type == 1) {
              noartcover();
              oled_run = true;
              one = true;
            } else if (art_type == 2) {
              xTaskNotifyGive(taskartcover);
              one = true;
              oled_run = true;
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
      oled_run = true;
    }
  }
  //========================
  if (wait2 >= wait1 + 1000 || skip_wait == true) {
    if (flag == true) {
      Serial.println("change song");

      if (skip_wait == true && ScreenMode(10) == 1) {
        if (*P_album_SELECTED_PLAYING != true) {
          *P_oled_run = false;
        }
      }

      if (playindex > song_amount - 1) {  // restart1 prevent reading above songamountnum
        playindex = 0;
      }
      if (playindex < 0) {  // prevent going below 0
        playindex = song_amount - 1;
      }

      skip_wait = false;
      String song_path;

      if (*P_album_SELECTED_PLAYING == true) {

        if (album == true) {
          Serial.print("Album: ");
          Serial.println(AlbumList.Album_name[*P_albums_INDEX]);
          //*P_ALBUM_name = *P_Album_name;
        } else if (playlist == true) {
          Serial.print("Album: ");
          Serial.println(PlayList_list.Playlist_name[*P_albums_INDEX]);
          *P_ALBUM_name = Playlist_name_manip(PlayList_list.Playlist_name[*P_albums_INDEX]);
        }
        *P_SONG_name = manip_song_name(Playing_list.Song_name[playindex]);
        Serial.print("Playing: ");
        Serial.print(Playing_list.Song_name[playindex]);
        Serial.print(" ");
        Serial.println(playindex);

        song_path = Playing_list.Song_name[playindex];


        if (*P_Album_art_general == true) {
          *P_new_song = true;
          Serial.println("opening general art");
          art_type = openingbmp(song_path);
        } else {
          if (display_art_once == false) {
            *P_new_song = true;
            Serial.println("displaying new album art");
            if (album == true) {
              Serial.println("opening album art");
              art_type = openingbmp(AlbumList.Album_name[*P_albums_INDEX]);
            } else if (playlist == true) {
              art_type = openingbmp(PlayList_list.Playlist_name[*P_albums_INDEX]);
            }
            display_art_once = true;
          }
        }

      } else {
        *P_new_song = true;
        Serial.print("Playing song: ");
        Serial.print(Songname.songsnameS[playindex]);
        Serial.print(" ");
        Serial.println(playindex);

        song_path = Songname.songsnameS[playindex];
        xQueueSend(getsongname, &Songname.songsnameS[playindex], 0);

        art_type = openingbmp(song_path);
      }
      if (art_type == 1) {
        *P_oled_run = true;
      }
      WavFile.close();  // there can be a crash on this somehow?????
      delay(20);
      WavFile = SD_MMC.open(song_path);  // Open the wav file

      if (WavFile == false) {
        Serial.println("Could not open this file");
        flag = true;
      } else {
        WavFile.read((byte*)&WavHeader, 44);  // Read in the WAV header, which is first 44 bytes of the file.
        //DumpWAVHeader(&WavHeader);                                      // Dump the header data to serial, optional!
        if (ValidWavData(&WavHeader))                               // optional if your sure the WAV file will be valid.
          i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * 1);  //set sample rate

        flag = false;
        *P_processing_band = 20;  //Processing for every new song// could optimise by only process if the sample rate change
        xTaskNotifyGive(taskEQdelta);
        *P_changing_song = false;
      }

      if (playindex > song_amount - 1) {  // restart2 // dont know exactlly but it need two
      } else {
        playindex++;
      }

      xTaskNotifyGive(taskreadingwavfile);
      oled_run = true;
    }
  }
  xTaskNotifyGive(taskdiskcontroll);
  xTaskNotifyGive(taskdrawoled);
}


void waveform(void* pvParameters) {
  static int16_t currentSample = 0;
  static int16_t arr[2];
  bool once = false;
  int16_t wave_sample[waveget];
  unsigned long mi1;
  unsigned long mi2;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    *P_draw_wave_done = false;
    if (*P_playstate == false) {
      mi1 = millis();
      if (uxQueueMessagesWaiting(waveform_queue) != 0) {  //only run when there is 1 or more
        if (xQueueReceive(waveform_queue, &wave_sample, 0) == pdPASS) {
        }
        // wave.fillSprite(0);
        for (int i = -zoom; i <= 240 + zoom; i += zoom) {

          float posamp = wave_sample[i / zoom + 1] / 170;

          tft.fillRect(i + 1, 0, zoom, 240, 0);  //draw clearing line // must be one step ahead
          tft.drawPixel(i, 120, 0x9492);

          arr[1] = posamp;
          tft.drawLine(i, arr[0] + 120, i + zoom, arr[1] + 120, RGBcolorBGR(convert, 0xfd00));  // draw connecting line from point
          arr[0] = arr[1];
        }
        // wave.pushSprite(0, 0);
      }
      mi2 = millis();
      if (*P_song_changed == false) {
        if (*P_reverse_play_run != true) {
          fpslock(30, (mi2 - mi1), uxQueueMessagesWaiting(waveform_queue));  //
        }
      }
      once = false;
    } else {
      tft.fillScreen(0);
      tft.drawFastHLine(0, 120, 240, RGBcolorBGR(convert, 0xfd00));  // draw mid line
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    *P_draw_wave_done = true;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
void fpslock(float fps, unsigned long fr, int8_t sample_coming) {
  unsigned long mi1;
  unsigned long mi2;
  static float fps_inc = 0;
  // static int8_t fps_cap = fps;
  static int count_0_sample = 0;
  static int count_sample = 0;
  float fps_cap = 1000 / (fr + 0.1);


  //this isnt concret logic
  if (sample_coming == 0) {
    count_0_sample++;
  } else if (count_sample > 2) {
    count_0_sample = 0;
  } else if (sample_coming != 0) {
    count_sample++;
  } else {
    count_sample = 0;
  }
  if (count_0_sample > 1) {
    if (fps > 0) {
      fps_inc -= 0.05;
    }
  } else if (count_sample > 2) {
    fps_inc += 0.05;
    if (fps_inc >= fps_cap) {
      fps_inc = fps_cap;
    }
  }

  fps = fps + fps_inc;
  // Serial.print("fps: ");
  //Serial.println(fps);

  mi1 = millis();
  unsigned long time = 1000.0 / fps;
  while (mi2 <= mi1 + (time - fr)) {
    vTaskDelay(pdMS_TO_TICKS(1));
    mi2 = millis();
  }
}

void report(void* pvParameters) {
  while (1) {
    // Serial.printf("Free: %u  Largest: %u\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM), heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    // Serial.print("RAM lelf: ");
    // Serial.println(heap_caps_get_free_size(MALLOC_CAP_EXEC));
    // Serial.print("Internal RAM free: ");
    // Serial.println(heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    // Serial.print("PSRAM free: ");
    // Serial.println(heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

//===============
void drawoled(void* pvParameters) {

  uint32_t bytetime = 10;
  bool once = false;
  String name;
  String nameog;
  String namecheck;
  String nameartis;
  String namesong;
  int strL;

  int rolling;
  int rolling2;

  bool playback_state;

  display.setTextSize(1);
  display.setTextColor(1);

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while (1) {
      if (*P_oled_run == true) {
        if (once == false)
          break;
      }
      vTaskDelay(pdMS_TO_TICKS(50));
      Serial.println("wating");
    }

    int count = 0;
    display.clearDisplay();
    display.setCursor(0, 0);

    if (xQueueReceive(byteredfortime, &bytetime, pdMS_TO_TICKS(10)) == pdPASS) {
    }

    if (*P_album_SELECTED_PLAYING == true) {
      nameartis = *P_ALBUM_name;
      namesong = *P_SONG_name;
    } else {
      if (xQueueReceive(getsongname, &nameog, pdMS_TO_TICKS(10)) == pdPASS) {
      }
      //song name processing part
      //------------------------------------------------
      if (namecheck != nameog) {
        namecheck = nameog;  // check for new name, prevent it from runing all the time
        name = namecheck;

        //rolling = 0;  // must reset or if two song with long name run in sequence the next song name will disappear
        //rolling2 = 0;
        //Serial.printf("|%s|", name);
        //Serial.printf("|%s|", name.c_str());
        //Serial.println("");
        name = name.substring(1, name.length() - 4);  //remove / and .wav

        if (name.charAt(name.length() - 1) == '.') {
          name = name.substring(0, name.length() - 1);
        }
        // this need to be better

        while (count <= name.length()) {
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
    }

    //displaying parts
    // if (namesong == "dU5k M3Igb VkgN Wsxbg") { // commentd out due to inconsistancy with changing song
    //   //Serial.println("glitching");
    //
    //   switch (random(0, 4)) {
    //     case 0:
    //       display.startscrollleft(0, 100);
    //       break;
    //     case 1:
    //       display.startscrolldiagleft(0, 100);
    //       break;
    //     case 2:
    //       display.startscrollright(0, 100);
    //       break;
    //     case 3:
    //       display.startscrolldiagright(0, 100);
    //       break;
    //   }
    // } else {
    //   display.stopscroll();
    // }
    //display.startscrollleft(0, 20); cool glitch effect if use this wrongly

    //display song name
    int pxstrlen = 5 * nameartis.length() + nameartis.length() - 1;  // calculating the string pixel lenght, at size 1: 5x7px
    if (nameartis.length() > 21) {
      display.setCursor(rolling, 2);
      display.println(nameartis);
      display.setCursor(rolling + pxstrlen + 20, 2);
      display.println(nameartis);

      if (*P_reverse_play_run == true) {
        if (rolling >= 0) {
          rolling = -pxstrlen - 20;
        }
        display.setCursor(rolling + pxstrlen + 7, 2);
        display.println(">");
        rolling++;
      } else {
        if (rolling + pxstrlen + 20 <= 0) {  // scrolls text if longer than 21 character
          rolling = 0;
        }
        display.setCursor(rolling + pxstrlen + 7, 2);
        display.println("<");
        rolling--;
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
      if (*P_reverse_play_run == true) {

        if (rolling2 >= 0) {
          rolling2 = -pxstrlen2 - 20;
        }
        display.setCursor(rolling2 + pxstrlen2 + 7, 22);
        display.println(">");
        rolling2++;
      } else {

        if (rolling2 + pxstrlen2 + 20 <= 0) {  // scrolls text if longer than 21 character
          rolling2 = 0;
        }
        display.setCursor(rolling2 + pxstrlen2 + 7, 22);
        display.println("<");
        rolling2--;
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
    if (*P_reverse_spin == true) {
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
    //activetime.close();

    *P_EQdelta_no_run_while_audio_processing = true;

    if (*P_reverse_spin == true || *P_reverse_play_processing == true) {  //reverse reading when high
      *P_reverse_play_processing = true;
      //*P_reverse_data_R = true;
      BytesReadSoFar -= NUM_BYTES_TO_READ_REVERSE;
      seek_to_byte = BytesReadSoFar;
      if (seek_to_byte <= 44) {
        seek_to_byte = 44;
      }
      WavFile.seek(seek_to_byte);
      WavFile.read(RSample, NUM_BYTES_TO_READ_REVERSE);
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

      xQueueSend(Rsendsample, &RSample, 0);
      *P_reverse_play_processing = false;

    } else if (*P_reverse_spin == false) {

      // *P_normal_play_processing = true;
      //*P_normal_data_R = true;
      *P_processing = true;

      if (BytesReadSoFar + NUM_BYTES_TO_READ_FROM_FILE > WavHeader.DataSize) {  // If next read goes past the end then adjust the
        BytesToRead = WavHeader.DataSize - BytesReadSoFar;                      // amount to read to whatever is remaining to read
      } else {
        BytesToRead = NUM_BYTES_TO_READ_FROM_FILE;  // Default to max to read
      }

      if (BytesToRead == 0) {  // this should prevent divided by 0 crash
        Serial.println("BytesToRead = 0 some how?");
        Serial.println("We got problem!!!");
        Serial.println("Seeking back");
        WavFile.seek(44);
        //BytesToRead = 1;
      }

      WavFile.read(Sample, BytesToRead);
      EQ(Sample, BytesToRead, 0);
      // *P_processing_time = millis();
      xQueueSend(sendsample, &Sample, 0);

      //*P_normal_play_processing = false;

      *P_processing = false;

      if (*P_song_changed == true) {  // if song change is detected this will reset the reading// this is for skip/bakc
        BytesReadSoFar = 0;
        *P_song_changed = false;
      }
      xQueueSend(byteredfortime, &BytesReadSoFar, 0);
      BytesReadSoFar += BytesToRead;

      if (BytesReadSoFar >= WavHeader.DataSize) {  // Have we read in all the data?
        *P_song_complete = true;
        BytesReadSoFar = 0;  // Clear to no bytes read in so far
      }
      *P_bytetoread = BytesToRead;
    }
    *P_EQdelta_no_run_while_audio_processing = false;

    //f_count_time();
  }
}
void PlayWav() {
  static byte Samples[NUM_BYTES_TO_READ_FROM_FILE];
  static uint16_t BytesRead = 0;
  static bool send_notice_processing_data = false;
  static bool send_notice_processing_data_R = false;
  static int8_t sample_left;
  static int8_t wait_count = 0;
  static bool skip = false;
  // static int8_t i2s_done;
  //static int8_t count;  // this count is for extra cehcking data, when a sample is 0 it need to be 0 twice, to mean clear all data
  static bool get_reverse_data_request_send = false;
  *P_change_song_request_respond = false;

  if (*P_was_at_R_N == true) {
    send_notice_processing_data_R = false;
    send_notice_processing_data = false;
  }

  if (*P_wait_for_new_audio_data == true || skip == true) {  // for changing song while pausing
    memset(Samples, 0, NUM_BYTES_TO_READ_FROM_FILE);
    if (wait_count++ == 2) {
      *P_wait_for_new_audio_data = false;
      skip = false;
      wait_count = 0;
    }
  }

  *P_buffer_done == false;

  if (FillI2SBuffer(Samples, BytesRead) == false && *P_was_at_R_N == false && *P_wait_for_new_audio_data == false) {
    // *P_reverse_yes = false;
    //if (*P_playstate == false) {
    if ((*P_reverse_spin == false) && *P_change_song_request == false) {

      if (send_notice_processing_data == false) {  // reading once per
        xTaskNotifyGive(taskreadingwavfile);
        send_notice_processing_data = true;
      }
    }

    if (*P_reverse_spin == true) {

      if (send_notice_processing_data_R == false) {  // reading once per
        Serial.println("notice to reverse");
        xTaskNotifyGive(taskreadingwavfile);
        get_reverse_data_request_send = true;
        send_notice_processing_data_R = true;
      }
    }
    //}

  } else {
    *P_buffer_done == true;
    send_notice_processing_data = false;

    if (*P_change_song_request == true || *P_reverse_spin == true) {
      if (uxQueueMessagesWaiting(sendsample) == 0 && *P_reverse_spin == true && get_reverse_data_request_send == true) {
        Serial.println("P: I2S is empty, and no more data coming, switching to reverse");
        *P_reverse_play_run = true;
        *P_normal_play_run = false;
        return;
      } else if (uxQueueMessagesWaiting(sendsample) == 0 && *P_change_song_request == true) {
        Serial.println("P: I2S is empty, and no more data coming, P_change_song_request accept");
        *P_change_song_request_respond = true;
        skip = true;
        memset(Samples, 0, NUM_BYTES_TO_READ_FROM_FILE);  // prevent rereading old data, fill it with 0 to shut it up
        return;
      }
    }

    if (*P_processing == true && uxQueueMessagesWaiting(sendsample) == 0) {
      Serial.print("Buffer small ");
      memset(Samples, 0, NUM_BYTES_TO_READ_FROM_FILE);
      //while (1) {
      //  xTaskNotifyGive(taskreadingwavfile);
      //  if (*P_processing == false || uxQueueMessagesWaiting(sendsample) == 1) {
      //    break;
      //  }
      //}
    }

    if (xQueueReceive(sendsample, &Samples, pdMS_TO_TICKS(0)) == pdPASS) {
    }


    *P_was_at_R_N = false;
    BytesRead = *P_bytetoread;
  }
}
void ReversePlayback() {
  static byte RSamples[NUM_BYTES_TO_READ_REVERSE];
  static uint16_t BytesRead = 0;
  static bool switching = false;
  static bool send_notice_processing_data = false;
  static bool send_notice_processing_data_R = false;
  static bool once = false;
  static bool get_normal_data_request_send = false;

  if (*P_was_at_R_N == false) {
    send_notice_processing_data = false;
    send_notice_processing_data_R = false;
  }


  if (FillI2SBufferR(RSamples, BytesRead) == false && *P_was_at_R_N == true) {

    if (*P_reverse_spin == true) {
      if (send_notice_processing_data_R == false) {
        xTaskNotifyGive(taskreadingwavfile);
        send_notice_processing_data_R = true;
      }
    }

    if (*P_reverse_spin == false) {
      if (send_notice_processing_data == false) {
        Serial.println("notice to normal");
        xTaskNotifyGive(taskreadingwavfile);
        get_normal_data_request_send = true;
        send_notice_processing_data = true;
      }
    }


  } else {

    send_notice_processing_data_R = false;

    if (uxQueueMessagesWaiting(Rsendsample) == 0 && *P_reverse_spin == false && get_normal_data_request_send == true) {
      Serial.println("R: I2S is empty, and no more data coming, switching to normal");
      *P_reverse_play_run = false;
      *P_normal_play_run = true;
      return;
    }
    if (*P_was_at_R_N == true) {
      if (uxQueueMessagesWaiting(Rsendsample) == 0 && *P_reverse_play_processing == true) {
        Serial.println("R: done and empty");
      }
    }

    if (xQueueReceive(Rsendsample, &RSamples, pdMS_TO_TICKS(0)) == pdPASS) {
    }
    *P_was_at_R_N = true;
    BytesRead = NUM_BYTES_TO_READ_REVERSE;
  }
}
bool FillI2SBuffer(byte* Samples, uint16_t BytesInBuffer) {  // fully explain
  // Writes bytes to buffer, returns true if all bytes sent else false, keeps track itself of how many left
  // to write, so just keep calling this routine until returns true to know they've all been written, then
  // you can re-fill the buffer

  size_t BytesWritten = 0;        // Returned by the I2S write routine,
  static uint16_t BufferIdx = 0;  // Current pos of buffer to output next
  uint8_t* DataPtr;               // Point to next data to send to I2S
  uint16_t BytesToSend;           // Number of bytes to send to I2S
  static int avg_BytesWritten;
  //static bool mask = false;
  //static int count = 0;

  if (BytesInBuffer == 0) {
    return true;
  }

  //*P_skip_back_available = false;

  DataPtr = (Samples) + BufferIdx;                             // Set address to next byte in buffer to send out
  BytesToSend = BytesInBuffer - BufferIdx;                     //BytesToSend is the remaining byte to send, it tell write how many bytes left to expect
  i2s_write(i2s_num, DataPtr, BytesToSend, &BytesWritten, 0);  // Send the bytes, wait 1 RTOS tick to complete
  BufferIdx += BytesWritten;                                   // this set the index cursor to the next staring point

  //avg_BytesWritten = (avg_BytesWritten + BytesWritten) / 2;
  //Serial.println(avg_BytesWritten); //255 to 400 byte

  //BytesInBuffer is use to tell the write how many byte is left
  //Samples, is use as a array "decay" to a pointer, it has all the data just in a pointer packet
  // BufferIdx, this set the starting point for the write,
  // the write return how much it has red as BytesWritten and adding it to the BufferIdx and repeat
  // increasue by number of bytes actually written

  if (BufferIdx >= BytesInBuffer) {
    // sent out all bytes in buffer, reset and return true to indicate this
    BufferIdx = 0;
    return true;        // true when fully drain
  } else return false;  //false to stop filling when not all data are drained
}
bool FillI2SBufferR(byte* Samples, uint16_t BytesInBuffer) {  // fully explain
  // Writes bytes to buffer, returns true if all bytes sent else false, keeps track itself of how many left
  // to write, so just keep calling this routine until returns true to know they've all been written, then
  // you can re-fill the buffer

  size_t BytesWritten;            // Returned by the I2S write routine,
  static uint16_t BufferIdx = 0;  // Current pos of buffer to output next
  uint8_t* DataPtr;               // Point to next data to send to I2S
  uint16_t BytesToSend;           // Number of bytes to send to I2S
  static int avg_BytesWritten;
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

  //avg_BytesWritten = (avg_BytesWritten + BytesWritten) / 2;
  //Serial.println(avg_BytesWritten); //255 to 400 byte

  //BytesInBuffer is use to tell the write how many byte is left
  //Samples, is use as a array "decay" to a pointer, it has all the data just in a pointer packet
  // BufferIdx, this set the starting point for the write,
  // the write return how much it has red as BytesWritten and adding it to the BufferIdx and repeat
  // increasue by number of bytes actually written

  if (BufferIdx >= BytesInBuffer) {
    // sent out all bytes in buffer, reset and return true to indicate this
    BufferIdx = 0;
    return true;        // true when fully drain
  } else return false;  //false to stop filling when not all data are drained
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
  if (songname.charAt(songname.length() - 1) == '.') {  // for album song, idk why this has to be the case
    songname = songname.substring(0, songname.length() - 1);
  }
  songname.concat(".bmp");
  // Serial.println(songname);

  BMPIMG.close();                             // close the old one before opening new one
  BMPIMG = SD_MMC.open(songname, FILE_READ);  // Open the wav file

  if (BMPIMG == false) {
    //Serial.println("Could not open this file");
    if (*P_no_art_draw_while_wave_run == false) {
      //sprite.fillSprite(TFT_BLACK);
      //xTaskNotifyGive(tasknoartcover);
      noartcover();
      //   *P_no_art_draw_while_wave_run == true;
    }
    //
    return 1;
  } else {

    BMPIMG.read((byte*)&bmpheader, 54);
    if (bmpheader.ImgDataStart != 54) {
      BMPIMG.seek(bmpheader.ImgDataStart);  // in case each software make the data start at different index
    }

    //seem to work but might crash should add a blocking to the wave draw while its processing
    xTaskNotifyGive(taskartcover);

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
const uint8_t rows = 120 / 10;
const uint pixel_to_read = 240 * rows;
const uint pixel_chunk = 3 * pixel_to_read;
int shiftdown = -rows;          //239
int shiftdown_120 = -rows / 2;  //239
unsigned long bytered = 0;
//note bmp first pixel 0,0 is at the bottom left
//it read 720 byte mean 240px and cover art are 240x240 so doesnt need to check for remain left
//172800
byte* PixelSamples = NULL;
byte* PixelSamples_past = NULL;
void artcover(void* pvParameters) {
  //MALLOC_CAP_DEFAULT
  //MALLOC_CAP_SPIRAM
  PixelSamples = (byte*)heap_caps_malloc(pixel_chunk, MALLOC_CAP_SPIRAM);
  PixelSamples_past = (byte*)heap_caps_malloc(480 * 3, MALLOC_CAP_SPIRAM);
  memset(PixelSamples_past, 0, 480 * 3);

  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    *P_oled_run = false;
    if (*P_no_art_draw_while_wave_run == false) {
      tft.setRotation(2);
      tft.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
      tft.writedata(TFT_MAD_MY);     // command for mirroring Y // for multi rows
    }
    rgb888_to_rgb565_to_tft_rows_PSRAM();

    if (*P_no_art_draw_while_wave_run == false) {
      tft.setRotation(0);
    }
    *P_oled_run = true;
  }
}
unsigned long Stime1;
unsigned long Stime2;
bool rgb888_to_rgb565_to_tft_rows_PSRAM() {  //converter// generate both 120 and 240 art at the same time
  static uint8_t byte1;
  static uint8_t byte2;
  static uint8_t byte3;
  static uint16_t rgb565[pixel_to_read];          // do they need to be static????
  static uint16_t rgb565_120[pixel_to_read / 4];  // do they need to be static????
  static bool last_P_playstate;
  uint16_t count_correct = 0;
  bool new_check = true;

  if (*P_new_song == true) {
    last_P_playstate = *P_playstate;

    if (last_P_playstate == false) {  //if run > stop > run again
      *P_playstate = true;
    }

    Stime1 = micros();

    for (int run = 0; run < 240 / rows; run++) {  // reading twice
      BMPIMG.read(PixelSamples, pixel_chunk);

      if (new_check == true) {
        for (int check = 0; check < 480 * 3; check++) {  // check for same img data
          if (PixelSamples_past[check] == PixelSamples[check]) {
            count_correct++;
          }
          if (count_correct < 720 / 2 && check > 720) {
            Serial.println("not the same img");
            new_check = false;
            break;
          } else if (count_correct >= 720) {
            if (*P_no_art_draw_while_wave_run == false) {
              sprite.pushSprite(0, 0);
            }
            Serial.println("same img");
            Stime2 = micros();
            snapshot_time(Stime1, Stime2);
            if (last_P_playstate == false) {
              *P_playstate = false;
            }
            return 0;
          }
        }
      }

      shiftdown += rows;
      shiftdown_120 += rows / 2;
      int x = 0;
      int art120_count_pixels = 0;
      int art120_count_rows = 0;

      for (int i = 0; i < pixel_to_read; i++) {  // converting rgb888_to_rgb565

        x = i * 3;
        byte1 = PixelSamples[x + 2] >> 3;
        byte2 = PixelSamples[x + 1] >> 2;
        byte3 = PixelSamples[x] >> 3;

        if (run == 0 && i <= 480) {  // take 2 rows
          PixelSamples_past[x + 2] = PixelSamples[x + 2];
          PixelSamples_past[x + 1] = PixelSamples[x + 1];
          PixelSamples_past[x] = PixelSamples[x];
        }

        rgb565[i] = (byte3 << 11 | byte2 << 5 | byte1 << 0);

        if (i % 2 == 0) {
          if (i / 240 % 2 == 0) {
            rgb565_120[art120_count_pixels] = rgb565[i];
            art120_count_pixels++;
          }
        }
      }

      sprite.pushImage(0, shiftdown, 240, rows, rgb565);

      if (*P_inner_menu_song_selected_notice == true) {
        art120_sprite.pushImage(0, shiftdown_120, 120, 60, rgb565_120);
      }
    }

    // this should only run on inner menu
    if (*P_inner_menu_song_selected_notice == true) {
      openingbmp120("no need for name", 1, 1, 1);
      *P_songs_menu_song_change = false;
      *P_art120done = true;
      *P_inner_menu_song_selected_notice = false;
    }

    shiftdown = -rows;          // reset
    shiftdown_120 = -rows / 2;  // reset

    BMPIMG.close();

    // Serial.print("240 art ready ");
    if (*P_no_art_draw_while_wave_run == false) {
      sprite.pushSprite(0, 0);
      //Serial.println(" push");
    }

    Stime2 = micros();
    if (last_P_playstate == false) {
      *P_playstate = false;
    }

    snapshot_time(Stime1, Stime2);

  } else {
    Serial.println("already have it");
    sprite.pushSprite(0, 0);
    BMPIMG.close();
  }

  *P_new_song = false;
  return false;
}
void noartcover() {
  static int8_t pxle = 30;
  static int8_t nocartcover_x = 120;
  static int8_t nocartcover_y = 120 + 5;
  //bool nocoverart_run_flag = false;

  tft.fillScreen(0);
  //Serial.println("drawing no art cover");
  tft.drawFastHLine(3, 3, pxle, RGBcolorBGR(convert, 0xd000));  // up right
  tft.drawFastVLine(3, 3, pxle, RGBcolorBGR(convert, 0xd000));

  tft.drawFastHLine(239 - 3 - pxle, 3, pxle, RGBcolorBGR(convert, 0xd000));  // up left
  tft.drawFastVLine(239 - 3, 3, pxle, RGBcolorBGR(convert, 0xd000));

  tft.drawFastHLine(3, 239 - 3, pxle, RGBcolorBGR(convert, 0xd000));  // down right
  tft.drawFastVLine(3, 239 - 3 - pxle, pxle, RGBcolorBGR(convert, 0xd000));

  tft.drawFastHLine(239 - 3 - pxle, 239 - 3, pxle, RGBcolorBGR(convert, 0xd000));  // down left
  tft.drawFastVLine(239 - 3, 239 - 3 - pxle, pxle, RGBcolorBGR(convert, 0xd000));

  tft.setCursor(nocartcover_x - 53 / 2, nocartcover_y - 7 / 2);
  tft.setTextSize(1);
  tft.setTextColor(RGBcolorBGR(convert, 0xd000));
  tft.println("240 x 240");  //53px
  tft.setCursor(nocartcover_x - 53 / 2, nocartcover_y - 10 - 7 / 2);

  tft.println("ART COVER");  //53px
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
  name = name.substring(0, name.length() - 4);  //remove .wav
  if (name.charAt(name.length() - 1) == '.') {  // for album song, idk why this has to be the case
    name = name.substring(0, name.length() - 1);
  }
  for (uint8_t i = 0; i < name.length(); i++) {
    if (name.charAt(i) == '-') {  //taking the right half

      if (name.charAt(i + 1) == ' ') {
        if (name.charAt(i + 2) == ' ') {
          name = name.substring(i + 3, name.length());
        } else {
          name = name.substring(i + 2, name.length());
        }
        return name;

      } else {
        name = name.substring(i + 1, name.length());
        return name;
      }
    }
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

  if (*P_if_updating_index == false) {
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
  }

  return latch;
}
bool skipbutton() {  //output TRUE if press
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

bool backbutton() {  //output TRUE if press
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
    //if (*P_reverse_play_run == true) {  // for reverse
    //  if (once == false) {
    //    i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * 1);
    //    once = true;
    //  }
    //} else if (*P_normal_play_run == true) {
    //  if (once == true) {
    //    i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * 1);
    //    once = false;
    //  }
    //}
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
//================



//======== Upadating song section
void Updating_songs(int16_t x, int16_t y) {  //update song file name from sd card  to the txt file
  File log;
  File Songamountlog;
  uint32_t total_used_size = 0;
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
    song.close();
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

  Songamountlog = SD_MMC.open("/.SDinfo.txt", "r+");  //open /.SDinfo.txt to log data like used size and amount of song
  *P_SongAmount = counting_song;
  Songamountlog.seek(3);
  Songamountlog.write((uint8_t*)&counting_song, 2);
  Songamountlog.seek(8);
  Songamountlog.write((uint8_t*)&total_used_size, 4);


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

  uint32_t total_used_size = 0;
  int16_t counting_song = 0;
  static int8_t move = 0;

  tft.fillRect(46 - 6, 113, 240, 25, 0);

  File log;
  File Songamountlog;

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

    song.close();
    song = songsatroot.openNextFile();

    if (counting_song % 50 == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }

  Songamountlog = SD_MMC.open("/.SDinfo.txt", "r+");  //open /.SDinfo.txt to log data like used size and amount of song
  *P_SongAmount = counting_song;
  Songamountlog.seek(3);
  Songamountlog.write((uint8_t*)&counting_song, 2);
  Songamountlog.seek(8);
  Songamountlog.write((uint8_t*)&total_used_size, 4);


  Songamountlog.close();
  song.close();
  log.close();
}
void Update_songs_name(Songname_Struct& sa) {  //update song to Songname_Struct

  String name;
  uint32_t total_used_size = 0;
  int16_t counting_song = 0;

  SDinfo = SD_MMC.open("/.SDinfo.txt", FILE_READ);

  SDinfo.seek(3);
  SDinfo.read((uint8_t*)&counting_song, 2);
  SDinfo.seek(8);
  SDinfo.read((uint8_t*)&total_used_size, 4);

  *P_sdusedsize = total_used_size;
  *P_SongAmount = counting_song;



  float used_space = total_used_size / 1024.0;
  float total_space = SD_MMC.totalBytes() / 1024.0 / 1024.0 / 1024.0;

  setting_preview_info.sd_precent_used_space = 100 - ((total_space - used_space) / total_space) * 100;
  setting_preview_info.remaining_space = total_space - used_space;
  setting_preview_info.songs = counting_song;

  Stime1 = micros();
  logread = SD_MMC.open("/.Log_song.txt", FILE_READ);

  for (int i = 0; i < *P_SongAmount; i++) {
    name = logread.readStringUntil('\n');
    //logread.seek(0, SeekCur);
    sa.songsnameS[i] = name;
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.println(name);
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  Stime2 = micros();
  Serial.print("song reading ");
  snapshot_time(Stime1, Stime2);

  logread.close();
  SDinfo.close();
}
//============

//=============== Equalizer section
float bandFreqs[9] = { 60, 150, 400, 1000, 2400, 6000, 12000, 14000, 16000 };
float bandQ[9] = { 1.2, 1.3, 1.3, 1.3, 1.4, 1.6, 1.6, 1.6, 1.8 };
float Gain[9] = { -10, -10, -10, -10, -10, -10, -10, -10, -10 };
//float Gain[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 0 }; // sum = 9 vol 1 x=9 y=1
//float Gain[9] = { -20, -20, -20, -20, -20, -20, -20, -20, -20 };  // sum = 0.9 vol 5 x=0.9 y =5
void EQ(byte* Sample, uint16_t length, bool playbacktype) {  // EQ and volume controll

  static int16_t Left;
  static int16_t Right;
  int16_t* Sample16 = (int16_t*)Sample;

  length = length / 2;

  int16_t wave[waveget];

  //using y=mx+b;
  static float x = 9;
  static float y = 0.7;
  static float x1 = 0.9;
  static float y1 = 1;
  //float
  float lbal = 1;
  float rbal = 1;
  uint16_t data_point_count = 0;

  static float m = (y - y1) / (x - x1);
  static float b = y1 - m * x1;

  float cal_vol = m * (*P_gainSum) + b;
  float state_vol_scale = cal_vol / 9.0;
  float main_vol = (*P_vol_state) * state_vol_scale;

  //if (length != 0) {

  // vTaskDelay(pdMS_TO_TICKS(1));
  if (*P_LR_balance_vol > 0) {  // right

    lbal = (100 - *P_LR_balance_vol * 2) / 100.0;

  } else if (*P_LR_balance_vol < 0) {  // left

    rbal = (*P_LR_balance_vol * 2 + 100) / 100.0;
  }

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

    Left = (L * 32767.0 * lbal) * main_vol;
    Right = (R * 32767.0 * rbal) * main_vol;

    if (ScreenMode(10) == 2) {
      if ((i / 2) % *P_data_point_spacing == 0) {
        wave[data_point_count] = Left;
        data_point_count++;
        if (data_point_count >= waveget) {
          xQueueSend(waveform_queue, &wave, 0);
          data_point_count = 0;
        }
      }
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
  uint8_t freq[9];
  Serial.println("EQ READING");

  EQFile = SD_MMC.open("/.Setting_Save.txt", FILE_READ);

  if (EQFile.available()) {
    Serial.println("load saved EQ");

    EQFile.seek(19);
    EQFile.read(freq, 9);

    for (uint8_t i = 0; i < 9; i++) {
      P_Gain[i] = new float;

      //uint8_t toint = freq[i];

      float freqf = freq[i] / 10.0 * -1;

      *P_Gain[i] = freqf;

      Serial.println(*P_Gain[i]);
    }

  } else {
    EQW = SD_MMC.open("/.Setting_Save.txt", "r+");  //erase and repopulate

    Serial.println("default EQ");

    EQW.seek(19);

    for (uint8_t i = 0; i < 9; i++) {
      P_Gain[i] = new float;
      *P_Gain[i] = Gain[i];

      freq[i] = Gain[i] * -1;

      // EQW.printf("%d\n", gainint);
    }

    EQW.write(freq, 9);
    EQW.write(124);
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
  // autoupdate = SD_MMC.open("/.AutoUpdate.txt", FILE_READ);

  // if (autoupdate.available() == false) {
  //   update_song = false;
  // } else {
  //   AUPDATE = autoupdate.readStringUntil('\n');
  //   update_song = AUPDATE.toInt();
  // }
  //
  // *P_Aupdate = update_song;

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
  // if (update_song == true) {
  //   tft.print("TRUE");
  // } else {
  //   tft.print("FALSE");
  // }
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
    while (1) {
    }

    //int8_t fdelay[27] = { 10, 7, 15, 10, 5, 10, 10, 20, 10, 20, 10, 5, 50, 5, 5, 15, 5, 5, 10, 10, 2, 10, 10, 10, 10 };

    //while (1) {
    //  for (int8_t frame = 0; frame < 25; frame++) {

    //   // const uint16_t* framePointer = sdcard_insert_ani1[frame];

    //    //sd_insert_ani_sprite.pushImage(0, 0, 66, 24, framePointer);  //remove if there isnt enough ram space
    //    //sd_insert_ani_sprite.pushSprite(240 - 66 + 25, y + 90);

    //    // Serial.print("RAM lelf: ");
    //    // Serial.print((heap_caps_get_free_size(MALLOC_CAP_EXEC)) / 1024.0);
    //    // Serial.println("kb");

    //    //Serial.println("ghf");
    //    delay((fdelay[frame] * 10));
    //  }
    //}

  } else {
    tft.setTextColor(RGBcolorBGR(convert, 0x2724));
    tft.println("OK");
  }
  tft.setTextColor(RGBcolorBGR(convert, 0xffff));

  // const uint16_t* framePointer = sdcard_insert_ani1[14];

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

  // if (*P_Aupdate == 1) {
  //
  //   tft.print("YES");
  //   Updating_songs(x + 77, y + S * 15);
  //
  //   tft.setCursor(x + 77, y + S * 14);
  //   tft.print("DONE!");
  //
  // } else {
  //
  //   tft.print("NO");
  // }
  // tft.print(" / NOW READING");

  Update_songs_name(Songname);

  tft.setCursor(x + 77, y + S * 15);

  tft.print(*P_SongAmount);

  // delay(1000);
}
void main_menu(void* pvParameters) {
  static int8_t index = 0;
  static bool song_once = false;
  static bool album_once = false;
  static bool playlist_once = false;
  static bool setting_once = false;
  static bool menu_draw_once = true;
  static bool draw_static_again = true;
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (*P_menu_black == true || draw_static_again == true) {
      if (draw_static_again == false) {
        setting_quick_info(1);
        delay(100);
        tft.fillScreen(0);
      }
      draw_static_again = false;
      tft.setTextSize(1);
      small_songs_menu.fillSprite(0);
      tft.drawLine(115, 0, 115, 240, RGBcolorBGR(convert, 0xDD08));
      tft.drawLine(114, 13, 0, 13, RGBcolorBGR(convert, 0xFD00));
      tft.drawFastHLine(115, 115, 240 - 115, RGBcolorBGR(convert, 0xFD00));
      tft.setTextColor(RGBcolorBGR(convert, 0xFFFF));  // batt percentage
      tft.setCursor(8, 3);
      tft.print("100%");
      tft.setCursor(45, 3);
      tft.println("Est:4h 32m");
      underline(1, 1);
      *P_menu_black = false;
    }
    if (*P_all_menu_reset == true) {
      song_once = false;
      album_once = false;
      playlist_once = false;
      setting_once = false;
      *P_all_menu_reset = false;
    }

    *P_menu_done_draw = false;
    if (digitalRead(UP_pin) == HIGH) {
      index--;
      menu_draw_once = true;
      delay(100);
    }
    if (digitalRead(DOWN_pin) == HIGH) {
      index++;
      menu_draw_once = true;
      delay(100);
    }
    if (index < 0) {
      index = 3;
    }
    if (index > 3) {
      index = 0;
    }

    // to skip the rest
    if (menu_draw_once == true) {
      //if (ScreenMode(10) != 1) {

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
    }

    tft.setTextSize(1);
    switch (index) {
      case 0:
        song_once = false;
        setting_once = false;
        if (skipbutton() == HIGH) {
          while (1) {
            Album(0);
            vTaskDelay(pdMS_TO_TICKS(10));
            if (backbutton() == HIGH || ScreenMode(10) == 1) {
              album_once = false;
              break;
            }
          }
        } else {
          if (album_once == false) {
            // tft.fillRect(116, 0, 240, 10, 0);
            // setting_qi.fillSprite(0);
            // setting_qi.pushSprite(118, 1);
            setting_quick_info(1);
            tft.fillRect(116, 0, 124, 114, 0);
            if (Album(1) == true) {
              album_once = true;
            }
          }
        }

        break;
      case 1:
        album_once = false;
        playlist_once = false;

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
            Songs_inner(1);
            if (ScreenMode(10) != 1) {
              small_songs_menu.fillSprite(0);
              tft.fillScreen(0);
              draw_static_again = true;
              song_once = false;
            }
          }
        } else {
          if (song_once == false) {
            tft.fillRect(116, 0, 240, 10, 0);
            if (ScreenMode(10) != 1) {
              if (Songs(1) == true) {
                song_once = true;
              }
            }
          }
        }

        break;
      case 2:
        song_once = false;
        setting_once = false;
        if (skipbutton() == HIGH) {
          while (1) {
            PlayList(0);
            vTaskDelay(pdMS_TO_TICKS(10));
            if (backbutton() == HIGH || ScreenMode(10) == 1) {
              playlist_once = false;
              break;
            }
          }
        } else {
          if (playlist_once == false) {
            //tft.fillRect(116, 0, 240, 10, 0);
            //setting_qi.fillSprite(0);
            // setting_qi.pushSprite(118, 1);
            tft.fillRect(116, 0, 124, 114, 0);
            setting_quick_info(1);
            if (PlayList(1) == true) {
              playlist_once = true;
            }
          }
        }
        break;
      case 3:
        playlist_once = false;
        album_once = false;
        if (setting_once == false) {
          tft.fillRect(116, 0, 124, 114, 0);
          setting_once = true;
        }

        if (skipbutton() == HIGH) {
          delay(50);
          tft.fillScreen(0);
          setting();
          tft.fillScreen(0);
          draw_static_again = true;
          setting_quick_info(1);
        } else {
          setting_quick_info(0);
        }
    }

    *P_menu_done_draw = false;
    *P_menu_done_draw = true;
    vTaskDelay(pdMS_TO_TICKS(10));
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
  //String S_SongAmount;
  //String S_index;

  //========= display song index and total songs

  if (index_song == -30) {
    *P_songs_menu_index = index_song = *P_selected_song_index + 1;  //131
    index_img = *P_selected_song_index;
  }

  display_index_for_menu(233, 2, index_song, *P_SongAmount, 0xd508, 0x8c71);

  index_song = *P_songs_menu_index;  // here to sync it with the inner songs list and reverse
  index_img = index_song - 1;        // here to sync it with the inner songs list and reverse



  tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0x6B4D));
  tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0x6B4D));
  if (display == false) {
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
          tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (index_song != 1) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        if (timeout(100) == true) {
          *P_songs_menu_song_change = true;
          tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0xFD85));
          index_img--;
          S_up = true;
        }
      }
    }

    if (S_down == true || S_up == true) {
      img_type = openingbmp120(Songname.songsnameS[index_img], 1, 0, 0);
    }

    for (float q = 0; q < 20; q += 0.5) {
      if (S_up == true || S_down == true) {
        scrolls[3] = 0;
        scoll_end[3] = 1;
      }

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
              scrolls[i] = (scrolls[i] - .24 - (1 - 17.0 / name.length()) / 2.5);  // scale by string lenght
            } else {
              scrolls[i] = (scrolls[i] + .4 + (1 - 17.0 / name.length()) / 2.0);

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

    if (S_down == true || S_up == true) {
      if (img_type == 1) {
        art120_sprite.fillSprite(0);
        art120_sprite.pushSprite(118, 118);
        noartcover120();
      } else {
        while (1) {
          if (*P_art120done == true) {
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(20));
        }
      }
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
    }

    //reset
    S_up = false;
    S_down = false;
    text_y = -20;
    w = 0;

    *P_songs_menu_index = index_song;
  } else {
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

      scrolls[i] = 0;
      scoll_end[i] = 0;
    }
    small_songs_menu.pushSprite(126, 14);

    img_type = openingbmp120(Songname.songsnameS[index_img], 1, 0, 0);

    pushed = false;

    if (img_type == 1) {
      art120_sprite.fillSprite(0);
      art120_sprite.pushSprite(118, 118);
      noartcover120();
      pushed = true;
    }
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
  }


  return pushed;
}
void Songs_inner(bool display) {  // display full song without cover art

  // likely to crash to spinlock_acquire spinlock.h:122 (result == core_id || result == SPINLOCK_FREE) if you stay for too long
  // this is the 20 songs display version, splt screen
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

  static int16_t staring_song_index = 0 * 20;

  static float scrolls[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  static bool scoll_end[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  static uint16_t sprite_width = 107;

  //String name;
  String S_index;

  String song_name_page1;
  String song_name_page2;

  static uint8_t spacing = 7 + 15;
  static int16_t selected_song_color = 0xFD85;
  static int16_t page_text1_color = 0xc658;
  static int16_t page_text2_color = 0x8c71;


  //0xd508 light gold

  //0xFD85 bright gold

  //0x069f light blue

  //if (draw_again == true) {
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
  // }
  while (1) {
    S_index = num_to_str(staring_song_index);

    tft.setTextColor(RGBcolorBGR(convert, 0xffff), 0);
    tft.setTextSize(1);
    tft.setCursor(208 - 6 - (S_index.length() * 5 + S_index.length()), 9);
    tft.printf(" %d", staring_song_index + 1);
    tft.setCursor(208, 9);

    if (staring_song_index + 20 >= Songs) {
      tft.printf(" - %d ", Songs);
    } else {
      tft.printf(" - %d ", staring_song_index + 20);
    }

    //if (display == true) {
    tft.setCursor(0, 0);
    tft.println("page");

    if (page_index != (Songs - 1) / 20) {  // for when at the end of the list
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

    inner_songs_page1.pushSprite(6, 28);
    inner_songs_page2.pushSprite(127, 28);
    //}

    if (skipbutton() == HIGH) {
      while (1) {
        tft.setCursor(0, 0);
        tft.println("selection");

        if (skipbutton() == true) {  //a song is selected
          *P_inner_menu_song_selected_notice = true;
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
                scrolls[i] = (scrolls[i] - .25 - (1 - 18.0 / song_name_page1.length()) / 2.5);  // scale by string lenght
              } else {
                scrolls[i] = (scrolls[i] + .4 + (1 - 18.0 / song_name_page1.length()) / 2.0);
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

        inner_songs_page1.pushSprite(6, 28);
        inner_songs_page2.pushSprite(127, 28);

        vTaskDelay(pdMS_TO_TICKS(10));
        if (backbutton() == HIGH || ScreenMode(10) == 1) {
          break;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
    if (backbutton() == HIGH || ScreenMode(10) == 1) {
      break;
    }
  }
}
//=======================
void display_index_for_menu(int16_t x, int16_t y, int16_t index, int16_t total, uint16_t index_color, uint16_t total_color) {
  String S_index = num_to_str(index);
  String S_total = num_to_str(total);
  //0x39c7
  tft.setTextColor(RGBcolorBGR(convert, total_color), 0);
  tft.setCursor(x - (S_total.length() * 5 + S_total.length()), y);
  tft.printf("/%s", S_total);
  //0x6b4d
  tft.setTextColor(RGBcolorBGR(convert, index_color), 0);
  tft.fillRect((x - 5) - (S_total.length() * 5 + S_total.length()) - (S_index.length() * 5 + S_index.length()), (y), 6, 7, 0);

  tft.setCursor(x - (S_total.length() * 5 + S_total.length()) - (S_index.length() * 5 + S_index.length()), y);
  tft.print(index);
}
//=====================
// these 3 could just work in one function by using if
bool Album(bool display) {  //display artis
  static uint8_t artis_amount = *P_artis_amount;
  static int16_t index_album = 1;  // -30 to get it to sync with the perset index //at 131
  static int16_t index_img = 0;    // to enable the img to be process at the same time as the text is scrolling
  static int text_y = -20;
  static int pxstrlen2 = 0;
  static float scrolls[7];
  static int8_t img_type = 0;
  static bool push_img_now = false;

  static bool get_Artises = false;
  static bool scoll_end[7] = { 0, 0, 0, 0, 0, 0, 0 };
  static bool S_up = false;  // heh, sup
  static bool S_down = false;

  static int16_t w = 0;
  static int16_t trans = 0;
  bool pushed = false;

  tft.setTextColor(0x8c71, 0);
  tft.setTextSize(1);
  tft.setCursor(118, 2);
  tft.print("ARTISES");
  //tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
  //tft.print(index_album);
  //tft.setTextColor(0x8c71, 0);
  //tft.printf(" / %d   ", artis_amount);

  String name;

  display_index_for_menu(233, 2, index_album, artis_amount, 0xd508, 0x8c71);

  // display name and art cover
  // art cover use the same name as album
  // only display art when current song is not play
  // no inner menu
  // new menu type for album
  // read .abl file
  // add data to songs list array 100 songs at max
  //(could just use the already existed song list. searching that would be too long. and indexing sont work since it could change every update)
  //play within the album
  //change the loop() playlist to current album playlist
  // there maybe should be 2 buffer for album, one for playing, other for browsing

  //able to:
  // select a speciflic songs is the playlist
  // play the whole list, and loop
  tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0x6B4D));
  tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0x6B4D));

  if (display == false) {
    if (skipbutton() == true) {  //a song is selected
      uint8_t album_amount = Read_artis_albums(AlbumList, Artis_Folder_List.Artis_Folder_path[index_album - 1]);
      small_Album_menu.fillSprite(0);
      Serial.println(Artis_name_manip(Artis_Folder_List.Artis_Folder_path[index_album - 1]));
      Open_artis(album_amount, Artis_name_manip(Artis_Folder_List.Artis_Folder_path[index_album - 1]));

      *P_songs_menu_song_change_AL = true;  // for img display
      img_type = openingbmp120(Artis_Folder_List.Artis_Folder_path[index_img], 0, 0, 0);
      if (img_type == 1) {
        push_img_now = true;
      }
    }

    if (index_album != artis_amount) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        if (timeout(100) == true) {
          index_img++;
          S_down = true;
          tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (index_album != 1) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        if (timeout(100) == true) {
          index_img--;
          S_up = true;
          tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (S_up == true || S_down == true) {   // puting this here so the img dispaly can look more respondsive
      *P_songs_menu_song_change_AL = true;  // for img display
      img_type = openingbmp120(Artis_Folder_List.Artis_Folder_path[index_img], 0, 0, 0);
    }


    for (float q = 0; q < 20; q += 0.5) {
      if (S_up == true || S_down == true) {
        scrolls[3] = 0;
        scoll_end[3] = 1;
      }
      if (S_up == true) {
        w = q * 1;
        trans = q * 1;
      }
      if (S_down == true) {
        w = q * -1;
        trans = q * -1;
      }

      for (int i = 0; i < 7; i++) {  /// this one for when controlling this menu

        if (i + index_album - 4 < 0) {  // prevent it from reading out of bound
          name = " ";
        } else if (i + index_album - 4 > artis_amount - 1) {
          name = " ";
        } else {
          name = Artis_name_manip(Artis_Folder_List.Artis_Folder_path[i + index_album - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
        }
        // Serial.println(name);

        pxstrlen2 = 5 * name.length() + name.length() - 1;

        switch (i) {
          case 0:
            text_y = -20 + w;
            //Serial.println(w);
            small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;
          case 1:
            text_y = 0 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black
            }
            break;
          case 2:
            text_y = 20 * 1 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray
            }
            break;
          case 3:
            text_y = 20 * 2 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xFD85), 0);
            } else {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xFD85, 0xbdf7)), 0);
            }
            break;
          case 4:
            text_y = 20 * 3 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray

            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            }
            break;
          case 5:
            text_y = 20 * 4 + w;
            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black

            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            }

            break;
          case 6:
            text_y = 20 * 5 + w;

            small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;

          default:
            break;
        }

        small_Album_menu.drawFastHLine(0, text_y - 1, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y + 8, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y - 2, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y + 9, 101, 0);

        if (name.length() > 17) {
          if (i == 3) {  // restricted to the selected

            if (scrolls[3] <= 101 - pxstrlen2 - 10) {
              scoll_end[3] = true;
            }
            if ((scrolls[3] >= 0 + 10)) {
              scoll_end[3] = false;
            }

            if (scoll_end[3] == false) {
              scrolls[3] = (scrolls[i] - .25 - (1 - 17.0 / name.length()) / 2.5);  // scale by string lenght
            } else {
              scrolls[3] = (scrolls[i] + .4 + (1 - 17.0 / name.length()) / 2.0);

              // scrolls[i] = (scrolls[i] + speed + (1 - max length / name.length()) / scale); // higer scale is slower
            }
            small_Album_menu.drawFastVLine(scrolls[3] - 1, text_y, 7, 1);
            small_Album_menu.setCursor(scrolls[3], text_y);
          } else {
            small_Album_menu.drawFastVLine(0, text_y, 7, 1);
            small_Album_menu.setCursor(0, text_y);
          }
        } else {

          scrolls[i] = 0;
          scoll_end[i] = 1;
          small_Album_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
        }


        small_Album_menu.print(name);
      }

      small_Album_menu.pushSprite(126, 14);

      if (S_up == false && S_down == false) {
        break;
      }

      //delay(100);
    }


    if (S_up == true) {  // puting this here so the img dispaly can looke more respondsive
      index_album--;
    }
    if (S_down == true) {
      index_album++;
    }

    if (S_down == true || S_up == true || push_img_now == true) {
      if (img_type == 1) {
        art120_sprite_AL.fillSprite(0);
        //art120_sprite_AL.pushSprite(118, 118);
        // noartcover120();
        Preview_albums(Artis_Folder_List.Artis_Folder_path[index_img]);
        push_img_now = false;
      } else {
        while (1) {
          if (*P_art120done == true) {
            break;
          }
          vTaskDelay(pdMS_TO_TICKS(20));
        }
      }
    }

    if (*P_art120done == true) {
      if (img_type == 2) {
        tft.setRotation(2);
        art120_sprite_AL.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
        art120_sprite_AL.writedata(TFT_MAD_MY);
        art120_sprite_AL.pushSprite(118, 2);
        tft.setRotation(0);
      }
      *P_art120done = false;
      pushed = true;
    }




    //reset
    S_up = false;
    S_down = false;
    text_y = -20;
    w = 0;

    //*P_songs_menu_song_change = false;
    // *P_songs_menu_index = index_album;

  } else {
    img_type = openingbmp120(Artis_Folder_List.Artis_Folder_path[index_img], 0, 0, 0);

    for (int i = 0; i < 7; i++) {  /// this one for when controlling this menu
      //Serial.println("printing preview menu");

      if (i + index_album - 4 < 0) {  // prevent it from reading out of bound
        name = " ";
      } else if (i + index_album - 4 > artis_amount - 1) {
        name = " ";
      } else {
        name = Artis_name_manip(Artis_Folder_List.Artis_Folder_path[i + index_album - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
      }

      //; Artis_Folder_List.Artis_Folder_path[]
      // Serial.println(name);

      pxstrlen2 = 5 * name.length() + name.length() - 1;

      switch (i) {
        case 0:
          text_y = -20 + w;
          small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 1:
          text_y = 0 + w;
          small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 2:
          text_y = 20 * 1 + w;
          small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 3:
          text_y = 20 * 2 + w;
          small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 4:
          text_y = 20 * 3 + w;
          small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 5:
          text_y = 20 * 4 + w;
          small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 6:
          text_y = 20 * 5 + w;
          small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        default:
          break;
      }

      if (name.length() > 17) {
        small_Album_menu.setCursor(0, text_y);
      } else {
        small_Album_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
      }
      small_Album_menu.print(name);
      scrolls[i] = 0;
      scoll_end[i] = 0;
    }
    small_Album_menu.pushSprite(126, 14);

    if (*P_art120done == true) {
      if (img_type == 2) {
        tft.setRotation(2);
        art120_sprite_AL.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
        art120_sprite_AL.writedata(TFT_MAD_MY);
        art120_sprite_AL.pushSprite(118, 2);
        tft.setRotation(0);
      }
      *P_art120done = false;
      pushed = true;
    }
    if (img_type == 1) {
      art120_sprite_AL.fillSprite(0);
      //art120_sprite_AL.pushSprite(118, 118);
      // noartcover120();
      Preview_albums(Artis_Folder_List.Artis_Folder_path[index_img]);
      pushed = true;
    }
  }
  return pushed;
}
void Open_artis(uint8_t album_amount, String artis_name) {  // display album
  uint8_t index_album = 1;                                  // -30 to get it to sync with the perset index //at 131
  int16_t index_img = 0;                                    // to enable the img to be process at the same time as the text is scrolling
  static int text_y = -20;
  int8_t img_type = 0;
  bool img_load = true;  // only used for when entering
  //static bool push_img_now = false;

  static int pxstrlen2 = 0;
  float scrolls[7];
  bool scoll_end[7] = { 0, 0, 0, 0, 0, 0, 0 };

  static bool S_up = false;  // heh, sup
  static bool S_down = false;

  static int16_t w = 0;
  static int16_t trans = 0;
  String name;
  //String Album_name_Check = "EPs";
  //String Album_name_Check = artis_name;



  while (1) {

    tft.setTextColor(0x8c71, 0);
    tft.setTextSize(1);
    tft.setCursor(118, 2);
    tft.println("ALBUMS ");
    //tft.setCursor(118 + 48, 2);
    //tft.setTextColor(RGBcolorBGR(convert, 0xd508), 0);
    //tft.print(index_album);
    //tft.setTextColor(0x8c71, 0);
    //tft.print(" / ");
    //tft.print(album_amount);
    //tft.println("    ");
    tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0x6B4D));
    tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0x6B4D));

    display_index_for_menu(233, 2, index_album, album_amount, 0xd508, 0x8c71);
    if (skipbutton() == true) {
      uint8_t songs_amount = Read_artis_ablum_songs(AlbumB, AlbumList.Album_name[index_album - 1]);
      small_Album_menu.fillSprite(0);
      //small_Album_menu.pushSprite(126, 14);
      if (img_type == 1) {
        art120_sprite_AL.fillSprite(0);
        art120_sprite_AL.pushSprite(118, 118);
      }
      *P_albums_INDEX = index_album - 1;
      Open_album(songs_amount, Album_name_manip(AlbumList.Album_name[index_album - 1], artis_name), artis_name);

      *P_songs_menu_song_change_AL = true;  // for img display
      img_type = openingbmp120(AlbumList.Album_name[index_img], 0, 0, 0);
      if (img_type == 1) {
        img_load = true;
      }
      // tft.setTextColor(0xffff, 0);
      // tft.setTextSize(1);
      // tft.setCursor(118, 2);
      // tft.println("ALBUMS     ");
    }

    if (index_album != album_amount) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        if (timeout(100) == true) {
          index_img++;
          S_down = true;
          *P_songs_menu_song_change_AL = true;  // for img display
          tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (index_album != 1) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        if (timeout(100) == true) {
          *P_songs_menu_song_change_AL = true;
          index_img--;
          S_up = true;
          tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }
    if (S_up == true || S_down == true || img_load == true) {  // puting this here so the img dispaly can look more respondsive
      *P_songs_menu_song_change_AL = true;
      //Serial.println(AlbumList.Album_name[index_img]);
      img_type = openingbmp120(AlbumList.Album_name[index_img], 0, 0, 0);
    }

    for (float q = 0; q < 20; q += 0.5) {
      if (S_up == true || S_down == true) {
        scrolls[3] = 0;
        scoll_end[3] = 1;
      }
      if (S_up == true) {
        w = q * 1;
        trans = q * 1;
      }
      if (S_down == true) {
        w = q * -1;
        trans = q * -1;
      }

      for (int i = 0; i < 7; i++) {  /// this one for when controlling this menu

        if (i + index_album - 4 < 0) {  // prevent it from reading out of bound
          name = " ";
        } else if (i + index_album - 4 > album_amount - 1) {
          name = " ";
        } else {
          name = Album_name_manip(AlbumList.Album_name[i + index_album - 4], artis_name);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
        }

        pxstrlen2 = 5 * name.length() + name.length() - 1;

        switch (i) {
          case 0:
            text_y = -20 + w;
            //Serial.println(w);
            small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;
          case 1:
            text_y = 0 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black
            }
            break;
          case 2:
            text_y = 20 * 1 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray
            }
            break;
          case 3:
            text_y = 20 * 2 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xFD85), 0);
            } else {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xFD85, 0xbdf7)), 0);
            }
            break;
          case 4:
            text_y = 20 * 3 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray

            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            }
            break;
          case 5:
            text_y = 20 * 4 + w;
            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black

            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            }

            break;
          case 6:
            text_y = 20 * 5 + w;

            small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;

          default:
            break;
        }

        small_Album_menu.drawFastHLine(0, text_y - 1, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y + 8, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y - 2, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y + 9, 101, 0);

        if (name.length() > 17) {
          if (i == 3) {  // restricted to the selected

            if (scrolls[3] <= 101 - pxstrlen2 - 10) {
              scoll_end[3] = true;
            }
            if ((scrolls[3] >= 0 + 10)) {
              scoll_end[3] = false;
            }

            if (scoll_end[3] == false) {
              scrolls[3] = (scrolls[i] - .25 - (1 - 17.0 / name.length()) / 2.5);  // scale by string lenght
            } else {
              scrolls[3] = (scrolls[i] + .4 + (1 - 17.0 / name.length()) / 2.0);

              // scrolls[i] = (scrolls[i] + speed + (1 - max length / name.length()) / scale); // higer scale is slower
            }
            small_Album_menu.drawFastVLine(scrolls[3] - 1, text_y, 7, 1);
            small_Album_menu.setCursor(scrolls[3], text_y);
          } else {
            small_Album_menu.drawFastVLine(0, text_y, 7, 1);
            small_Album_menu.setCursor(0, text_y);
          }
        } else {

          scrolls[i] = 0;
          scoll_end[i] = 1;
          small_Album_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
        }
        small_Album_menu.print(name);
      }

      small_Album_menu.pushSprite(126, 14);

      if (S_up == false && S_down == false) {
        break;
      }
    }

    if (S_up == true) {  // puting this here so the img dispaly can looke more respondsive
      index_album--;
    }
    if (S_down == true) {
      index_album++;
    }

    if (*P_art120done == true) {
      if (img_type == 2) {
        tft.setRotation(2);
        art120_sprite_AL.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
        art120_sprite_AL.writedata(TFT_MAD_MY);
        art120_sprite_AL.pushSprite(118, 2);
        tft.setRotation(0);
      }
      *P_art120done = false;
    }

    if (S_down == true || S_up == true || img_load == true) {
      if (img_type == 1) {
        art120_sprite_AL.fillSprite(0);
        // art120_sprite_AL.pushSprite(118, 118);
        //noartcover120();
        Preview_songs(AlbumList.Album_name[index_album - 1]);
      }
    }

    //reset
    S_up = false;
    S_down = false;
    img_load = false;
    text_y = -20;
    w = 0;

    *P_songs_menu_song_change = false;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (backbutton() == HIGH || ScreenMode(10) == 1) {
      small_Album_menu.fillSprite(0);
      break;
    }
  }
}
void Open_album(uint8_t song_amount, String album_name, String artis_name) {  //display album song
  int16_t index_song = 1;                                                     // -30 to get it to sync with the perset index //at 131
  // int16_t index_img = 0;   // to enable the img to be process at the same time as the text is scrolling
  static int text_y = -20;

  static int pxstrlen2 = 0;
  float scrolls[7];
  bool scoll_end[7] = { 0, 0, 0, 0, 0, 0, 0 };

  bool S_up = false;  // heh, sup
  bool S_down = false;

  int16_t w = 0;
  int16_t trans = 0;
  String name;
  String Album_name_Check = "EPs";

  while (1) {
    tft.setTextColor(0x8c71, 0);
    tft.setTextSize(1);
    tft.setCursor(118, 2);
    tft.println("SONGS  ");

    tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0x6B4D));
    tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0x6B4D));

    display_index_for_menu(233, 2, index_song, song_amount, 0xd508, 0x8c71);

    if (skipbutton() == true) {  //a song is selected

      if (album_name == Album_name_Check || album_name == artis_name) {
        Serial.println("general art cover for this album");
        *P_Album_art_general = true;
      } else {
        *P_Album_art_general = false;
      }

      if (album_name == Album_name_Check) {
        *P_ALBUM_name = artis_name + " >> " + album_name;
      } else {
        *P_ALBUM_name = album_name;
      }

      *P_album_SELECTED_PLAYING = true;
      *P_album_song_SELECTED = true;

      *P_playing_list_song_INDEX = index_song - 1;
      *P_playing_list_song_AMOUNT = song_amount;
    }

    if (index_song != song_amount) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        if (timeout(100) == true) {
          S_down = true;
          tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (index_song != 1) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        if (timeout(100) == true) {
          S_up = true;
          tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    for (float q = 0; q < 20; q += 0.5) {
      if (S_up == true || S_down == true) {
        scrolls[3] = 0;
        scoll_end[3] = 1;
      }

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
        } else if (i + index_song - 4 > song_amount - 1) {
          name = " ";
        } else {
          name = manip_song_name(AlbumB.Song_name[i + index_song - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
        }

        pxstrlen2 = 5 * name.length() + name.length() - 1;

        switch (i) {
          case 0:
            text_y = -20 + w;
            small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;
          case 1:
            text_y = 0 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black
            }
            break;
          case 2:
            text_y = 20 * 1 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray
            }
            break;
          case 3:
            text_y = 20 * 2 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xFD85), 0);
            } else {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xFD85, 0xbdf7)), 0);
            }
            break;
          case 4:
            text_y = 20 * 3 + w;

            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray

            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            }
            break;
          case 5:
            text_y = 20 * 4 + w;
            if (S_up == false && S_down == false) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black

            } else if (S_down == true) {
              small_Album_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            }

            break;
          case 6:
            text_y = 20 * 5 + w;

            small_Album_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;

          default:
            break;
        }

        small_Album_menu.drawFastHLine(0, text_y - 1, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y + 8, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y - 2, 101, 0);
        small_Album_menu.drawFastHLine(0, text_y + 9, 101, 0);

        if (name.length() > 17) {
          if (i == 3) {  // restricted to the selected

            if (scrolls[3] <= 101 - pxstrlen2 - 10) {
              scoll_end[3] = true;
            }
            if ((scrolls[3] >= 0 + 10)) {
              scoll_end[3] = false;
            }

            if (scoll_end[3] == false) {
              scrolls[3] = (scrolls[i] - .25 - (1 - 17.0 / name.length()) / 2.5);  // scale by string lenght
            } else {
              scrolls[3] = (scrolls[i] + .4 + (1 - 17.0 / name.length()) / 2.0);

              // scrolls[i] = (scrolls[i] + speed + (1 - max length / name.length()) / scale); // higer scale is slower
            }
            small_Album_menu.drawFastVLine(scrolls[3] - 1, text_y, 7, 1);
            small_Album_menu.setCursor(scrolls[3], text_y);
          } else {
            small_Album_menu.drawFastVLine(0, text_y, 7, 1);
            small_Album_menu.setCursor(0, text_y);
          }
        } else {

          scrolls[i] = 0;
          scoll_end[i] = 1;
          small_Album_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
        }

        small_Album_menu.print(name);
      }

      small_Album_menu.pushSprite(126, 14);

      if (S_up == false && S_down == false) {
        break;
      }
    }

    if (S_up == true) {  // puting this here so the img dispaly can looke more respondsive
      index_song--;
    }
    if (S_down == true) {
      index_song++;
    }
    //reset
    S_up = false;
    S_down = false;
    text_y = -20;
    w = 0;
    vTaskDelay(pdMS_TO_TICKS(20));
    if (backbutton() == HIGH || ScreenMode(10) == 1) {
      small_Album_menu.fillSprite(0);
      //small_Album_menu.pushSprite(126, 14);
      break;
    }
  }
}
void Preview_albums(String artis_name_folder_path) {
  static int8_t text_spacing = 20;
  static int8_t set_text_list_x = 5;
  static int8_t set_text_list_y = 7 + 5;
  static uint8_t dynamic_list_y;

  uint8_t total_album = 0;
  static uint16_t text1_color = 0xc658;
  static uint16_t text2_color = 0x8c71;

  String Album_name;
  uint8_t length_sub = artis_name_folder_path.length() + 1;
  art120_sprite_AL.setTextSize(1);

  artis_name_folder_path = artis_name_folder_path + "/Album_list.txt";

  File Albums_in_artis = SD_MMC.open(artis_name_folder_path, FILE_READ);
  total_album = Albums_in_artis.read();
  dynamic_list_y = (6 - total_album) * 10.6;  //10.6 becus 53 = ( 6 - 1 ) * x

  for (uint8_t i = 0; i < total_album; i++) {
    Album_name = Albums_in_artis.readStringUntil('\n');
    Album_name = Album_name.substring(length_sub, Album_name.length() - 4);

    if (i % 2 == 0) {
      art120_sprite_AL.setTextColor(text2_color);
    } else {
      art120_sprite_AL.setTextColor(text1_color);
    }

    uint8_t pxstrlen = 5 * Album_name.length() + Album_name.length() - 1;

    if (Album_name.length() >= 20) {
      art120_sprite_AL.setCursor(0, dynamic_list_y + set_text_list_y + text_spacing * i);
    } else {
      art120_sprite_AL.setCursor(60 - (pxstrlen / 2.0), dynamic_list_y + set_text_list_y + text_spacing * i);
    }

    art120_sprite_AL.println(Album_name);
  }

  art120_sprite_AL.setTextColor(RGBcolorBGR(convert, text1_color));
  art120_sprite_AL.setCursor(1, 0);
  art120_sprite_AL.print("Albums: ");
  art120_sprite_AL.setTextColor(RGBcolorBGR(convert, 0xFD85));
  art120_sprite_AL.print(total_album);
  art120_sprite_AL.pushSprite(118, 118);

  Albums_in_artis.close();
}
void Preview_songs(String album_path) {

  static int8_t text_spacing = 20;
  static int8_t set_text_list_x = 5;
  static int8_t set_text_list_y = 7 + 5;
  static uint8_t dynamic_list_y;

  uint8_t total_album = 0;
  static uint16_t text1_color = 0xc658;
  static uint16_t text2_color = 0x8c71;

  String Album_name;
  uint8_t length_sub = album_path.length() + 1;
  art120_sprite_AL.setTextSize(1);
  Serial.println(album_path);
  String Song_name[6];

  File Albums_in_artis = SD_MMC.open(album_path, FILE_READ);



  for (uint8_t i = 0; i < Songs_per_Album_limit; i++) {
    Album_name = Albums_in_artis.readStringUntil('\n');
    if (Album_name.charAt(0) != '/') {
      break;
    }
    if (total_album < 6) {
      Song_name[total_album] = manip_song_name(Album_name);
    }
    total_album++;
  }
  if (total_album < 6) {
    dynamic_list_y = (6 - total_album) * 10.6;  //10.6 becus 53 = ( 6 - 1 ) * x
  } else {
    dynamic_list_y = 0;
  }



  for (uint8_t o = 0; o < 6; o++) {
    if (o % 2 == 0) {
      art120_sprite_AL.setTextColor(text2_color);
    } else {
      art120_sprite_AL.setTextColor(text1_color);
    }
    uint8_t pxstrlen = 5 * Song_name[o].length() + Song_name[o].length() - 1;
    if (Song_name[o].length() >= 20) {
      art120_sprite_AL.setCursor(0, dynamic_list_y + set_text_list_y + text_spacing * o);
    } else {
      art120_sprite_AL.setCursor(60 - (pxstrlen / 2.0), dynamic_list_y + set_text_list_y + text_spacing * o);
    }
    art120_sprite_AL.println(Song_name[o]);
  }

  art120_sprite_AL.setTextColor(RGBcolorBGR(convert, text1_color));
  art120_sprite_AL.setCursor(1, 0);
  art120_sprite_AL.print("Songs: ");
  art120_sprite_AL.setTextColor(RGBcolorBGR(convert, 0xFD85));
  art120_sprite_AL.print(total_album);
  art120_sprite_AL.pushSprite(118, 118);

  Albums_in_artis.close();
}

String Artis_name_manip(String Artis_name) {
  // /Album/artisname
  Artis_name = Artis_name.substring(7, Artis_name.length());
  return Artis_name;
}
String Album_name_manip(String Album_name, String artis_name_sub) {
  // /Album/artisname/albumname.alb
  uint8_t skiping_index = artis_name_sub.length();
  Album_name = Album_name.substring(skiping_index + 8, Album_name.length() - 4);

  return Album_name;
}
uint8_t Read_artis_albums(AlbumList_Struct& ALS, String Artis_path) {  //geting album file from album file save file
  uint8_t Album_amount = 0;
  Artis_path = Artis_path + "/Album_list.txt";
  // Serial.println(Artis_path);
  File Artis_album_save_file = SD_MMC.open(Artis_path, FILE_READ);
  Album_amount = Artis_album_save_file.read();
  // Serial.println(Album_amount);
  for (uint8_t i = 0; i < Album_amount; i++) {
    String Album_file_path = Artis_album_save_file.readStringUntil('\n');
    ALS.Album_name[i] = Album_file_path;
    // Serial.println(ALS.Album_name[i]);
  }
  return Album_amount;
}
uint8_t Read_artis_ablum_songs(Album_Struct& AS, String Album_name) {
  //Album_Struct& AS selecting browse or play struct
  // take in album name as int path to write that file content to the selected struct
  //Serial.println(Album_name);
  File Songs_in_Album = SD_MMC.open(Album_name, FILE_READ);
  String song_name;
  uint8_t count_song = 0;  // have to count who know how mmany the user put tin

  Serial.println("READING SONGS IN ALBUM");

  while (Songs_in_Album) {
    song_name = Songs_in_Album.readStringUntil('\n');
    if (song_name.charAt(0) != '/') {
      //  count_song - 1;
      break;
    }

    AS.Song_name[count_song] = song_name;
    //  Serial.println(AS.Song_name[count_song]);
    count_song++;

    if (count_song >= Songs_per_Album_limit) {
      break;
    }
  }
  //Serial.println(count_song);
  return count_song;
}

//aka update
void Artists_Album_read_and_save(Artis_Folder_Struct& AFS, int16_t x, int16_t y) {
  //read Album dir, list out all artis dir name and save them into Artis_list.txt
  //read artis dir and save all album file name into "artis folder name"_Album_list.txt
  uint8_t artis_dir_count = 0;
  uint8_t artis_album_count = 0;
  uint16_t total_album_count = 0;

  if (!SD_MMC.exists("/Album")) {
    SD_MMC.mkdir("/Album");
  } else {
    Serial.println("Album Folder exist");
  }

  String extension_checking = ".alb";

  File total_album_save = SD_MMC.open("/.SDinfo.txt", "r+");

  File Album = SD_MMC.open("/Album");
  File Artis_DIR = Album.openNextFile();

  File Artis_list = SD_MMC.open("/Album/Artis_list.txt", FILE_WRITE);
  Artis_list.write(artis_dir_count);
  while (Artis_DIR) {
    if (Artis_DIR.isDirectory()) {
      //=============
      const char* c_Artis_folder_name = Artis_DIR.name();
      String Artis_folder_name = c_Artis_folder_name;
      AFS.Artis_Folder_path[artis_dir_count] = "/Album/" + Artis_folder_name;  //save path to struct
      Artis_list.printf("/Album/%s\n", c_Artis_folder_name);                   //write to file
      Serial.printf("/Album/%s\n", c_Artis_folder_name);
      //=================
      //==================
      File Artis_Album_folder = SD_MMC.open(AFS.Artis_Folder_path[artis_dir_count]);  // open current folder
      File Artis_Album = Artis_Album_folder.openNextFile();                           // open the file in that folder

      String Artis_Album_list_save_file_name = AFS.Artis_Folder_path[artis_dir_count] + "/" + "Album_list.txt";  // path to the save list
      File Artis_Album_list_save_file = SD_MMC.open(Artis_Album_list_save_file_name, FILE_WRITE);                // /Album/Artis folder name/artis folder name_Album_list.txt

      Serial.println(Artis_Album_list_save_file_name);
      Artis_Album_list_save_file.write(artis_album_count);  // write 1 byte first
      while (Artis_Album) {
        const char* Album_cname = Artis_Album.name();
        String S_Album_name = Album_cname;

        if (S_Album_name.substring(S_Album_name.length() - 4, S_Album_name.length()) == extension_checking) {
          Artis_Album_list_save_file.printf("/Album/%s/%s\n", c_Artis_folder_name, Album_cname);  // save album path

          Serial.printf("/Album/%s/%s\n", c_Artis_folder_name, Album_cname);  // /Album/Artis folder name/album name.alb

          artis_album_count++;
          total_album_count++;

          // tft.setTextColor(RGBcolorBGR(convert, highlight));
          tft.setCursor(x, y);
          tft.println(total_album_count);
        }
        Artis_Album.close();
        Artis_Album = Artis_Album_folder.openNextFile();
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      Serial.println(artis_album_count);

      Artis_Album.close();
      Artis_Album_list_save_file.seek(0);
      Artis_Album_list_save_file.write(artis_album_count);
      Artis_Album_list_save_file.close();
      //=====================
      artis_album_count = 0;
      artis_dir_count++;
      Serial.println(" ");
    }
    Artis_DIR.close();
    Artis_DIR = Album.openNextFile();
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  Artis_list.seek(0);
  Artis_list.write(artis_dir_count);
  Serial.println(artis_dir_count);

  *P_total_albums = total_album_count;
  Serial.print("total_album");
  Serial.println(total_album_count);

  total_album_save.write((uint8_t*)&total_album_count, 2);

  total_album_save.close();
  Artis_list.close();
  Album.close();
  Artis_DIR.close();
}
//read from save file
void Artists_Album_read() {
  //read Album dir, list out all artis dir name and save them into Artis_list.txt
  //read artis dir and save all album file name into "artis folder name"_Album_list.txt

  uint8_t artis_dir_count = 0;
  uint16_t albums;

  if (!SD_MMC.exists("/Album")) {
    SD_MMC.mkdir("/Album");
  } else {
    Serial.println("Album Folder exist");
  }

  String extension_checking = ".alb";

  File total_album = SD_MMC.open("/.SDinfo.txt", FILE_READ);
  total_album.read((uint8_t*)&albums, 2);
  *P_total_albums = albums;

  setting_preview_info.albums = albums;

  File Artis_list = SD_MMC.open("/Album/Artis_list.txt", FILE_READ);
  *P_artis_amount = artis_dir_count = Artis_list.read();

  if (artis_dir_count >= 100) {
    artis_dir_count = 100;
  }

  for (uint8_t i = 0; i < artis_dir_count; i++) {
    String name = Artis_list.readStringUntil('\n');
    Artis_Folder_List.Artis_Folder_path[i] = name;
    Serial.println(name);
  }

  total_album.close();
  Artis_list.close();
  // Album.close();
  //Artis_DIR.close();
}
//=====================

//===============
bool PlayList(bool display) {
  uint8_t playlists = *P_playlist_amount;
  // static  song_amount = 0;
  static bool get_playlist_amount = false;
  static int16_t index_album = 1;  // -30 to get it to sync with the perset index //at 131
  static int16_t index_img = 0;    // to enable the img to be process at the same time as the text is scrolling
  static int text_y = -20;

  static int pxstrlen2 = 0;
  static float scrolls[7];
  static bool scoll_end[7] = { 0, 0, 0, 0, 0, 0, 0 };

  static bool S_up = false;  // heh, sup
  static bool S_down = false;
  static int8_t img_type = 0;

  static int16_t w = 0;
  static int16_t trans = 0;
  bool pushed = false;

  static bool view_push_once = false;

  String name;

  tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0x6B4D));
  tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0x6B4D));

  display_index_for_menu(233, 2, index_album, playlists, 0xd508, 0x8c71);

  //tft.setTextColor(0x8c71, 0);
  //tft.setTextSize(1);
  //tft.setCursor(118, 2);
  //tft.print("PLAYLISTS");

  if (display == false) {
    if (skipbutton() == true) {  //a song is selected
      uint8_t song_amount = read_Playlist_Song_list(PlayListB, PlayList_list.Playlist_name[index_album - 1]);
      *P_albums_INDEX = index_album - 1;
      small_playlist_menu.fillSprite(0);
      // small_playlist_menu.pushSprite(126, 14);
      Open_PlayList(song_amount);
    }


    if (index_album != playlists) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        if (timeout(100) == true) {
          index_img++;
          S_down = true;
          *P_songs_menu_song_change_PL = true;  // for img display
          tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (index_album != 1) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        if (timeout(100) == true) {
          *P_songs_menu_song_change_PL = true;
          index_img--;
          S_up = true;
          tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (S_up == true) {  // puting this here so the img dispaly can look more respondsive
      img_type = openingbmp120(PlayList_list.Playlist_name[index_img], 2, 0, 0);
    }

    if (S_down == true) {
      img_type = openingbmp120(PlayList_list.Playlist_name[index_img], 2, 0, 0);
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

        if (i + index_album - 4 < 0) {  // prevent it from reading out of bound
          name = " ";
        } else if (i + index_album - 4 > 4 - 1) {
          name = " ";
        } else {
          name = Playlist_name_manip(PlayList_list.Playlist_name[i + index_album - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
        }
        // Serial.println(name);

        pxstrlen2 = 5 * name.length() + name.length() - 1;

        switch (i) {
          case 0:
            text_y = -20 + w;
            //Serial.println(w);
            small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;
          case 1:
            text_y = 0 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black
            }
            break;
          case 2:
            text_y = 20 * 1 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray
            }
            break;
          case 3:
            text_y = 20 * 2 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xFD85), 0);
            } else {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xFD85, 0xbdf7)), 0);
            }
            break;
          case 4:
            text_y = 20 * 3 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray

            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            }
            break;
          case 5:
            text_y = 20 * 4 + w;
            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black

            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            }

            break;
          case 6:
            text_y = 20 * 5 + w;

            small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;

          default:
            break;
        }

        small_playlist_menu.drawFastHLine(0, text_y - 1, 101, 0);
        small_playlist_menu.drawFastHLine(0, text_y + 8, 101, 0);
        small_playlist_menu.drawFastHLine(0, text_y - 2, 101, 0);
        small_playlist_menu.drawFastHLine(0, text_y + 9, 101, 0);


        if (name.length() > 17) {

          if (scrolls[i] <= 101 - pxstrlen2 - 10) {
            scoll_end[i] = true;
          }
          if ((scrolls[i] >= 0 + 10)) {
            scoll_end[i] = false;
          }

          if (i == 3) {  // restricted to the selected
            if (scoll_end[i] == false) {
              scrolls[i] = (scrolls[i] - .25 - (1 - 17.0 / name.length()) / 2.5);  // scale by string lenght
            } else {
              scrolls[i] = (scrolls[i] + .4 + (1 - 17.0 / name.length()) / 2.0);

              // scrolls[i] = (scrolls[i] + speed + (1 - max length / name.length()) / scale); // higer scale is slower
            }
          }

          small_playlist_menu.drawFastVLine(scrolls[i] - 1, text_y, 7, 1);
          small_playlist_menu.setCursor(scrolls[i], text_y);

        } else {

          scrolls[i] = 0;
          small_playlist_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
        }

        small_playlist_menu.print(name);
      }

      small_playlist_menu.pushSprite(126, 14);

      if (S_up == false && S_down == false) {
        break;
      }

      //delay(100);
    }


    if (S_up == true) {  // puting this here so the img dispaly can looke more respondsive
      index_album--;
    }
    if (S_down == true) {
      index_album++;
    }


    if (*P_art120done == true) {
      if (img_type == 2) {
        tft.setRotation(2);
        art120_sprite_PL.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
        art120_sprite_PL.writedata(TFT_MAD_MY);
        art120_sprite_PL.pushSprite(118, 2);
        tft.setRotation(0);
      }
      *P_art120done = false;
      //pushed = true;
    }

    if (S_down == true || S_up == true) {
      if (img_type == 1) {
        art120_sprite_PL.fillSprite(0);
        art120_sprite_PL.pushSprite(118, 118);
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


    //*P_songs_menu_song_change = false;
    // *P_songs_menu_index = index_album;
  } else {
    //*P_songs_menu_song_change = true;
    // Serial.println("here1");
    //img_type = openingbmp120(AlbumList.Album_name[index_img], 0);
    img_type = openingbmp120(PlayList_list.Playlist_name[index_img], 2, 0, 0);

    for (int i = 0; i < 7; i++) {  /// this one for when controlling this menu

      if (i + index_album - 4 < 0) {  // prevent it from reading out of bound
        name = " ";
      } else if (i + index_album - 4 > 4 - 1) {
        name = " ";
      } else {
        name = Playlist_name_manip(PlayList_list.Playlist_name[i + index_album - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
      }
      // Serial.println(name);

      pxstrlen2 = 5 * name.length() + name.length() - 1;

      switch (i) {
        case 0:
          text_y = -20 + w;
          small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 1:
          text_y = 0 + w;
          small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 2:
          text_y = 20 * 1 + w;
          small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 3:
          text_y = 20 * 2 + w;
          small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 4:
          text_y = 20 * 3 + w;
          small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
          break;
        case 5:
          text_y = 20 * 4 + w;
          small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        case 6:
          text_y = 20 * 5 + w;
          small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
          break;
        default:
          break;
      }

      if (name.length() > 17) {
        small_playlist_menu.setCursor(0, text_y);
      } else {
        small_playlist_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
      }
      scrolls[i] = 0;
      scoll_end[i] = 0;
      small_playlist_menu.print(name);
    }
    small_playlist_menu.pushSprite(126, 14);

    if (*P_art120done == true) {
      if (img_type == 2) {
        tft.setRotation(2);
        art120_sprite_PL.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
        art120_sprite_PL.writedata(TFT_MAD_MY);
        art120_sprite_PL.pushSprite(118, 2);
        tft.setRotation(0);
      }
      *P_art120done = false;
      pushed = true;
    }

    //if (S_down == true || S_up == true) {
    if (img_type == 1) {
      art120_sprite_PL.fillSprite(0);
      art120_sprite_PL.pushSprite(118, 118);
      noartcover120();
      pushed = true;
    }
    //}
  }
  return pushed;
}
void Open_PlayList(uint8_t song_amount) {
  int16_t index_song = 1;  // -30 to get it to sync with the perset index //at 131
  static int text_y = -20;

  static int pxstrlen2 = 0;
  static float scrolls[7];
  static bool scoll_end[7] = { 0, 0, 0, 0, 0, 0, 0 };

  static bool S_up = false;  // heh, sup
  static bool S_down = false;

  static int16_t w = 0;
  static int16_t trans = 0;
  String name;

  while (1) {
    tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0x6B4D));
    tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0x6B4D));

    display_index_for_menu(233, 2, index_song, song_amount, 0xd508, 0x8c71);

    if (skipbutton() == true) {  //a song is selected
      *P_album_SELECTED_PLAYING = true;
      *P_playlist_song_SELECTED = true;

      *P_playing_list_song_INDEX = index_song - 1;
      *P_playing_list_song_AMOUNT = song_amount;
    }

    if (index_song != song_amount) {  // for when at the end of the list
      if (digitalRead(DOWN_pin) == HIGH) {
        if (timeout(100) == true) {
          //index_img++;
          S_down = true;
          *P_songs_menu_song_change_AL = true;  // for img display
          tft.drawBitmap(171, 109 - 2, menu_down, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
    }

    if (index_song != 1) {  // for when at the top of the list
      if (digitalRead(UP_pin) == HIGH) {
        if (timeout(100) == true) {
          *P_songs_menu_song_change_AL = true;
          S_up = true;
          tft.drawBitmap(171, 3 + 2, menu_up, 13, 3, RGBcolorBGR(convert, 0xFD85));
        }
      }
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
      for (int i = 0; i < 7; i++) {    /// this one for when controlling this menu
        if (i + index_song - 4 < 0) {  // prevent it from reading out of bound
          name = " ";
        } else if (i + index_song - 4 > song_amount - 1) {
          name = " ";
        } else {
          name = manip_song_name(PlayListB.Song_name[i + index_song - 4]);  //at index, ex: at 131 - 4 + i  so it start at above the index// there indexsong is the middle song
        }

        pxstrlen2 = 5 * name.length() + name.length() - 1;

        switch (i) {
          case 0:
            text_y = -20 + w;
            //Serial.println(w);
            small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;
          case 1:
            text_y = 0 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black
            }
            break;
          case 2:
            text_y = 20 * 1 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray
            }
            break;
          case 3:
            text_y = 20 * 2 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xFD85), 0);
            } else {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xFD85, 0xbdf7)), 0);
            }
            break;
          case 4:
            text_y = 20 * 3 + w;

            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0xBDF7), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0x6B4D)), 0);  // gray to dark gray

            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0xBDF7, 0xFD85)), 0);  //  gray to gold
            }
            break;
          case 5:
            text_y = 20 * 4 + w;
            if (S_up == false && S_down == false) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            } else if (S_up == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0)), 0);  //  dark gray to black

            } else if (S_down == true) {
              small_playlist_menu.setTextColor(RGBcolorBGR(convert, colortrans(trans, 20, 0x6B4D, 0xBDF7)), 0);  //dark gray to gray
            }

            break;
          case 6:
            text_y = 20 * 5 + w;

            small_playlist_menu.setTextColor(RGBcolorBGR(convert, 0x6B4D), 0);
            break;

          default:
            break;
        }

        small_playlist_menu.drawFastHLine(0, text_y - 1, 101, 0);
        small_playlist_menu.drawFastHLine(0, text_y + 8, 101, 0);
        small_playlist_menu.drawFastHLine(0, text_y - 2, 101, 0);
        small_playlist_menu.drawFastHLine(0, text_y + 9, 101, 0);


        if (name.length() > 17) {

          if (scrolls[i] <= 101 - pxstrlen2 - 10) {
            scoll_end[i] = true;
          }
          if ((scrolls[i] >= 0 + 10)) {
            scoll_end[i] = false;
          }

          if (i == 3) {  // restricted to the selected
            if (scoll_end[i] == false) {
              scrolls[i] = (scrolls[i] - .25 - (1 - 17.0 / name.length()) / 2.5);  // scale by string lenght
            } else {
              scrolls[i] = (scrolls[i] + .4 + (1 - 17.0 / name.length()) / 2.0);

              // scrolls[i] = (scrolls[i] + speed + (1 - max length / name.length()) / scale); // higer scale is slower
            }
          }

          small_playlist_menu.drawFastVLine(scrolls[i] - 1, text_y, 7, 1);
          small_playlist_menu.setCursor(scrolls[i], text_y);

        } else {

          scrolls[i] = 0;
          small_playlist_menu.setCursor(51 - (pxstrlen2 / 2.0), text_y);
        }

        small_playlist_menu.print(name);
      }

      small_playlist_menu.pushSprite(126, 14);

      if (S_up == false && S_down == false) {
        break;
      }
    }


    if (S_up == true) {  // puting this here so the img dispaly can looke more respondsive
      index_song--;
    }
    if (S_down == true) {
      index_song++;
    }


    //reset
    S_up = false;
    S_down = false;
    text_y = -20;
    w = 0;

    vTaskDelay(pdMS_TO_TICKS(20));
    if (backbutton() == HIGH || ScreenMode(10) == 1) {
      small_playlist_menu.fillSprite(0);
      //  small_playlist_menu.pushSprite(126, 14);
      break;
    }
  }
}
String Playlist_name_manip(String Album_name) {
  Album_name = Album_name.substring(10, Album_name.length() - 4);
  // Album_name = Album_name.substring(Album_name.length() - 4, Album_name.length());
  return Album_name;
}
//save to save file and to struct// aka update
void save_to_Playlist_list(PlayList_list_Struct& PLS, int16_t x, int16_t y) {
  if (!SD_MMC.exists("/Playlist")) {
    SD_MMC.mkdir("/Playlist");
  }

  File total_playlist_save = SD_MMC.open("/.SDinfo.txt", "r+");

  File PlaylistFolder = SD_MMC.open("/Playlist");
  File Playlist = PlaylistFolder.openNextFile();
  File save_to_list = SD_MMC.open("/Playlist/Playlist_list.txt", FILE_WRITE);

  String extension = ".pls";
  uint8_t Playlist_count = 0;

  save_to_list.write(Playlist_count);  //set 1byte offset for later write

  while (Playlist) {
    const char* cpls_name = Playlist.name();
    String name = cpls_name;
    String checking = cpls_name;

    if (checking.substring(checking.length() - 4, checking.length()) == extension) {
      PLS.Playlist_name[Playlist_count] = "/Playlist/" + name;  //save name to struct
      save_to_list.printf("/Playlist/%s\n", cpls_name);         // save name to file

      Playlist_count++;

      // tft.setTextColor(RGBcolorBGR(convert, highlight));
      tft.setCursor(x, y);
      tft.println(Playlist_count);
    }

    if (Playlist_count == Playlists_limit) {
      Serial.println("alright too many, limit reached");
      break;
    }
    //Serial.println(Playlist_count);
    Playlist.close();
    Playlist = PlaylistFolder.openNextFile();
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  *P_playlist_amount = Playlist_count;

  total_playlist_save.seek(6);
  total_playlist_save.write(Playlist_count);

  if (Playlist_count == 0) {
    Serial.println("Found no Playlist file");
  }

  save_to_list.seek(0);
  save_to_list.write(Playlist_count);  // save amount of Albums

  PlaylistFolder.close();
  Playlist.close();
  save_to_list.close();
}
//read from save file
void playlist_save_read(PlayList_list_Struct& PLS) {
  uint8_t playlist_count = 0;

  if (!SD_MMC.exists("/Playlist")) {
    SD_MMC.mkdir("/Playlist");
  }


  File playlist_list = SD_MMC.open("/Playlist/Playlist_list.txt", FILE_READ);

  playlist_count = *P_playlist_amount = playlist_list.read();

  setting_preview_info.playlists = playlist_count;

  if (playlist_count >= 100) {
    playlist_count = 100;
  }

  for (uint8_t i = 0; i < playlist_count; i++) {
    String name = playlist_list.readStringUntil('\n');
    PLS.Playlist_name[i] = name;
    Serial.println(name);
  }


  playlist_list.close();
}
//read songs list
uint8_t read_Playlist_Song_list(PlayList_Struct& PL, String Playlist_name) {
  //PlayList_Struct& AS selecting browse or play struct
  // take in Playlist_name as in path to write that file content to the selected struct

  File Songs_in_Playlist = SD_MMC.open(Playlist_name, FILE_READ);
  String song_name;
  uint8_t count_song = 0;

  for (uint8_t i = 0; i < Songs_per_Playlist_limit; i++) {
    song_name = Songs_in_Playlist.readStringUntil('\n');
    if (song_name.charAt(0) != '/') {
      count_song = i;
      break;
    }
    PL.Song_name[i] = song_name;
  }
  return count_song;
}
//====================================

//==========
void setting() {
  static int8_t index = 0;
  bool audio_once = false;
  bool draw_static = true;
  bool change = true;

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

  while (1) {
    if (digitalRead(UP_pin) == HIGH && index > 0) {
      index--;
      delay(50);
      tft.fillRect(0, 44, 240, 240, 0);
      change = true;
    }
    if (digitalRead(DOWN_pin) == HIGH && index < 4) {
      index++;
      delay(50);
      tft.fillRect(0, 44, 240, 240, 0);
      change = true;
    }
    //==================changing color when index hit
    if (change == true) {
      if (index != 0) {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
      }
      tft.setCursor(4, 26);
      tft.println("INTERFACE");

      if (index != 1) {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
      }
      tft.setCursor(67, 26);
      tft.println("AUDIO");

      if (index != 2) {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
      }
      tft.setCursor(106, 26);
      tft.println("STATS");

      if (index != 3) {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
      }
      tft.setCursor(145, 26);
      tft.println("BLET");
      if (index != 4) {
        tft.setTextColor(RGBcolorBGR(convert, 0x6B4D));  // dark gray
      } else {
        tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  // gold
      }
      tft.setCursor(200, 26);
      tft.println("Abouts");
      change = false;
    }

    //=================
    switch (index) {
      case 0:
        //tft.fillScreen(0);
        Interface(1);
        audio_once = false;
        break;
      case 1:
        if (skipbutton() == HIGH) {
          delay(100);
          while (1) {
            Audio(0);
            vTaskDelay(pdMS_TO_TICKS(50));
            if (backbutton() == HIGH) {
              delay(100);
              audio_once = false;
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

        Stats(1);
        break;

      case 3:
        BleT(1);
        break;

      case 4:

        break;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    if (backbutton() == HIGH || digitalRead(Back_pin) == HIGH) {
      delay(10);
      break;
    }
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
  vTaskDelay(pdMS_TO_TICKS(30));
}
void Audio(bool display) {
  static int8_t freq_index = 0;
  static bool once = false;
  static float pixel_scale = 82.0 / 20.0;  // 80 pixels high / 20 level (going from 0 to -20)
  static uint16_t freq_pos[9];
  static String freq_band_name[9];
  static bool eq_display_once = false;
  static bool update = false;

  String freq;
  int x_pos_sum = 40 - 22;
  bool last_number_length = 0;  // 1 = to 3 or more // 0 = to 2
  static bool draw_once = false;
  static int8_t index = 0;

  if (display == 0) {
    //=======================================
    if (digitalRead(UP_pin) == HIGH && index > 0) {
      index--;
      delay(100);
    }
    if (digitalRead(DOWN_pin) == HIGH && index < 1) {
      index++;
      delay(100);
    }
    //=======================================
    tft.setCursor(5, 49);
    if (index == 0) {
      tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
    } else {
      tft.setTextColor(RGBcolorBGR(convert, 0xef7d));
    }
    tft.println("L/R Balance:");
    tft.setCursor(5, 96);
    if (index == 1) {
      tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
    } else {
      tft.setTextColor(RGBcolorBGR(convert, 0xef7d));
    }
    tft.println("Equalizer:");
    //================================

    if (skipbutton() == true) {
      if (index == 1) {  // enter the EQ
        draw_once = false;
        while (1) {
          if (digitalRead(UP_pin) == HIGH && freq_index > 0) {
            freq_index--;
            draw_once = false;
            delay(100);
          }

          if (digitalRead(DOWN_pin) == HIGH && freq_index < 8) {
            freq_index++;
            draw_once = false;
            delay(100);
          }

          if (draw_once == false) {  // redraw index to gray
            for (int i = 0; i < 9; i++) {
              tft.fillRect(45 + i * 22, 125, 3, 82, RGBcolorBGR(convert, 0x73ae));
              tft.fillRect(45 + i * 22, 125, 3, float((*P_Gain[i] * -1.0) * pixel_scale), RGBcolorBGR(convert, 0));

              if (i == freq_index) {
                tft.setTextColor(RGBcolorBGR(convert, 0xFD85));  //change color for selected freq
              } else {
                tft.setTextColor(RGBcolorBGR(convert, 0x73ae));
              }

              tft.setCursor(freq_pos[i], 214);
              tft.println(freq_band_name[i]);

              if (update == true) {  // update save file
                File EQW;
                EQW = SD_MMC.open("/.Setting_Save.txt", "r+");  //erase and repopulate

                uint8_t gainint = *P_Gain[freq_index] * -10;
                EQW.seek(19 + freq_index);
                EQW.write(gainint);

                EQW.close();
                update = false;
              }
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

              if (backbutton() == true || ScreenMode(10) == 1) {
                draw_once = false;
                delay(100);
                break;
              }
            }
          }
          vTaskDelay(pdMS_TO_TICKS(20));
          if (backbutton() == true || ScreenMode(10) == 1) {
            draw_once = false;
            delay(100);
            tft.setTextColor(RGBcolorBGR(convert, 0x73ae));
            tft.setCursor(freq_pos[freq_index], 214);
            tft.println(freq_band_name[freq_index]);
            break;
          }
        }
      } else {
        int8_t lrvol = *P_LR_balance_vol;
        while (1) {
          tft.setTextColor(RGBcolorBGR(convert, 0xbdf7));
          tft.setCursor(26, 61);
          tft.println("Left");
          tft.setCursor(182, 61);
          tft.println("Right");
          tft.drawFastHLine(65, 64, 101, RGBcolorBGR(convert, 0xffff));
          tft.setTextColor(RGBcolorBGR(convert, 0xFD85));
          tft.setCursor(58, 61);
          tft.println("<");
          tft.setCursor(169, 61);
          tft.println(">");


          if (digitalRead(UP_pin) == HIGH && lrvol > -50) {
            lrvol--;
            draw_once = false;
            delay(100);
          }

          if (digitalRead(DOWN_pin) == HIGH && lrvol < 50) {
            lrvol++;
            draw_once = false;
            delay(100);
          }

          // Serial.println(lrvol);

          tft.fillRect(115 + (lrvol)-3, 61, 3, 3, 0);
          tft.fillRect(115 + (lrvol)-3, 61 + 4, 3, 3, 0);
          tft.fillRect(115 + (lrvol) + 1, 61, 3, 3, 0);
          tft.fillRect(115 + (lrvol) + 1, 61 + 4, 3, 3, 0);

          tft.drawFastVLine(115, 61, 7, RGBcolorBGR(convert, 0xffff));
          tft.drawFastVLine(115 + (lrvol), 61, 7, RGBcolorBGR(convert, 0xFD85));


          //if (update == true) {  // update save file
          //
          //  update = false;
          //}
          *P_LR_balance_vol = lrvol;

          vTaskDelay(pdMS_TO_TICKS(20));

          if (backbutton() == true || ScreenMode(10) == 1) {
            draw_once = false;

            tft.setTextColor(RGBcolorBGR(convert, 0xbdf7));
            tft.setCursor(58, 61);
            tft.println("<");
            tft.setCursor(169, 61);
            tft.println(">");
            tft.setCursor(182, 61);
            tft.println("Right");

            tft.drawFastVLine(115, 61, 7, RGBcolorBGR(convert, 0x73ae));
            tft.drawFastVLine(115 + (*P_LR_balance_vol), 61, 7, RGBcolorBGR(convert, 0xd508));

            File EQW;
            EQW = SD_MMC.open("/.Setting_Save.txt", "r+");
            EQW.seek(15);
            EQW.write(*P_LR_balance_vol);
            EQW.close();

            delay(100);
            break;
          }
        }
      }
    }


  } else {
    tft.setCursor(5, 49);
    tft.setTextColor(RGBcolorBGR(convert, 0xef7d));
    tft.println("L/R Balance:");
    tft.setCursor(5, 96);
    tft.setTextColor(RGBcolorBGR(convert, 0xef7d));
    tft.println("Equalizer:");

    tft.setTextColor(RGBcolorBGR(convert, 0xbdf7));
    tft.setCursor(26, 61);
    tft.println("Left");
    tft.drawFastHLine(65, 64, 101, RGBcolorBGR(convert, 0xbdf7));
    tft.setTextColor(RGBcolorBGR(convert, 0xbdf7));
    tft.setCursor(58, 61);
    tft.println("<");
    tft.setCursor(169, 61);
    tft.println(">");
    tft.setCursor(182, 61);
    tft.println("Right");

    tft.drawFastVLine(115, 61, 7, RGBcolorBGR(convert, 0x73ae));
    tft.drawFastVLine(115 + (*P_LR_balance_vol), 61, 7, RGBcolorBGR(convert, 0xd508));

    tft.setTextColor(RGBcolorBGR(convert, 0x73ae));
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


    tft.drawFastVLine(34, 124, 84, RGBcolorBGR(convert, 0xFD85));
    tft.drawFastVLine(234, 122, 91, RGBcolorBGR(convert, 0xFD85));

    tft.setCursor(7, 122);
    tft.println("10dB");

    tft.setCursor(12, 165 - 3);
    tft.println("0dB");

    tft.setCursor(1, 204);
    tft.println("-10dB");
  }
}
void Stats(bool display) {
  static int8_t index = 0;
  float used_space = *P_sdusedsize / 1024.0;
  float total_space = SD_MMC.totalBytes() / 1024.0 / 1024.0 / 1024.0;
  float free_space = total_space - used_space;

  static uint16_t highlight = 0xd508;
  static uint16_t lightgray = 0xbdf7;
  static uint16_t darkgray = 0x4208;
  static uint16_t white = 0xef7d;

  static bool draw_once = true;

  String update_albums = "UPDATING ALBUMS... ";
  String update_songs = "UPDATING ENTRIES... ";
  String update_playlist = "UPDATING PLAYLISTS... ";



  {
    //=========
    tft.setCursor(5, 49);
    tft.setTextColor(RGBcolorBGR(convert, white));
    tft.println("ACTIVE TIME:");

    tft.setCursor(5, 65);
    tft.println("CPU TEMP:");

    tft.setCursor(5, 102);
    tft.println("SD CARD:");

    tft.setCursor(5, 142);
    tft.println("UPDATE:");

    //============
    tft.setTextColor(RGBcolorBGR(convert, lightgray));
    tft.setCursor(99, 76);
    tft.println("-10");
    tft.setCursor(207, 76);
    tft.println("80");
    tft.drawFastVLine(107, 86, 7, RGBcolorBGR(convert, lightgray));
    tft.drawFastVLine(212, 86, 7, RGBcolorBGR(convert, lightgray));
    //======= sdcard // when updating must stop clsoe all other
    tft.setSwapBytes(1);
    tft.pushImage(4, 112, 30, 20, sdcard_icon);
    tft.setSwapBytes(0);

    tft.setTextColor(RGBcolorBGR(convert, lightgray));
    tft.setCursor(46 - 6, 113);
    tft.print(used_space, 1);  // used space
    tft.println(" GB");

    tft.setCursor(190 + 6, 113);
    tft.print(total_space, 1);  // total space
    tft.println(" GB");


    tft.setTextColor(RGBcolorBGR(convert, highlight), 0);
    tft.setCursor(51 + 6 * 5, 125);
    tft.print((100 - (free_space / total_space) * 100), 1);  // free space
    tft.print("% / ");
    tft.print(free_space, 1);
    tft.printf(" GB Free");

    tft.fillRect(82 + 6, 113, 100 - (free_space / total_space) * 100, 7, RGBcolorBGR(convert, highlight));
    tft.fillRect(80 + 6, 113, 2, 7, RGBcolorBGR(convert, highlight));
    tft.fillRect(183 + 6, 113, 2, 7, RGBcolorBGR(convert, highlight));
    //=======
    tft.setTextColor(RGBcolorBGR(convert, highlight));
    tft.setCursor(72, 154);
    tft.println(*P_total_albums);

    tft.setTextColor(RGBcolorBGR(convert, highlight));
    tft.setCursor(72, 154 + 12);
    tft.println(*P_SongAmount);

    tft.setTextColor(RGBcolorBGR(convert, highlight));
    tft.setCursor(72, 154 + 12 * 2);
    tft.println(*P_playlist_amount);

    list_of_update(14, 154, darkgray);
  }

  while (1) {
    int8_t temp_celsius = temperatureRead();
    int8_t min_temp = -10;
    int8_t max_temp = 80;
    int8_t position_translation = ((temp_celsius - min_temp) * 70 / ((max_temp - min_temp)));  //translation from 90 to 70
    uint8_t dynamic_x_bar = 107 + 1.5 * position_translation;

    if (digitalRead(UP_pin) == HIGH || digitalRead(DOWN_pin) == HIGH || digitalRead(Back_pin) == HIGH) {
      break;
    }

    {  // time readout / temp indicator
      tft.setTextColor(RGBcolorBGR(convert, highlight), 0);
      //tft.setCursor(104 - 12, 48);
      //tft.setCursor(159, 48);
      display_active_time(104 - 12, 48, 159, 48, RGBcolorBGR(convert, highlight));
      //==============
      tft.setTextColor(RGBcolorBGR(convert, highlight), 0);
      tft.setCursor(80, 65);
      tft.print(temp_celsius);
      tft.print(" C");
      tft.fillRect(dynamic_x_bar - 3, 86, 3, 7, RGBcolorBGR(convert, 0));
      tft.fillRect(dynamic_x_bar + 1, 86, 3, 7, RGBcolorBGR(convert, 0));
      tft.drawFastVLine(dynamic_x_bar, 86, 7, RGBcolorBGR(convert, highlight));
      //=========
      if (temp_celsius > 70) {
        tft.setTextColor(RGBcolorBGR(convert, 0xf227));
      } else {
        tft.setTextColor(RGBcolorBGR(convert, darkgray));
      }
      tft.setCursor(8, 77);
      tft.println("Over Heating");
      if (temp_celsius < 25) {
        tft.setTextColor(RGBcolorBGR(convert, 0x24be));
      } else {
        tft.setTextColor(RGBcolorBGR(convert, darkgray));
      }
      tft.setCursor(11, 85);
      tft.println("Cold");
      if (temp_celsius >= 25 && temp_celsius <= 70) {
        tft.setTextColor(RGBcolorBGR(convert, 0x4d6a));
      } else {
        tft.setTextColor(RGBcolorBGR(convert, darkgray));
      }
      tft.setCursor(41, 85);
      tft.println("Normal");
    }

    if (skipbutton() == true) {
      index = 0;
      while (1) {  //update block
        tft.drawFastVLine(6, 152, 12 * 3 + 5, RGBcolorBGR(convert, darkgray));
        if (index == 0) {
          tft.drawFastHLine(6, 157, 6, RGBcolorBGR(convert, highlight));
        } else {
          tft.drawFastHLine(6, 157, 6, RGBcolorBGR(convert, darkgray));
        }
        if (index == 1) {
          tft.drawFastHLine(6, 157 + 12, 6, RGBcolorBGR(convert, highlight));
        } else {
          tft.drawFastHLine(6, 157 + 12, 6, RGBcolorBGR(convert, darkgray));
        }
        if (index == 2) {
          tft.drawFastHLine(6, 157 + 12 * 2, 6, RGBcolorBGR(convert, highlight));
        } else {
          tft.drawFastHLine(6, 157 + 12 * 2, 6, RGBcolorBGR(convert, darkgray));
        }
        if (index == 3) {
          tft.drawFastHLine(6, 157 + 12 * 3, 6, RGBcolorBGR(convert, highlight));
        } else {
          tft.drawFastHLine(6, 157 + 12 * 3, 6, RGBcolorBGR(convert, darkgray));
        }

        tft.drawFastVLine(6, 152, 12 * index + 5, RGBcolorBGR(convert, highlight));

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
        if (index > 3) {
          index = 0;
        }
        if (index < 0) {
          index = 3;
        }

        // static uint8_t

        if (index == 0) {
          tft.setTextColor(RGBcolorBGR(convert, highlight), 0);
          if (skipbutton() == true) {
            *P_if_updating_index = true;
            *P_playstate = true;


            // String text = "UPDATING ALBUMS... ";

            tft.setCursor(14, 154);
            tft.print(update_albums);

            Artists_Album_read_and_save(Artis_Folder_List, 14 + update_albums.length() * 5 + update_albums.length(), 154);

            tft.setCursor(14, 154);
            tft.println("DONE!                           ");

            tft.setCursor(72, 154);
            tft.println(*P_total_albums);

            delay(500);
            *P_if_updating_index = false;
            *P_update_done = true;
          }
        } else {
          tft.setTextColor(RGBcolorBGR(convert, darkgray), 0);
        }
        tft.setCursor(14, 154);
        tft.println("ALBUMS");

        if (index == 1) {
          tft.setTextColor(RGBcolorBGR(convert, highlight), 0);
          if (skipbutton() == true) {
            *P_if_updating_index = true;
            *P_playstate = true;
            //String text = "UPDATING ENTRIES... ";

            tft.setCursor(14, 154 + 12);
            tft.println(update_songs);

            Updating_songs_EX(14 + update_songs.length() * 5 + update_songs.length(), 154 + 12);

            tft.setCursor(14, 154 + 12);
            tft.println("SAVING TO RAM             ");

            Update_songs_name(Songname);

            tft.setCursor(14, 154 + 12);
            tft.println("DONE!               ");

            tft.setCursor(72, 154 + 12);
            tft.println(*P_SongAmount);

            delay(500);
            *P_update_done = true;
            *P_if_updating_index = false;
          }
        } else {
          tft.setTextColor(RGBcolorBGR(convert, darkgray), 0);
        }
        tft.setCursor(14, 154 + 12);
        tft.println("SONGS");

        if (index == 2) {
          tft.setTextColor(RGBcolorBGR(convert, highlight), 0);
          if (skipbutton() == true) {
            *P_if_updating_index = true;
            *P_playstate = true;
            //String text = "UPDATING PLAYLISTS... ";

            tft.setCursor(14, 154 + 12 * 2);
            tft.print(update_playlist);

            save_to_Playlist_list(PlayList_list, 14 + update_playlist.length() * 5 + update_playlist.length(), 154 + 12 * 2);

            tft.setCursor(14, 154 + 12 * 2);
            tft.println("DONE!                              ");

            tft.setCursor(72, 154 + 12 * 2);
            tft.println(*P_playlist_amount);

            delay(500);
            *P_update_done = true;
            *P_if_updating_index = false;
          }
        } else {
          tft.setTextColor(RGBcolorBGR(convert, darkgray), 0);
        }
        tft.setCursor(14, 154 + 12 * 2);
        tft.println("PLAYLIST");

        if (index == 3) {
          tft.setTextColor(RGBcolorBGR(convert, highlight), 0);
          if (skipbutton() == true) {
            *P_if_updating_index = true;
            *P_playstate = true;
            tft.setCursor(14, 154);
            tft.print(update_albums);
            Artists_Album_read_and_save(Artis_Folder_List, 14 + update_albums.length() * 5 + update_albums.length(), 154);
            tft.setCursor(14, 154);
            tft.println("DONE!                           ");
            tft.setCursor(72, 154);
            tft.println(*P_total_albums);

            tft.setCursor(14, 154 + 12);
            tft.println(update_songs);
            Updating_songs_EX(14 + update_songs.length() * 5 + update_songs.length(), 154 + 12);
            tft.setCursor(14, 154 + 12);
            tft.println("SAVING TO RAM             ");
            Update_songs_name(Songname);
            tft.setCursor(14, 154 + 12);
            tft.println("DONE!               ");
            tft.setCursor(72, 154 + 12);
            tft.println(*P_SongAmount);

            tft.setCursor(14, 154 + 12 * 2);
            tft.print(update_playlist);
            save_to_Playlist_list(PlayList_list, 14 + update_playlist.length() * 5 + update_playlist.length(), 154 + 12 * 2);
            tft.setCursor(14, 154 + 12 * 2);
            tft.println("DONE!                              ");
            tft.setCursor(72, 154 + 12 * 2);
            tft.println(*P_playlist_amount);

            delay(100);
            *P_update_done = true;
            *P_if_updating_index = false;
          }
        } else {
          tft.setTextColor(RGBcolorBGR(convert, darkgray), 0);
        }
        tft.setCursor(14, 154 + 12 * 3);
        tft.println("ALL");

        vTaskDelay(pdMS_TO_TICKS(50));
        if (backbutton() == true) {
          delay(100);
          list_of_update(14, 154, darkgray);
          break;
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
void list_of_update(int16_t x, int16_t y, uint16_t darkgray) {
  // static uint16_t darkgray = 0x4208;
  tft.setTextColor(RGBcolorBGR(convert, darkgray));
  tft.setCursor(x, y);
  tft.println("ALBUMS");
  tft.setTextColor(RGBcolorBGR(convert, darkgray));
  tft.setCursor(x, y + 12);
  tft.println("SONGS");
  tft.setTextColor(RGBcolorBGR(convert, darkgray));
  tft.setCursor(x, y + 12 * 2);
  tft.println("PLAYLIST");
  tft.setTextColor(RGBcolorBGR(convert, darkgray));
  tft.setCursor(x, y + 12 * 3);
  tft.println("ALL");

  tft.drawFastVLine(x - 8, y + 152 - 154, 12 * 3 + 5, RGBcolorBGR(convert, darkgray));
  tft.drawFastHLine(x - 8, y + 157 - 154, 6, RGBcolorBGR(convert, darkgray));
  tft.drawFastHLine(x - 8, y + 157 - 154 + 12, 6, RGBcolorBGR(convert, darkgray));
  tft.drawFastHLine(x - 8, y + 157 - 154 + 12 * 2, 6, RGBcolorBGR(convert, darkgray));
  tft.drawFastHLine(x - 8, y + 157 - 154 + 12 * 3, 6, RGBcolorBGR(convert, darkgray));
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
  const int artis_pixel_to_read = 120 * rows;
  const int pixel_chunk = 3 * pixel_to_read;
  const int artis_pixel_chunk = 3 * artis_pixel_to_read;
  int shiftdown = -rows;  //239
  uint16_t count_correct = 0;
  bool new_check = true;

  int countpixel = -1;
  //MALLOC_CAP_DEFAULT
  //MALLOC_CAP_SPIRAM

  byte* PixelSamples120 = NULL;
  byte* PixelSamples_past120 = NULL;

  PixelSamples120 = (byte*)heap_caps_malloc(pixel_chunk, MALLOC_CAP_SPIRAM);
  PixelSamples_past120 = (byte*)heap_caps_malloc(480 * 3, MALLOC_CAP_SPIRAM);
  memset(PixelSamples_past120, 0, 480 * 3);

  unsigned long Stime1;
  unsigned long Stime2;

  int count = -2;

  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    uint8_t menutype = *P_menu_type;

    if ((*P_songs_menu_song_change == true && menutype == 1) || (*P_songs_menu_song_change_AL == true && menutype == 0) || (*P_songs_menu_song_change_PL == true && menutype == 2)) {
      Stime1 = micros();
      // BMPIMG120.seek(54);
      *P_art120done = false;

      if (bmpheader.Width != 120) {
        //Serial.print(" Art Cover: ");
        // Serial.println(bmpheader.Width);
        for (int run = 0; run < 2; run++) {
          BMPIMG120.read(PixelSamples120, pixel_chunk);

          shiftdown += rows;
          int x = 0;
          uint16_t rgb565[120 * 60];
          uint8_t byte1 = 0;
          uint8_t byte2 = 0;
          uint8_t byte3 = 0;

          for (int i = 0; i < pixel_to_read; i += 2) {  // converting rgb888_to_rgb565
            x = i * 3;
            count += 2;
            countpixel++;
            byte1 = PixelSamples120[x + 2] >> 3;  //RGB
            byte2 = PixelSamples120[x + 1] >> 2;  //G
            byte3 = PixelSamples120[x] >> 3;      //B
            rgb565[countpixel] = (byte3 << 11 | byte2 << 5 | byte1 << 0);
            if (count > 236) {
              if ((i + 2 / 240) % 2 == 0) {
                i += 240;
              }
              count = -2;
            }
          }
          countpixel = -1;
          if (menutype == 0) {  //albums
            art120_sprite_AL.pushImage(0, shiftdown, 120, 60, rgb565);
            //Serial.println("making art ALBUM");
          } else if (menutype == 1) {
            art120_sprite.pushImage(0, shiftdown, 120, 60, rgb565);
            // Serial.println("making art SONGS");
          } else if (menutype == 2) {
            art120_sprite_PL.pushImage(0, shiftdown, 120, 60, rgb565);
            //Serial.println("making art PLAYLIST");
          }
        }
        shiftdown = -rows;  // reset
        *P_art120done = true;
        if (menutype == 0) {
          *P_songs_menu_song_change_AL = false;
        } else if (menutype == 1) {
          *P_songs_menu_song_change = false;
        } else if (menutype == 2) {
          *P_songs_menu_song_change_PL = false;
        }
        // *P_art120done = true;
      } else {
        for (int run = 0; run < 120 / rows; run++) {
          BMPIMG120.read(PixelSamples120, artis_pixel_chunk);
          shiftdown += rows;

          int x = 0;
          uint16_t rgb565[artis_pixel_to_read];
          uint8_t byte1 = 0;
          uint8_t byte2 = 0;
          uint8_t byte3 = 0;

          for (int i = 0; i < artis_pixel_to_read; i += 1) {  // converting rgb888_to_rgb565
            x = i * 3;
            byte1 = PixelSamples120[x + 2] >> 3;  //RGB
            byte2 = PixelSamples120[x + 1] >> 2;  //G
            byte3 = PixelSamples120[x] >> 3;      //B
            rgb565[i] = (byte3 << 11 | byte2 << 5 | byte1 << 0);
          }

          art120_sprite_AL.pushImage(0, shiftdown, 120, 60, rgb565);
        }
        shiftdown = -rows;  // reset
        *P_songs_menu_song_change_AL = false;
      }

      *P_art120done = true;
      Stime2 = micros();
      Serial.println("mini art");
      snapshot_time(Stime1, Stime2);

      *P_snapshot2 = micros();
      Serial.print("mini art cover ");
      snapshot_time(*P_snapshot1, *P_snapshot2);

    } else {
      Serial.println("old art");
      *P_art120done = true;
    }
  }
}
//return if artcover exist or not, 1 = no, 2 = yes
int8_t openingbmp120(String songname, uint8_t menu_type, bool overdrive, bool artstate_overdrive) {
  *P_snapshot1 = micros();

  static bool Osave_songs_art_state = 0;
  static bool Osave_album_art_state = 0;
  static bool Osave_playlis_art_state = 0;
  static int8_t art_state = 0;
  String get_extention = songname.substring(songname.length() - 4, songname.length());
  // Serial.println(get_extention);

  *P_menu_type = menu_type;

  if (overdrive == true) {
    if (menu_type == 1) {
      Osave_songs_art_state = artstate_overdrive;
    } else if (menu_type == 0) {
      Osave_album_art_state = artstate_overdrive;
    } else if (menu_type == 2) {
      Osave_playlis_art_state = artstate_overdrive;
    }
  } else {
    if ((*P_songs_menu_song_change == true && menu_type == 1) || (*P_songs_menu_song_change_AL == true && menu_type == 0) || (*P_songs_menu_song_change_PL == true && menu_type == 2)) {

      if (get_extention == ".wav" || get_extention == ".alb" || get_extention == ".pls") {
        songname = songname.substring(0, songname.length() - 4);
      }
      songname.concat(".bmp");
      BMPIMG120.close();
      Serial.println(songname);

      if (SD_MMC.exists(songname)) {
        BMPIMG120 = SD_MMC.open(songname, FILE_READ);
        BMPIMG120.read((byte*)&bmpheader, 54);
        if (bmpheader.ImgDataStart != 54) {
          BMPIMG120.seek(bmpheader.ImgDataStart);  // in case each software make the data start at different index
        }
        if (bmpheader.Width != 120) {
          Serial.println("can open art cover");
        } else {
          Serial.println("can open artis profile");
        }
        art_state = 1;
      } else {
        art_state = 0;
      }

      if (art_state == 0) {
        if (menu_type == 1) {  //set to false since it doesnt need to run when there are no art
          *P_songs_menu_song_change = false;
        } else if (menu_type == 0) {
          *P_songs_menu_song_change_AL = false;
        } else if (menu_type == 2) {
          *P_songs_menu_song_change_PL = false;
        }
      }
      switch (menu_type) {
        case 0:
          Osave_album_art_state = art_state;
          break;
        case 1:
          Osave_songs_art_state = art_state;
          break;
        case 2:
          Osave_playlis_art_state = art_state;
          break;
      }
    }
  }

  switch (menu_type) {
    case 0:
      if (Osave_album_art_state == 0) {
        return 1;
      } else {
        xTaskNotifyGive(taskdraw120art);
        return 2;
      }
      break;
    case 1:
      if (Osave_songs_art_state == 0) {
        return 1;
      } else {
        xTaskNotifyGive(taskdraw120art);
        return 2;
      }
      break;
    case 2:
      if (Osave_playlis_art_state == 0) {
        return 1;
      } else {
        xTaskNotifyGive(taskdraw120art);
        return 2;
      }
      break;
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
void f_count_time(void* pvParameters) {
  static uint64_t time = 0;
  uint64_t time_run;
  while (1) {
    if (*P_if_updating_index == false) {
      time_run = *P_sec_get + (millis() / 1000);
      *P_timerun_sec = time_run;
      activetime.seek(0);
      vTaskDelay(pdMS_TO_TICKS(100));
      activetime.write((uint8_t*)&time_run, 8);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void display_active_time(int16_t dayx, int16_t dayy, int16_t timex, int16_t timey, uint16_t main_color) {
  uint8_t sec = 0;
  uint8_t min = 0;
  uint8_t hr = 0;
  uint16_t day = 0;

  sec = *P_timerun_sec % 60;
  min = (*P_timerun_sec / 60) % 60;
  hr = (*P_timerun_sec / 3600) % 24;
  day = *P_timerun_sec / 86400;

  tft.setCursor(dayx, dayy);
  tft.setTextColor(main_color, 0);
  tft.setTextSize(1);
  tft.print(day);
  tft.print(" days");

  tft.setCursor(timex, timey);
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
    if (bmpheader.ImgDataStart != 54) {
      BMPIMG.seek(bmpheader.ImgDataStart);  // incase each software make the data start at different index
    }
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
      //  BMPIMG.setBufferSize(size_t size)

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

void setting_save_preset() {
  uint8_t increment = 15;
  uint8_t rotation_speed = 15;
  uint8_t rotation_direction = 0;

  uint16_t FS1 = 10;
  uint16_t FS2 = 50;
  uint16_t FS3 = 100;

  int8_t LRBalance = -10;
  uint8_t BalanceSide = 1;  // mean the side getting tune down

  uint8_t EQ[9];

  //if(file doesnt exsit= load the preset)

  for (int i = 0; i < 9; i++) {
    EQ[i] = Gain[i] * -10;
  }
  File SS = SD_MMC.open("/.Setting_Save.txt", FILE_WRITE);

  SS = SD_MMC.open("/.Setting_Save.txt", "r+");

  SS.seek(0);
  SS.write(increment);
  SS.write(124);
  SS.write(rotation_speed);
  SS.write(124);
  SS.write(rotation_direction);
  SS.write(124);

  SS.write((uint8_t*)&FS1, 2);
  SS.write(124);
  SS.write((uint8_t*)&FS2, 2);
  SS.write(124);
  SS.write((uint8_t*)&FS3, 2);
  SS.write(124);

  SS.write(LRBalance);
  SS.write(124);
  SS.write(BalanceSide);
  SS.write(124);

  SS.write(EQ, 9);
  SS.write(124);
}

void getting_LRB() {
  File LRB = SD_MMC.open("/.Setting_Save.txt", FILE_READ);
  LRB.seek(15);
  *P_LR_balance_vol = LRB.read();
  LRB.close();
}
void setting_quick_info(bool reset) {
  uint16_t shadow = 0x73AE;

  static bool once = false;
  static bool draw_again = false;


  if (once == false) {
    //sd card
    setting_qi.setTextColor(RGBcolorBGR(convert, shadow));
    setting_qi.setCursor(0, 1);
    setting_qi.print("SD: ");
    setting_qi.print(setting_preview_info.sd_precent_used_space, 0);
    setting_qi.print("% ");
    setting_qi.print(setting_preview_info.remaining_space, 0);
    setting_qi.print("GB");

    setting_qi.setTextColor(RGBcolorBGR(convert, 0xFD85));
    setting_qi.setCursor(1, 0);
    setting_qi.print("SD: ");
    setting_qi.print(setting_preview_info.sd_precent_used_space, 0);
    setting_qi.print("% ");
    setting_qi.print(setting_preview_info.remaining_space, 0);
    setting_qi.print("GB");

    //blet
    setting_qi.setTextColor(RGBcolorBGR(convert, 0x51E), 0);
    setting_qi.setCursor(0, 13);
    setting_qi.println("BleT:");
    setting_qi.setTextColor(RGBcolorBGR(convert, 0x631f), 0);
    setting_qi.setCursor(36, 13);
    setting_qi.println("METIS Inc :3");  // device name

    // powe meter
    setting_qi.setTextColor(RGBcolorBGR(convert, 0xEF7D));
    setting_qi.setCursor(0, 37);
    setting_qi.println("A:");
    setting_qi.setCursor(0, 49);
    setting_qi.println("V:");
    setting_qi.setCursor(0, 61);
    setting_qi.println("ECS:");

    //setting_qilbum, song, playlist
    setting_qi.setCursor(67, 37);
    setting_qi.println("ALB:");
    setting_qi.setCursor(67, 49);
    setting_qi.println("WAV:");
    setting_qi.setCursor(67, 61);
    setting_qi.println("PLS:");

    setting_qi.setTextColor(RGBcolorBGR(convert, shadow));
    setting_qi.setCursor(91, 37);
    setting_qi.println(setting_preview_info.albums);
    setting_qi.setCursor(91, 49);
    setting_qi.println(setting_preview_info.songs);
    setting_qi.setCursor(91, 61);
    setting_qi.println(setting_preview_info.playlists);

    setting_qi.setTextColor(RGBcolorBGR(convert, 0xFD85));
    setting_qi.setCursor(92, 36);
    setting_qi.println(setting_preview_info.albums);
    setting_qi.setCursor(92, 48);
    setting_qi.println(setting_preview_info.songs);
    setting_qi.setCursor(92, 60);
    setting_qi.println(setting_preview_info.playlists);
    setting_qi.setCursor(12, 37);
    setting_qi.println("250mA");
    setting_qi.setCursor(12, 49);
    setting_qi.println("3.3V");
    setting_qi.setCursor(24, 61);
    setting_qi.println("0.66W");
  }

  if (reset == 1) {
    draw_again = true;
  }

  if (draw_again == true && reset == 0) {
    setting_qi.pushSprite(118, 1, 0);
    tft.setRotation(2);

    banner.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
    banner.writedata(TFT_MAD_MY);
    banner.pushSprite(118, 122 + 1 + 2 + 2);

    profile.writecommand(TFT_MADCTL);  // command for mirroring Y // for multi rows
    profile.writedata(TFT_MAD_MY);
    profile.pushSprite(118, 2);

    tft.setRotation(0);
    draw_again = false;
  }

  if (reset == 0) {
    //temp
    tft.setTextColor(RGBcolorBGR(convert, 0xFC00), 0);
    tft.setCursor(209, 2);
    tft.print("T:");
    tft.print(temperatureRead(), 0);
    tft.print("C");

    //time
    display_active_time(121, 26, 183, 26, RGBcolorBGR(convert, 0x7DC6));
  }
  //tft.pushImage(117,74,122,40,) // banner 122x40// take from sd card or internal
}

void load_banner_img() {

  if (SD_MMC.exists("/IMG")) {
    Serial.println("found IMG folder");
  } else {
    SD_MMC.mkdir("/IMG");
    Serial.println(" made IMG foldder");
    return;
  }
  if (SD_MMC.exists("/IMG/profile.bmp")) {
    Serial.println("found profile img");
  } else {
    return;
  }

  BMPIMG.close();
  BMPIMG = SD_MMC.open("/IMG/banner.bmp");
  // Width;

  BMPIMG.read((byte*)&bmpheader, 54);
  if (bmpheader.ImgDataStart != 54) {
    BMPIMG.seek(bmpheader.ImgDataStart);  // incase each software make the data start at different index
  }

  const uint8_t rows = 2;
  const int artis_pixel_to_read = 120 * rows;
  const int artis_pixel_chunk = 3 * artis_pixel_to_read;  // pixle clolor data in 888

  int shiftdown = -rows;  //239

  static byte colordata[artis_pixel_chunk];

  for (int run = 0; run < bmpheader.Height / rows; run++) {  // processing img data
    BMPIMG.read(colordata, artis_pixel_chunk);
    shiftdown += rows;

    int x = 0;
    uint16_t rgb565[artis_pixel_to_read];
    uint8_t byte1 = 0;
    uint8_t byte2 = 0;
    uint8_t byte3 = 0;

    for (int i = 0; i < artis_pixel_to_read; i += 1) {  // converting rgb888_to_rgb565
      x = i * 3;
      byte1 = colordata[x + 2] >> 3;  //RGB
      byte2 = colordata[x + 1] >> 2;  //G
      byte3 = colordata[x] >> 3;      //B
      rgb565[i] = (byte3 << 11 | byte2 << 5 | byte1 << 0);
    }

    banner.pushImage(0, shiftdown, 120, rows, rgb565);
  }
}
void load_profile_img() {
  if (SD_MMC.exists("/IMG")) {
    Serial.println("found IMG folder");
  } else {
    SD_MMC.mkdir("/IMG");
    Serial.println(" made IMG foldder");
    return;
  }
  if (SD_MMC.exists("/IMG/profile.bmp")) {
    Serial.println("found profile img");
  } else {
    return;
  }

  BMPIMG.close();
  BMPIMG = SD_MMC.open("/IMG/profile.bmp");

  BMPIMG.read((byte*)&bmpheader, 54);
  if (bmpheader.ImgDataStart != 54) {
    BMPIMG.seek(bmpheader.ImgDataStart);  // incase each software make the data start at different index
  }

  const uint8_t rows = 2;
  const int artis_pixel_to_read = 120 * rows;
  const int artis_pixel_chunk = 3 * artis_pixel_to_read;  // pixle clolor data in 888

  int shiftdown = -rows;  //239

  static byte colordata[artis_pixel_chunk];

  for (int run = 0; run < 120 / rows; run++) {
    BMPIMG.read(colordata, artis_pixel_chunk);
    shiftdown += rows;

    int x = 0;
    uint16_t rgb565[artis_pixel_to_read];
    uint8_t byte1 = 0;
    uint8_t byte2 = 0;
    uint8_t byte3 = 0;

    for (int i = 0; i < artis_pixel_to_read; i += 1) {  // converting rgb888_to_rgb565
      x = i * 3;
      byte1 = colordata[x + 2] >> 3;  //RGB
      byte2 = colordata[x + 1] >> 2;  //G
      byte3 = colordata[x] >> 3;      //B
      rgb565[i] = (byte3 << 11 | byte2 << 5 | byte1 << 0);
    }

    profile.pushImage(0, shiftdown, 120, rows, rgb565);
  }
}

void disk_audio_controll(void* pvParameters) {

  unsigned long now;
  static unsigned long last;
  static int last_angle;
  static int accum_last_angle;
  static int accum_angle;
  static int count_revolution = 0;
  static int delta = 0;
  bool once = false;
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    if (!as5600.isMagnetDetected()) {
      return;
    }

    now = millis();
    uint16_t rawAngle = as5600.getRawAngle();
    int angle = rawAngle * (360.0 / 4096.0);

    if ((angle - last_angle) > 270) {
      count_revolution++;
      delta = (angle - last_angle) + 1;
    } else if ((angle - last_angle) < -270) {
      count_revolution++;
      delta = (angle - last_angle) - 1;
    }

  //Serial.print("angle_sub:");
  //Serial.println(angle - last_angle);

  //Serial.print("delta:");
  //Serial.println(delta);

  //Serial.print("angle:");
  //Serial.println(angle);
  //Serial.println(count_revolution);
    // if (last_angle > 355) {
    //   last_angle = last_angle - 360;
    // }


    accum_angle += ((angle - last_angle) - delta);
    delta = 0;
   //Serial.print("accum_angle:");
   //Serial.println(accum_angle);

    float speed = ((abs(accum_angle - accum_last_angle)) / float((now - last) / 1000.0));
    speed /= 360;

    last_angle = angle;
    last = now;
    accum_last_angle = accum_angle;

    int8_t direction = rotation_direction(accum_angle);


    if (direction == 0) {
    //  Serial.println("clock wise");
      if (speed > 2) {
        speed = 1.9;
      }
      i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * speed + 0.1);
      *P_reverse_spin = false;
      once = false;
    } else if (direction == 2) {
 //     Serial.println("coutner cokc wise");
      if (speed > 1.5) {
        speed = 1.4;
      }
      i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * speed + 0.1);
      *P_reverse_spin = true;
      once = false;
    } else if (direction == 1) {
      if (once == false) {
        *P_reverse_spin = false;
        i2s_set_sample_rates(i2s_num, WavHeader.SampleRate * 1);
        once = true;
      }
    }
 //   Serial.print("deg/s:");
 //   Serial.println(speed);


    delay(50);
  }
}

uint8_t rotation_direction(int angle) {
  int new_angle = angle;
  static int last_angle = 0;
  if (new_angle < last_angle) {
    last_angle = new_angle;
    return 2;  //counter clock
  } else if (new_angle > last_angle) {
    last_angle = new_angle;
    return 0;  // cock wise
  } else {
    last_angle = new_angle;
    return 1;  // nuetural// for now
  }
}
