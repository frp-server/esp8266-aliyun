#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SHA256.h>
#include <ArduinoJson.h>

#define MQTT_PORT (1883)
#define SHA256HMAC_SIZE (32)

#define ALINK_CLIENTID_FORMAT "%s.%s|securemode=2,signmethod=hmacsha256,timestamp=%s|"
#define ALINK_POST_FORMAT "{\"method\":\"thing.service.property.post\",\"params\":{\"%s\":%s,\"%s\":\"%s\"},\"version\":\"1.0.0\"}"
/* {
  "ProductKey": "a12ZL0Z9m6r",
  "DeviceName": "oCufiU9iHKMLPKUUXYEw",
  "DeviceSecret": "0eccaf08368bbc8f95d29ddcd66cca08"
} */

const char *mqtt_server = "a12ZL0Z9m6r.iot-as-mqtt.cn-shanghai.aliyuncs.com";
const char *productKey = "a12ZL0Z9m6r";
const char *deviceName = "oCufiU9iHKMLPKUUXYEw";
const char *deviceSecret = "0eccaf08368bbc8f95d29ddcd66cca08";

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastMsg = 0;

void wifiInit() {
  delay(10);
  Serial.println("\r\nEsp8266 Connecting to WiFi");
  WiFi.begin();
  for (int i = 0; i < 5; i++) {  //5s use autoConfig Connecting to WiFi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.print(".");
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
    } else {
      Serial.println("\r\nAutoConfig Success");
      Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
      Serial.printf("PWD:%s\r\n", WiFi.psk().c_str());
      break;
    }
  }
  if (WiFi.status() != WL_CONNECTED) {
    smartConfig();
    delay(3000);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    for (int i = 0; i < 5; i++) {  //WiFi connected,led flashing 5 times
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
  } else {
    Serial.println("\r\nWiFi connection failed");
    Serial.println("Please restart and try again");
    digitalWrite(LED_BUILTIN, LOW);  //WiFi connection failed,led on
  }
}
void smartConfig() {
  Serial.println("\nWiFi connection failed");
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  Serial.println("\nWaiting for SmartConfig");
  while (1) {
    Serial.print(".");
    if (WiFi.smartConfigDone()) {
      Serial.println("");
      Serial.println("SmartConfig received");
      Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
      Serial.printf("PWD:%s\r\n", WiFi.psk().c_str());
      WiFi.setAutoConnect(true);
      break;
    }
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
  }
}


void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char temp_input[length + 1] = "";
  for (int i = 0; i < length; i++) {
    temp_input[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.println("");
  Serial.println("************************");
  parsingdata(temp_input);
  Serial.println("************************");
  Serial.println("");
}

void parsingdata(char *input) {
  // Stream& input;

  StaticJsonDocument<256> doc;

  DeserializationError error = deserializeJson(doc, input);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  int params_PowerSwitch = doc["params"]["PowerSwitch"];  // 0
  const char *params_Room = doc["params"]["Room"];        // "100133"
  Serial.print("PowerSwitch:");
  String powerStatus = params_PowerSwitch ? "ON" : "OFF";
  Serial.print(powerStatus);
  doled(params_PowerSwitch);

  Serial.print("  Room:");
  Serial.print(params_Room);
  Serial.println("");
}

void doled(int p) {
  if (p == 1) {
    //LED ON
    digitalWrite(BUILTIN_LED, LOW);
  } else {
    //LED OFF
    digitalWrite(BUILTIN_LED, HIGH);
  }
}

String hmac256(const String &signcontent, const String &ds) {
  byte hashCode[SHA256HMAC_SIZE];
  SHA256 sha256;

  const char *key = ds.c_str();
  size_t keySize = ds.length();

  sha256.resetHMAC(key, keySize);
  sha256.update((const byte *)signcontent.c_str(), signcontent.length());
  sha256.finalizeHMAC(key, keySize, hashCode, sizeof(hashCode));

  String sign = "";
  for (byte i = 0; i < SHA256HMAC_SIZE; ++i) {
    sign += "0123456789ABCDEF"[hashCode[i] >> 4];
    sign += "0123456789ABCDEF"[hashCode[i] & 0xf];
  }

  return sign;
}

void reconnect() {
  //Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID

    long times = millis();
    String timestamp = String(times);
    char mqtt_clientId[256] = "";
    sprintf(mqtt_clientId, ALINK_CLIENTID_FORMAT, productKey, deviceName, timestamp.c_str());

    char mqtt_user[100] = "";
    sprintf(mqtt_user, "%s&%s", deviceName, productKey);

    String signcontent = "clientId";
    signcontent += productKey;
    signcontent += ".";
    signcontent += deviceName;
    signcontent += "deviceName";
    signcontent += deviceName;
    signcontent += "productKey";
    signcontent += productKey;
    signcontent += "timestamp";
    signcontent += timestamp;
    String pwd = hmac256(signcontent, deviceSecret);
    char mqtt_pwd[100] = "";
    strcpy(mqtt_pwd, pwd.c_str());

    // Attempt to connect
    if (client.connect(mqtt_clientId, mqtt_user, mqtt_pwd)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(12000);
    }
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);
  wifiInit();

  client.setServer(mqtt_server, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    pubpost();
  }
}
void pubpost() {
  char topic[100] = "";
  char msg[100] = "";
  sprintf(topic, "/sys/%s/%s/thing/event/property/post", productKey, deviceName);
  sprintf(msg, ALINK_POST_FORMAT, "PowerSwitch", String(!digitalRead(BUILTIN_LED)), "Room", "1024");
  Serial.print("Message send [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.print(msg);
  Serial.println("");
  client.publish(topic, msg);
}
