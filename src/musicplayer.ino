#include <musicplayer.h>
#include <Wire.h>
#include <stdint.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>
#include <string>

#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>
#include <NfcAdapter.h>

SPIClass spi = SPIClass(VSPI);
PN532_SPI pn532_spi(spi, SS);
NfcAdapter nfc = NfcAdapter(pn532_spi);

// WiFi Setup
WiFiManager wifiManager;

// MQTT Setup
WiFiClient espClient;
PubSubClient client(espClient);
const char *willMessageOffline = "off";
const char *willMessageOnline = "on";

// setup topic name
String mqttWillTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(will_topic);
String mqttPlayTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(play_topic);
#ifdef WRITE_MODE
String mqttWriteModeTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(write_mode_topic);
String mqttWriteValueTopic = String(topic_prefix) + "/" + String(device_id) + "/" + String(write_value_topic);
#endif

// Buffers
uint8_t success;          // Flag to check if there was an error with the PN532
uint8_t isAlreadyPresent; // Flag to check if a tag was already present
byte charBuffer[MQTT_MAX_PACKET_SIZE];

// Buffers for write mode
#ifdef WRITE_MODE
boolean writeMode;
String writeValue;
#endif

void setup_PCD()
{
    nfc.begin();
}

void setup_wifi()
{
    // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;
    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    res = wm.autoConnect("AutoConnectAP"); // anonymous ap
    // res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap
    if (!res)
    {
        Serial.println("Failed to connect");
        ESP.restart();
    }
    else
    {
        // if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
    }
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void mqttconnect()
{
    /* Loop until reconnected */
    while (!client.connected())
    {
        Serial.print("MQTT connecting ...");
        /* client ID */
        String clientId = device_id;
        /* connect now */
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_password, mqttWillTopic.c_str(), 1, 1, willMessageOffline))
        {
            Serial.println("connected");
            /* subscribe topic with default QoS 0*/
            // turn on power light if mqtt connected
            digitalWrite(PWR_PIN, HIGH);
            delay(100);
            client.publish(mqttWillTopic.c_str(), willMessageOnline);
#ifdef WRITE_MODE
            client.subscribe(mqttWriteModeTopic.c_str());
            client.subscribe(mqttWriteValueTopic.c_str());
#endif
        }
        else
        {
            Serial.print("failed, status code =");
            Serial.print(client.state());
            Serial.println("try again in 5 seconds");
            /* Wait 5 seconds before retrying */
            // blink power light if mqtt disconnected
            digitalWrite(PWR_PIN, LOW);
            delay(5000);
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    payload[length] = '\0';
    String value = String((char *)payload);

#ifdef WRITE_MODE
    if (mqttWriteModeTopic.equals(topic))
    {
        if (value == "on")
        {
            writeMode = true;
            digitalWrite(WRT_PIN, HIGH);
        }
        else
        {
            writeMode = false;
            digitalWrite(WRT_PIN, LOW);
        }
    }

    if (mqttWriteValueTopic.equals(topic))
    {
        writeValue = value;
    }
#endif
}

void setup()
{
    Serial.begin(115200); // Initialize serial communications with the PC for debugging.
    while (!Serial)
        ; // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4).

    pinMode(PWR_PIN, OUTPUT);
    pinMode(WRT_PIN, OUTPUT);
    pinMode(BZR_PIN, OUTPUT);

    // turn on power light
    digitalWrite(PWR_PIN, HIGH);

    setup_PCD();
    setup_wifi();
}

void loop()
{
    if (!client.connected())
    {
        mqttconnect();
    }
    client.loop();

    if (!nfc.tagPresent())
    {
        isAlreadyPresent = false;
        delay(50);
        return;
    }

    // if we have a card present just ignore it
    if (isAlreadyPresent)
    {
        return;
    }

    isAlreadyPresent = true;

#ifdef WRITE_MODE
    if (writeMode)
    {
        digitalWrite(WRT_PIN, LOW);
        myTone(BZR_PIN);
        /* Call 'WriteURLToTag' function, which will write data to the block */
        WriteURLToTag();
        digitalWrite(WRT_PIN, HIGH);
        myNoTone(BZR_PIN);
    }
    else
    {
#endif

        digitalWrite(WRT_PIN, HIGH);
        myTone(BZR_PIN);
        /* Read data from the same block */
        ReadAndPublishFromTag();
        digitalWrite(WRT_PIN, LOW);
        myNoTone(BZR_PIN);

#ifdef WRITE_MODE
    }
#endif
}

#ifdef WRITE_MODE
void WriteURLToTag()
{
    // format to NDEF, will fail if already an NDEF
    nfc.format();
    NdefMessage message = NdefMessage();
    message.addHTTPSUriRecord(writeValue);
    success = nfc.write(message);
    if (success)
    {
        Serial.println("Success. Try reading this tag with your phone.");
    }
    else
    {
        Serial.println("Write failed.");
    }
}
#endif

void ReadAndPublishFromTag()
{
    NfcTag tag = nfc.read();
    if (tag.hasNdefMessage())
    {
        NdefMessage message = tag.getNdefMessage();
        int recordCount = message.getRecordCount();
        for (int i = 0; i < recordCount; i++)
        {
            NdefRecord record = message.getRecord(i);
            if (record.getTnf() == TNF_WELL_KNOWN)
            {
                // The TNF and Type should be used to determine how your application processes the payload
                // There's no generic processing for the payload, it's returned as a byte[]
                int payloadLength = record.getPayloadLength();
                byte payload[payloadLength];
                record.getPayload(payload);

                // Force the data into a String (might work depending on the content)
                // Real code should use smarter processing
                String string = "";
                for (int c = 1; c < payloadLength; c++)
                {
                    string += (char)payload[c];
                }
                success = client.publish(mqttPlayTopic.c_str(), string.c_str());
                if (!success)
                {
                    Serial.println("failed to publish mqtt message");
                }
                else
                {
                    Serial.print("Successfuly published: ");
                    Serial.println(string.c_str());
                    Serial.print("To: ");
                    Serial.println(mqttPlayTopic.c_str());
                }
                return;
            }
        }
    }
}

void BlinkLEDFromHigh(uint8_t pin, uint8_t final)
{
    digitalWrite(pin, LOW);
    delay(100);
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, final);
}

void BlinkLEDFromLow(uint8_t pin, uint8_t final)
{
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    delay(100);
    digitalWrite(pin, final);
}

void myTone(int pin)
{
    ledcAttachPin(pin, 0);       // pin, channel
    ledcWriteNote(0, NOTE_C, 6); // channel, frequency, octave
}

void myNoTone(int pin)
{
    ledcDetachPin(pin);
}