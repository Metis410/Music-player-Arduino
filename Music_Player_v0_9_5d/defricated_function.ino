/*
void Songs_inner() {  // display full song without cover art

//one licne list dispaly

  static int16_t index_song = -30;  // -30 to get it to sync with the perset index //at 131
  static int text_y = -31;
  static uint16_t pxstrlensong = 0;

  static uint16_t pxstrlenartis = 0;
  static float scrolls[9];
  static bool scoll_end[9] = { 0, 0, 0, 0, 0, 0, 0 };

  static bool S_up = false;  // heh, sup
  static bool S_down = false;
  static int16_t w = 0;

  static int16_t trans = 0;
  String name;
  String S_index;

  String artis_name;
  String song_name;

  uint8_t spacing = 31;
  uint8_t increment = spacing;
  uint8_t scroll_speed = 5;

  int16_t selected_artis_color = 0xFD85;
  int16_t selected_song_color = 0xffff;
  int16_t nd_color = 0x18e3;
  int16_t rd_color = 0x18e3;
  int16_t th_color = 0x18e3;


  //0xd508 light gold

  //0xFD85 bright gold

  S_index = num_to_str(index_song);

  tft.drawFastHLine(0, 13, 120, RGBcolorBGR(convert, 0xd508));
  tft.drawFastHLine(120, 20, 120, RGBcolorBGR(convert, 0xd508));
  tft.drawFastVLine(120, 0, 20, RGBcolorBGR(convert, 0xd508));
  tft.setCursor(123, 2);
  tft.setTextSize(2);
  tft.setTextColor(RGBcolorBGR(convert, 0xd508));
  tft.println("Songs");

  index_song = *P_songs_menu_index;

  if (skipbutton() == true) {  //a song is selected
    *P_menu_selected_song = true;
    *P_change_song_notice = true;
    *P_song_changed = true;
    *P_selected_song_index = index_song;  // the song selected always in the 4th place due to how index read
    *P_playstate = false;
  }

  tft.setTextColor(RGBcolorBGR(convert, 0xffff), 0);
  tft.setTextSize(1);
  tft.setCursor(208 - 6 - (S_index.length() * 5 + S_index.length()), 9);
  tft.printf(" %d", index_song);
  tft.setCursor(208, 9);
  tft.printf("/%d", *P_SongAmount);

  if (index_song != *P_SongAmount) {  // for when at the end of the list
    if (digitalRead(DOWN_pin) == HIGH) {
      S_down = true;
      *P_songs_menu_song_change = true;
    }
  }

  if (index_song != 1) {  // for when at the top of the list
    if (digitalRead(UP_pin) == HIGH) {
      *P_songs_menu_song_change = true;
      S_up = true;
    }
  }


  for (int q = 0; q < spacing; q += scroll_speed) {

    if (q >= spacing) {
      q = spacing;
    }


    if (S_up == true) {
      w = q * 1;
      trans = q * 1;
    }
    if (S_down == true) {
      w = q * -1;
      trans = q * -1;
    }

    for (int i = 0; i < 9; i++) {  /// this one for when controlling this menu

      if (i + index_song - 5 < 0) {  // prevent it from reading out of bound

        song_name = " ";
        artis_name = " ";

      } else if (i + index_song - 5 > *P_SongAmount - 1) {

        song_name = " ";
        artis_name = " ";

      } else {

        name = Songname.songsnameS[i + index_song - 5];  //at index, ex: at 131 - 4 + i  so it start at above the index
        name = name.substring(0, name.length() - 4);     //remove .wave
        for (uint8_t i = 0; i < name.length(); i++) {
          if (name.charAt(i) == '-') {
            if (name.charAt(i + 1) == ' ') {    // check for " "
              if (name.charAt(i + 2) == ' ') {  // check for "  "
                song_name = name.substring(i + 3, name.length());
              } else {
                song_name = name.substring(i + 2, name.length());
              }
            } else {  // check for ""
              song_name = name.substring(i + 1, name.length());
            }

            if (name.charAt(i - 1) == ' ') {    // check for " "
              if (name.charAt(i - 2) == ' ') {  // check for "  "
                artis_name = name.substring(1, i - 2);
              } else {
                artis_name = name.substring(1, i - 1);
              }
            } else {  // check for ""
              artis_name = name.substring(1, i);
            }
            break;
          }
        }
      }


      pxstrlensong = 5 * song_name.length() + song_name.length() - 1;
      pxstrlenartis = 5 * artis_name.length() + artis_name.length() - 1;


      switch (i) {
        case 0:
          text_y = -spacing + w;

          inner_songs.setTextColor(RGBcolorBGR(convert, th_color), 0);
          break;
        case 1:
          text_y = 0 + w;

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, th_color), 0);
          } else if (S_up == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, th_color, rd_color)), 0);  //dark gray to gray
          } else if (S_down == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, th_color, 0)), 0);  //  dark gray to black
          }
          break;
        case 2:
          text_y = spacing * 1 + w;

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, rd_color), 0);
          } else if (S_up == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, rd_color, nd_color)), 0);  //dark gray to gray
          } else if (S_down == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, rd_color, th_color)), 0);  //  dark gray to black
          }
          break;
        case 3:
          text_y = spacing * 2 + w;

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, nd_color), 0);
          } else if (S_up == true) {

            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, nd_color, selected_song_color)), 0);  //  gray to gold
          } else if (S_down == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, nd_color, rd_color)), 0);  // gray to dark gray
          }
          break;
        case 4:  ////////////////////////////////////////
          text_y = spacing * 3 + w;

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, selected_artis_color), 0);
            inner_songs.setCursor(120 - (pxstrlenartis / 2.0), text_y);
            inner_songs.print(artis_name);
            inner_songs.setTextColor(RGBcolorBGR(convert, selected_song_color), 0);
          } else {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, selected_artis_color, nd_color)), 0);
            inner_songs.setCursor(120 - (pxstrlenartis / 2.0), text_y);
            inner_songs.print(artis_name);
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, selected_song_color, nd_color)), 0);
          }


          break;  //////////////////////////////////////
        case 5:
          text_y = spacing * 4 + w;

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, nd_color), 0);
          } else if (S_up == true) {

            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, nd_color, rd_color)), 0);  //  gray to gold

          } else if (S_down == true) {

            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, nd_color, selected_song_color)), 0);  // gray to dark gray
          }
          break;
        case 6:
          text_y = spacing * 5 + w;

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, rd_color), 0);
          } else if (S_up == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, rd_color, th_color)), 0);  //dark gray to gray
          } else if (S_down == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, rd_color, nd_color)), 0);  //  dark gray to black
          }

          break;
        case 7:
          text_y = spacing * 6 + w;

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, th_color), 0);
          } else if (S_up == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, th_color, 0)), 0);  //dark gray to gray
          } else if (S_down == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, th_color, rd_color)), 0);  //  dark gray to black
          }
          break;
        case 8:
          text_y = spacing * 7 + w;
          inner_songs.setTextColor(RGBcolorBGR(convert, th_color), 0);
          break;

        default:
          break;
      }



      inner_songs.fillRect(120 - (pxstrlensong / 2.0), text_y - scroll_speed + 10, pxstrlensong + 1, scroll_speed, 0);  // for top and bottom of the text to precent
      inner_songs.fillRect(120 - (pxstrlensong / 2.0), text_y + 8 + 10, pxstrlensong + 1, scroll_speed, 0);

      inner_songs.fillRect(120 - (pxstrlenartis / 2.0), text_y - scroll_speed, pxstrlenartis + 1, scroll_speed, 0);
      inner_songs.fillRect(120 - (pxstrlenartis / 2.0), text_y + 8, pxstrlenartis + 1, scroll_speed, 0);


      inner_songs.setCursor(120 - (pxstrlenartis / 2.0), text_y);  // artis name for the rest
      inner_songs.print(artis_name);


      if (song_name.length() > 40) {

        if (scrolls[i] <= 240 - pxstrlensong - 20) {  // only scroll as far as the length be behinde
          scoll_end[i] = true;
        }
        if ((scrolls[i] >= 0 + 20)) {
          scoll_end[i] = false;
        }

        if (i == 4) {  // restricted to the selected
          if (scoll_end[i] == false) {
            scrolls[i] = (scrolls[i] - .2 - (1 - 17.0 / song_name.length()) / 2.0);  // scale by string lenght
          } else {
            scrolls[i] = (scrolls[i] + .2 + (1 - 17.0 / song_name.length()) / 2.0);
          }
        }

        inner_songs.drawFastVLine(scrolls[i] - 1, text_y + 10, 7, 0);
        inner_songs.setCursor(scrolls[i], text_y + 10);

      } else {

        scrolls[i] = 0;
        inner_songs.setCursor(120 - (pxstrlensong / 2.0), text_y + 10);
      }

      inner_songs.print(song_name);

      switch (i) {  // case for specific artis name
        case 3:

          if (S_up == true) {

            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, nd_color, selected_artis_color)), 0);
            inner_songs.setCursor(120 - (pxstrlenartis / 2.0), text_y);
            inner_songs.print(artis_name);
          }

          break;
        case 4:

          if (S_up == false && S_down == false) {
            inner_songs.setTextColor(RGBcolorBGR(convert, selected_artis_color), 0);
            inner_songs.setCursor(120 - (pxstrlenartis / 2.0), text_y);
            inner_songs.print(artis_name);
          } else {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, selected_artis_color, nd_color)), 0);
            inner_songs.setCursor(120 - (pxstrlenartis / 2.0), text_y);
            inner_songs.print(artis_name);
          }

          break;
        case 5:
          if (S_down == true) {
            inner_songs.setTextColor(RGBcolorBGR(convert, colortrans(trans, increment, nd_color, selected_artis_color)), 0);
            inner_songs.setCursor(120 - (pxstrlenartis / 2.0), text_y);
            inner_songs.print(artis_name);
          }
          break;
      }
    }

    inner_songs.pushSprite(0, 29);

    if (S_up == false && S_down == false) {
      break;
    }
  }


  if (S_up == true) {  // puting this here so the img dispaly can looke more respondsive
    index_song--;
    *P_songs_menu_index = index_song;
  }

  if (S_down == true) {
    index_song++;
    *P_songs_menu_index = index_song;
  }

  S_up = false;
  S_down = false;
  text_y = -20;
  w = 0;
}















uint16_t text_img_array[16][240];  //7680 byte not bad
//+4 up and down padding for the bloom, the font is actually 8x5 due to the "g being 1 pixel lower"

uint16_t text_img_array_og[16][240];

void bloomed_text() {
  // note that the adafurit font letter index match with the ascii index

  std::string text = "ABC DEF asdw qw faswed ";

  uint16_t textlen = text.length();
  uint16_t text_pix_len = (textlen * 5) + (textlen - 1) + 8;
  char textarray[textlen];

  String text2 = "ABC DEF asdw qw faswed ";

  // text.copy(text2, textlen, 0);
  text.copy(textarray, textlen, 0);
  memset(text_img_array, 0, sizeof(text_img_array));
  memset(text_img_array_og, 0, sizeof(text_img_array_og));

  //uint16_t text_img[16 * text_pix_len];




  for (uint8_t e = 0; e < textlen; e++) {  // run as long as the letter count

    uint16_t start_index = uint16_t(textarray[e]) * 5;

    for (uint8_t q = 0; q < 5; q++) {  // run as long as how many byte a letter compose of

      uint8_t hex = font[start_index + q];  //0x23 8 bit hex

      for (int8_t i = 7; i > -1; i--) {  //run as long as how many bit its using

        bool extracted_bit = (hex >> i) & 1;

        text_img_array[5 + i][5 + q + (e * 6)] = RGBcolorBGR(convert, 0xfd00) * extracted_bit;
        text_img_array_og[5 + i][5 + q + (e * 6)] = RGBcolorBGR(convert, 0xfd00) * extracted_bit;
      }
    }
  }

  tft.setSwapBytes(true);

  uint16_t* test = &text_img_array[0][0];
  tft.pushImage(0, 0, 240, 16, test);

  delay(3000);



  //doing matrix

  int8_t matrix[3][3] = {
    { 1, 2, 1 },
    { 2, 4, 2 },
    { 1, 2, 1 }
  };

  int8_t bmatrix[5][5] = {
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 }
  };

  int sum = 0;

  uint16_t red;
  uint16_t green;
  uint16_t blue;

  uint8_t bred;
  uint8_t bgreen;
  uint8_t bblue;

  int8_t avg = 16;
  int intens;

  uint16_t output;

  for (int8_t gausian_loop = 0; gausian_loop < 5; gausian_loop++) {
    if (gausian_loop == 0) {
      for (int q = 0; q < 16; q++) {
        for (int w = 0; w < text_pix_len; w++) {
          //===================
          if (text_img_array_og[q + 1][w + 1] != 0) {
            for (int caly = 0; caly < 3; caly++) {
              for (int calx = 0; calx < 3; calx++) {

                text_img_array[q + caly][w + calx] = RGBcolorBGR(convert, 0xc468);

                uint16_t* test2 = &text_img_array[0][0];
                tft.pushImage(0, 0, 240, 16, test2);
              }
            }
          }
          //===================
        }
      }
    } else {
      for (int q = 0; q < 16; q++) {
        for (int w = 0; w < text_pix_len; w++) {
          //===================
          for (int caly = 0; caly < 3; caly++) {
            for (int calx = 0; calx < 3; calx++) {
              if (q + caly > 0 && q + calx > 0) {
                red += ((text_img_array[q + caly][w + calx] & 0xF800) >> 11) * matrix[caly][calx];
                green += ((text_img_array[q + caly][w + calx] & 0x07E0) >> 5) * matrix[caly][calx];
                blue += (text_img_array[q + caly][w + calx] & 0x001F) * matrix[caly][calx];
              }
            }
          }
          //==================

          output = (red / avg) << 11 | (green / avg) << 5 | (blue / avg) << 0;
          text_img_array[q + 1][w + 1] = output;  //RGBcolorBGR(convert, output);

          red = 0;
          green = 0;
          blue = 0;

          uint16_t* test2 = &text_img_array[0][0];
          tft.pushImage(0, 0, 240, 16, test2);
        }
      }
    }
  }


  delay(3000);

  tft.setTextColor(RGBcolorBGR(convert, 0xfcc0));
  tft.setCursor(5, 5);
  tft.println(text2);


  delay(12351476);
}









void gauusian blur() {

 for (int8_t gausian_loop = 0; gausian_loop < 1; gausian_loop++) {
    
    for (int q = 0; q < 16; q++) {
      for (int w = 0; w < text_pix_len; w++) {
        //===================
        for (int caly = 0; caly < 3; caly++) {
          for (int calx = 0; calx < 3; calx++) {

            red += ((text_img_array[q + caly][w + calx] & 0xF800) >> 11) * matrix[caly][calx];
            green += ((text_img_array[q + caly][w + calx] & 0x07E0) >> 5) * matrix[caly][calx];
            blue += (text_img_array[q + caly][w + calx] & 0x001F) * matrix[caly][calx];
          }
        }
        //==================

        output = (red / avg) << 11 | (green / avg) << 5 | (blue / avg) << 0;
        text_img_array[q + 1][w + 1] = output;  //RGBcolorBGR(convert, output);

        //Serial.println(colorbrightness(sum, 16));
        red = 0;
        green = 0;
        blue = 0;
      }
    }

    //uint16_t*


    // delay(3000);
  }
}



/*
bool backbutton() {
  int8_t static pass_count = 0;
  bool static latch = false;
  bool static flip_flop = false;

  if (digitalRead(Back_pin) == HIGH) {
    pass_count++;
  }
  if (pass_count == 1) {
    latch = true;
  } else {
    latch = false;
  }
  if (pass_count >= 3) {
    pass_count = 3;
  }
  if (digitalRead(Back_pin) == LOW && pass_count > 1) {
    delay(100);
    pass_count = 0;
  }

  return latch;
}

//defricated
bool smbutton() {
  int8_t static pass_count = 0;
  bool static latch = false;
  bool static flip_flop = false;
  if (digitalRead(Back_pin) == HIGH) {
    pass_count++;
  }
  if (pass_count == 1) {
    latch = true;
  } else {
    latch = false;
  }
  if (pass_count >= 3) {
    pass_count = 3;
  }
  if (digitalRead(Back_pin) == LOW && pass_count > 1) {
    delay(50);
    pass_count = 0;
  }
  return latch;
}



/// Making FFT


void hann_window(hann_w& H) {
  //float hann[NUM_BYTES_TO_READ_FROM_FILE];
  for (int i = 0; i < NUM_BYTES_TO_READ_FROM_FILE; i++) {
    H.hann_win[i] = 0.5 * (1 - cos(2 * PI * i / (NUM_BYTES_TO_READ_FROM_FILE - 1)));
  }
}


void gettingL(byte* sample){

  

}




















*/

