#include <NTPClient.h>
#include "DHTesp.h"
#include <WiFiUdp.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <secrets.h>

// This project needs a file called secrets.h where all the private stuff goes

DHTesp dht;

UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);

WiFiUDP ntpUDP;
// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionally you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", -10800, 60000);

void asyncCB(AsyncResult &aResult);

void printResult(AsyncResult &aResult);

DefaultNetwork network; // initilize with boolean parameter to enable/disable network reconnection

FirebaseApp app;

WiFiClientSecure ssl_client;

using AsyncClient = AsyncClientClass;

AsyncClient aClient(ssl_client, getNetwork(network));

Firestore::Documents Docs;

float humidity;
float temperature;

unsigned long previousMillis = 0;
unsigned long documentPreviousMillis = 0;
const unsigned int documentCreationInterval = 3000;

void setup()
{

  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  Serial.println("Initializing app...");

#if defined(ESP32) || defined(ESP8266) || defined(PICO_RP2040)
  ssl_client.setInsecure();
#if defined(ESP8266)
  ssl_client.setBufferSizes(4096, 1024);
#endif
#endif

  initializeApp(aClient, app, getAuth(user_auth), asyncCB, "authTask");

  app.getApp<Firestore::Documents>(Docs);

  dht.setup(D0, DHTesp::DHT22); // Connect DHT sensor to GPIO 17
}

void loop()
{
  // The async task handler should run inside the main loop
  // without blocking delay or bypassing with millis code blocks.
  app.loop();
  Docs.loop();
  timeClient.update();

  if (previousMillis < millis() - 5000)
  {
    previousMillis = millis();

    humidity = dht.getHumidity();
    temperature = dht.getTemperature();

    Serial.print(dht.getStatusString());
    Serial.print("\t");
    Serial.print(humidity, 1);
    Serial.print("\t\t");
    Serial.print(temperature, 1);
    Serial.print("\t\t");

    Serial.println(timeClient.getEpochTime());
  }

  if (app.ready() && documentPreviousMillis < millis() - documentCreationInterval)
  {
    documentPreviousMillis = millis();

    String documentPath = "proyecto/" + String(timeClient.getEpochTime());

    // double
    Values::DoubleValue temperatureValue(temperature);
    Values::DoubleValue humidityValue(humidity);

    Document<Values::Value> doc("temperatura", Values::Value(temperatureValue));
    doc.add("humedad", Values::Value(humidityValue));

    Serial.println("Create document... ");

    Docs.createDocument(aClient, Firestore::Parent(FIREBASE_PROJECT_ID), documentPath, DocumentMask(), doc, asyncCB, "task de creacion de documento");
  }
}

void asyncCB(AsyncResult &aResult)
{
  // WARNING!
  // Do not put your codes inside the callback and printResult.

  printResult(aResult);
}

void printResult(AsyncResult &aResult)
{
  if (aResult.isEvent())
  {
    Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.appEvent().message().c_str(), aResult.appEvent().code());
  }

  if (aResult.isDebug())
  {
    Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  }

  if (aResult.isError())
  {
    Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  }

  if (aResult.available())
  {
    Firebase.printf("task: %s", aResult.uid().c_str());
    //, payload : % s\n ", aResult.uid().c_str(), aResult.c_str());
  }
}
