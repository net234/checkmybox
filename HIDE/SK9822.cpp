//#include <sys/_stdint.h>
/************************************
    SK9822  rgb serial led driver

    Copyright 2020 Pierre HENRY net23@frdev.com

    SK9822 is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SK9822 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with betaEvents.  If not, see <https://www.gnu.org/licenses/lglp.txt>.


    A reset is issued as early as at 9 µs???, contrary to the 50 µs mentioned in the data sheet. Longer delays between transmissions should be avoided.
    On tested componant 50µs reset is a mandatory
    The cycle time of a bit should be at least 1.25 µs, the value given in the data sheet, and at most ~50 µs, the shortest time for a reset.
    A “0” can be encoded with a pulse as short as 62.5 ns, but should not be longer than ~500 ns (maximum on WS2812).
    A “1” can be encoded with pulses almost as long as the total cycle time, but it should not be shorter than ~625 ns (minimum on WS2812B).
    from https://cpldcpu.wordpress.com/2014/01/14/light_ws2812-library-v2-0-part-i-understanding-the-ws2812/

    https://roboticsbackend.com/arduino-fast-digitalwrite/

   V1.1 (05/11/2021)
   - Adjust for RVBW
   V1.2  (01/10/2022)
   - Add code for ESP8266


*/


#include "SK9822.h"

uint8_t SK9822Level = 25;    // 25% de charge au boot

#if defined(__AVR__)
#error "ESP8266  uniquement"
/**************************************
ecriture rapide des GPIO pour AVR (NANO ou TINY)
***************************************/



#define MSK_WS2812 (1 << (PIN_WS2812 - 8))  //N° pin portB donc D8..D13 sur nano
#define PORT_WS2812 PORTB

//inline void WS2812_LOW() __attribute__((always_inline));
//inline void WS2812_HIGH() __attribute__((always_inline));


void WS2812_LOW() {
  PORT_WS2812 &= ~MSK_WS2812;
}

void WS2812_HIGH() {
  PORT_WS2812 |= MSK_WS2812;
}


#elif defined(ESP8266)                          //|| defined(ESP32)
/**************************************
ecriture rapide des GPIO pour ESP8266
***************************************/
#define MSK_ClkSK9822 (1 << (ClkSK9822_PIN))    //ESP8266   N° pin 0 a 15 uniquement
#define MSK_DataSK9822 (1 << (DataSK9822_PIN))  //ESP8266   N° pin 0 a 15 uniquement




inline void ClkSK9822_LOW() {
  GPOC = MSK_ClkSK9822;
}

inline void ClkSK9822_HIGH() {
  GPOS = MSK_ClkSK9822;
}

inline void DataSK9822_LOW() {
  GPOC = MSK_DataSK9822;
}

inline void DataSK9822_HIGH() {
  GPOS = MSK_DataSK9822;
}

#endif











void SK9822rvb_t::start() {

  DataSK9822_LOW();
  for (byte N = 0; N < 32; N++) {
    ClkSK9822_HIGH();
    ClkSK9822_LOW();
  }
}

void SK9822rvb_t::stop() {

  DataSK9822_HIGH();
  for (byte N = 0; N < 32; N++) {
    ClkSK9822_HIGH();
    ClkSK9822_LOW();
  }
}



//timing for nano
#if defined(ESP8266)  //|| defined(ESP32)
//void IRAM_ATTR WS2812rvb_t::shift(uint8_t shift) {
void SK9822rvb_t::shift(uint8_t shift) {
  for (byte n = 8; n > 0; n-- ) {
    if (shift & 0x80) {
      DataSK9822_HIGH();  //0,3µs
    } else {
      DataSK9822_LOW();
    }  //0,3µs
    ClkSK9822_LOW();
    shift = shift << 1;
    ClkSK9822_HIGH();
  }
}

#endif

//void IRAM_ATTR WS2812rvb_t::write() {
void SK9822rvb_t::write() {
  uint8_t level = (int)31 * SK9822Level / 100;
  //uint8_t level = 0x1F;
  level |= 0b11100000;  
  shift(level);
  shift(blue);
  shift(green);
  shift(red);
}



void rvbLed::setcolor(const e_rvb acolor, const uint8_t alevel, const uint16_t increase, const uint16_t decrease) {
  //maxLevel =  (uint16_t)alevel * SK9822Level / 100;
  maxLevel = min(alevel,(uint8_t)100);
  color = acolor;
  if (increase == 0) {
    red = (uint16_t)map_color[color].red * maxLevel / 100;
    green = (uint16_t)map_color[color].green * maxLevel / 100;
    blue = (uint16_t)map_color[color].blue * maxLevel / 100;
  } else {
    red = 0;
    green = 0;
    blue = 0;
  }

  baseIncDelay = increase;
  incDelay = increase;
  baseDecDelay = decrease;
  decDelay = decrease;
}



void rvbLed::anime(const uint8_t delta) {
  if (incDelay > 0) {
    if (incDelay > delta) {
      incDelay -= delta;
    } else {
      incDelay = 0;
    }
    // increment
    uint16_t curLevel = (uint16_t)maxLevel - ((uint32_t)maxLevel * incDelay / baseIncDelay);
    //    Serial.print('I');
    //    Serial.println(curLevel);
    red = (uint16_t)map_color[color].red * curLevel / 100;
    green = (uint16_t)map_color[color].green * curLevel / 100;
    blue = (uint16_t)map_color[color].blue * curLevel / 100;
    return;
  }

  if (decDelay > 0) {
    if (decDelay > delta) {
      decDelay -= delta;
    } else {
      decDelay = 0;
    }
    // decrem
    uint16_t curLevel = (uint32_t)maxLevel * decDelay / baseDecDelay;
    //Serial.print('D');
    //Serial.println(curLevel);
    //
    red = (uint16_t)map_color[color].red * curLevel / 100;
    green = (uint16_t)map_color[color].green * curLevel / 100;
    blue = (uint16_t)map_color[color].blue * curLevel / 100;
    return;
  }
}
