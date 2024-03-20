// definition de IO pour beta four

#define __BOARD__ESP8266__

#define LED_ON LOW



//Pin out NODEMCU ESP8266
#define D0  16    //!LED_BUILTIN (OLD)
#define D1  5     //       I2C_SCL
#define D2  4     //       I2C_SDA
#define D3  0     //!FLASH    BEEP_PIN
#define D4  2     //!LED2     !LED_BUILTIN (NEW)

#define D5  14    //!SPI_CLK    DOORLOCK_PIN Entrée porte verouillée
#define D6  12    //!SPI_MISO   GACHE_PIN
#define D7  13    //!SPI_MOSI   BP0_PIN
#define D8  15    //!BOOT_STS            

#define BP0_PIN   D3                 //  High to Low = will wleep in, 5 minutes 
#define LED0_PIN  LED_BUILTIN   //   By default Led0 is on LED_BUILTIN should be 2 (D4)
#define BP1_PIN D6            // Bouton Libre BP1

//#define I2C_SDA  D2
//#define I2C_SCL  D1
//#define LED_ON  HIGH

//#define WS2812_PIN D7  //Uniquement D8..D13
#define ClkSK9822_PIN D8
#define DataSK9822_PIN D7

//bNode01
#define ONEWIRE_PIN D4  //SHARE LED BUILTIN !!!
#define BEEP_PIN D3     //SHARE BP0 (FLASH)
