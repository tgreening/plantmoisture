#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <ThingSpeak.h>
#include <ArduinoOTA.h>

char thingSpeakAPIKey[17];
char thingSpeakChannelID[7];

const long POST_MILLIS = 3600000UL * 12; //post every 12 hours
const int moistureSensor = A0;

//flag for saving data
bool shouldSaveConfig = false;
long lastReadingTime = 0;
const char* thingSpeakserver = "api.thingspeak.com";
const char* host = "plantmoisture";
long nextPost;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
void setup() {
  // Serial Begin so we can see the data from the mosture sensor in our serial input window.
  Serial.begin(115200);
  WiFiManager wifiManager;
  // setting the led pins to outp
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        DynamicJsonDocument json(1024);
        DeserializationError error = deserializeJson(json, configFile);
        if (!error) {
          Serial.println("\nparsed json");
          strlcpy(thingSpeakAPIKey, json["ThingSpeakWriteKey"] | "Any Key", sizeof(thingSpeakAPIKey));
          strlcpy(thingSpeakChannelID, json["ThingSpeakChannelID"] | "12345", sizeof(thingSpeakChannelID));
        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  WiFiManagerParameter custom_thingspeak_api_key("thingspeakapikey", "Thingspeak API key", thingSpeakAPIKey, 40);
  wifiManager.addParameter(&custom_thingspeak_api_key);
  Serial.print("Thingspeak Write key: ");
  Serial.println(thingSpeakAPIKey);

  WiFiManagerParameter custom_thingspeak_channel_id("thingspeakchannelid", "Thingsspeak Channel ID", thingSpeakChannelID, 7);
  wifiManager.addParameter(&custom_thingspeak_channel_id);
  Serial.print("Thingspeak Channel ID: ");
  Serial.println(thingSpeakChannelID);

  WiFi.hostname(String(host));
  wifiManager.setConfigPortalTimeout(30);
  if (!wifiManager.startConfigPortal(host)) {
  //if (!wifiManager.autoConnect(host)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  WiFi.mode(WIFI_STA);
  //read updated parameters
  strcpy(thingSpeakAPIKey, custom_thingspeak_api_key.getValue());
  strcpy(thingSpeakChannelID, custom_thingspeak_channel_id.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonDocument json(1024);
    json["ThingSpeakWriteKey"] = thingSpeakAPIKey;
    json["ThingSpeakChannelID"] = thingSpeakChannelID;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    } else {

      serializeJson(json, configFile);
      configFile.close();
    }
    //end save
  }
  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  pinMode(moistureSensor, INPUT);
}

// the loop routine runs over and over again forever:
void loop() {
  if (millis() > nextPost) {
    int sensorValue = analogRead(moistureSensor);
    postReading(sensorValue, thingSpeakAPIKey, thingSpeakChannelID);
    nextPost = millis() + POST_MILLIS;
  }
  // Power-down the probe
  ArduinoOTA.handle();
}

void postReading(int reading,  char* APIKey, char* channelID) {
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFiClient client;
  ThingSpeak.begin(client);  // Initialize ThingSpeak
  Serial.println("About to post!");
  Serial.print("Channel: ");
  Serial.println(channelID);
  Serial.print("API Key: ");
  Serial.println(APIKey);
  ThingSpeak.setField(1, reading);
  int x = ThingSpeak.writeFields(atoi(channelID), APIKey);
  Serial.println(x);
}
