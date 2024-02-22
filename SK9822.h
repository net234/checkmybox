/************************************
    SK9822  rgb serial led driver type SK9822 ou APA102

    Copyright 20201 Pierre HENRY net23@frdev.com

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

  from WS2812 V1.1 (05/11/2021)
   - Adjust for RVBW

  SK9822 V1.0 20/01/2023

*/
#pragma once

#include <Arduino.h>
//#include "Nano.h"
#include "ESP8266.h"

extern uint8_t SK9822Level;

// il faut definir 2 sorties pour ces bandeau (en general dans ESP8266.h)
//#define PIN_DataSK9822 D2
//#define PIN_ClkSK9822 D3

enum e_rvb  { rvb_white, rvb_red, rvb_green, rvb_blue, rvb_yellow, rvb_pink, rvb_brown, rvb_orange, rvb_lightblue, rvb_lightgreen,  rvb_purple, rvb_black, rvb_superWhite, MAX_e_rvb };




struct  rvb_t {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};



struct  rvbLed: rvb_t {
  int16_t decDelay;
  int16_t incDelay;
  int16_t baseDecDelay;
  int16_t baseIncDelay;
  uint8_t maxLevel;
  uint8_t color;
  void  setcolor(const e_rvb color, const uint8_t level, const uint16_t increase = 0, const uint16_t decrease = 0);
  void  anime(const uint8_t delta);

};

// pour homogeniser la luminosité la somme des couleur avoisine 250
const rvb_t map_color[MAX_e_rvb] = {
  {100, 100, 100}, // rvb_white
  {255,   0,   0}, // rvb_red
  {  0, 255,   0}, // rvb_green
  {  0,   0, 255}, // rvb_blue
  {150, 100,   0}, // rvb_yellow
  {200,  50,  50}, // rvb_pink
  {153,  71,   8}, // rvb_brown
  {200,  50,   0}, // rvb_orange
  { 50,  50, 200}, // rvb_lightblue
  { 50, 200,  50}, // rvb_lightgreen
  {150,   0, 150}, // rvb_purple
  {  0,   0,   0}, // rvb_black
  {255, 255, 200}, // rvb_superWhite
};


struct SK9822rvb_t : rvbLed {
  void  start();
  void  stop();
  void  write();
  void shift(uint8_t color);
};
