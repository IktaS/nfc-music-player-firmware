#include <stdint.h>

// Uncomment the next line to enable WRITE_MODE features
#define WRITE_MODE

// MQTT topic stuff
// Name of the json config file in SPIFFS.
#ifdef WRITE_MODE
const char *const write_mode_topic = "write_mode";
const char *const write_value_topic = "write_value";
#endif
const char *const play_topic = "play";
const char *const will_topic = "status";
const char *const topic_prefix = "homeassistant/music-player";
const char *const device_id = "38beb38e-5b9e-4bf5-b8b1-dd414cdae9fd";

// MQTT Stuff
const char *const mqtt_server = "192.168.18.52";
const uint16_t mqtt_port = 1883;
const char *const mqtt_user = "mosquitto";
const char *const mqtt_password = "mosquitto-client";

// RC522 Setup
#define SS_PIN 5
#define RST_PIN 21

// PN532 Setup
#define SCK_PIN 35
#define MOSI_PIN 36
#define MISO_PIN 19

// Lights and Buzzer Stuffs
#define PWR_PIN 27
#define WRT_PIN 26
#define BZR_PIN 25

#define BZR_TONE 3000