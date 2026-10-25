
// LIBRARIES

#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_TMP117.h>
#include <Adafruit_MPU6050.h>

#include <MAX30105.h>
#include "spo2_algorithm.h"

#include <SparkFun_u-blox_GNSS_Arduino_Library.h>

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


// WIFI CONFIGURATION

#define WIFI_SSID "WIFI_SSID"

#define WIFI_PASSWORD "WIFI_PASSWORD"


// ================================================================
// FIREBASE CONFIGURATION
// ================================================================

#define API_KEY "YOUR_FIREBASE_WEB_API_KEY"

#define DATABASE_URL "YOUR_FIREBASE_REALTIME_DATABASE_URL"

#define USER_EMAIL "YOUR_FIREBASE_AUTH_EMAIL"

#define USER_PASSWORD "YOUR_FIREBASE_AUTH_PASSWORD"


// ================================================================
// DEVICE ID
// ================================================================

#define DEVICE_ID "SMART_BAND_001"


// ================================================================
// GPIO CONFIGURATION
// ================================================================

// Shared I2C

#define I2C_SDA_PIN 8

#define I2C_SCL_PIN 9


// TMP117

#define TMP117_INT_PIN 10


// MPU6050

#define MPU6050_INT_PIN 16


// MAX30102

#define MAX30102_INT_PIN 40


// GPS

#define GPS_RX_PIN 2

#define GPS_TX_PIN 1

#define GPS_BAUD_RATE 9600


// AD8232

#define AD8232_OUTPUT_PIN 4

#define AD8232_LO_PLUS_PIN 41

#define AD8232_LO_MINUS_PIN 42

#define AD8232_SDN_PIN 21


// ================================================================
// TIME INTERVALS
// ================================================================

// 5 Minutes

const unsigned long MEASUREMENT_INTERVAL = 300000UL;


// Fall Detection Every 100 ms

const unsigned long FALL_CHECK_INTERVAL = 100UL;


// GPS Every 1 Second

const unsigned long GPS_CHECK_INTERVAL = 1000UL;


// ECG Every 10 ms

const unsigned long ECG_CHECK_INTERVAL = 10UL;


// WiFi Reconnect Every 10 Seconds

const unsigned long WIFI_CHECK_INTERVAL = 10000UL;


// ================================================================
// NTP TIME CONFIGURATION
// ================================================================

// India UTC +5:30

const long GMT_OFFSET_SECONDS = 19800;

const int DAYLIGHT_OFFSET_SECONDS = 0;


const char* NTP_SERVER_1 = "pool.ntp.org";

const char* NTP_SERVER_2 = "time.nist.gov";


// ================================================================
// SENSOR OBJECTS
// ================================================================

Adafruit_TMP117 tmp117;

Adafruit_MPU6050 mpu;

MAX30105 max30102;

SFE_UBLOX_GNSS gps;


// ================================================================
// GPS SERIAL
// ================================================================

HardwareSerial GPSSerial(1);


// ================================================================
// FIREBASE OBJECTS
// ================================================================

FirebaseData fbdo;

FirebaseAuth auth;

FirebaseConfig config;


// ================================================================
// SENSOR AVAILABILITY
// ================================================================

bool tmp117Available = false;

bool mpu6050Available = false;

bool max30102Available = false;

bool gpsAvailable = false;


// ================================================================
// TIMING VARIABLES
// ================================================================

unsigned long previousMeasurementTime = 0;

unsigned long previousFallCheckTime = 0;

unsigned long previousGPSCheckTime = 0;

unsigned long previousECGCheckTime = 0;

unsigned long previousWiFiCheckTime = 0;


// ================================================================
// TEMPERATURE DATA
// ================================================================

float temperatureC = 0.0;

String temperatureStatus = "Unknown";


// ================================================================
// HEART RATE DATA
// ================================================================

float heartRateBPM = 0.0;

String heartRateStatus = "Unknown";


// ================================================================
// SPO2 DATA
// ================================================================

float spo2Value = 0.0;

String spo2Status = "Unknown";


// ================================================================
// MAX30102 DATA
//
// IMPORTANT:
//
// Do NOT name this BUFFER_SIZE.
//
// spo2_algorithm.h already defines BUFFER_SIZE.
// ================================================================

const int MAX30102_SAMPLE_COUNT = 100;


uint32_t irBuffer[MAX30102_SAMPLE_COUNT];

uint32_t redBuffer[MAX30102_SAMPLE_COUNT];


int32_t calculatedSpO2 = 0;

int8_t validSpO2 = 0;


int32_t calculatedHeartRate = 0;

int8_t validHeartRate = 0;


// ================================================================
// FALL DETECTION DATA
// ================================================================

float accelerationX = 0.0;

float accelerationY = 0.0;

float accelerationZ = 0.0;

float totalAcceleration = 0.0;


bool freeFallDetected = false;

bool fallDetected = false;


unsigned long freeFallStartTime = 0;

unsigned long fallDetectedTime = 0;


// ================================================================
// GPS DATA
// ================================================================

double latitude = 0.0;

double longitude = 0.0;

bool locationValid = false;


// ================================================================
// ECG DATA
// ================================================================

int ecgValue = 0;

bool ecgLeadsConnected = false;


// ================================================================
// FUNCTION DECLARATIONS
// ================================================================

void connectWiFi();

void reconnectWiFi();

void initializeTime();

void initializeFirebase();

void initializeSensors();


void performMeasurementCycle();


void readTemperature();

void readHeartRateAndSpO2();

void readMPU6050();

void processFallDetection();

void readGPS();

void readECG();


void classifyVitalSigns();

void displaySensorData();


void uploadRealtimeData();

void uploadHistoryData();

void uploadDeviceStatus();

void uploadFallAlert();


String getCurrentDate();

String getCurrentTime();

String getTimestamp();


// ================================================================
// SETUP
// ================================================================

void setup()
{
  Serial.begin(115200);


  delay(2000);


  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "SMART HEALTH MONITORING BAND"
  );

  Serial.println(
    "========================================"
  );


  // --------------------------------------------------------------
  // INTERRUPT PINS
  // --------------------------------------------------------------

  pinMode(
    TMP117_INT_PIN,
    INPUT
  );


  pinMode(
    MPU6050_INT_PIN,
    INPUT
  );


  pinMode(
    MAX30102_INT_PIN,
    INPUT
  );


  // --------------------------------------------------------------
  // SHARED I2C
  // --------------------------------------------------------------

  Wire.begin(
    I2C_SDA_PIN,
    I2C_SCL_PIN
  );


  Wire.setClock(400000);


  // --------------------------------------------------------------
  // WIFI
  // --------------------------------------------------------------

  connectWiFi();


  // --------------------------------------------------------------
  // TIME
  // --------------------------------------------------------------

  initializeTime();


  // --------------------------------------------------------------
  // FIREBASE
  // --------------------------------------------------------------

  initializeFirebase();


  // --------------------------------------------------------------
  // SENSORS
  // --------------------------------------------------------------

  initializeSensors();


  Serial.println();

  Serial.println(
    "INITIALIZATION COMPLETE"
  );


  // --------------------------------------------------------------
  // FIRST MEASUREMENT
  // --------------------------------------------------------------

  performMeasurementCycle();


  previousMeasurementTime = millis();
}


// ================================================================
// MAIN LOOP
// ================================================================

void loop()
{
  unsigned long currentMillis = millis();


  // --------------------------------------------------------------
  // WIFI RECONNECTION
  // --------------------------------------------------------------

  if (
    WiFi.status() != WL_CONNECTED
    &&
    currentMillis - previousWiFiCheckTime
    >=
    WIFI_CHECK_INTERVAL
  )
  {
    previousWiFiCheckTime = currentMillis;


    reconnectWiFi();
  }


  // --------------------------------------------------------------
  // FALL DETECTION
  // --------------------------------------------------------------

  if (
    mpu6050Available
    &&
    currentMillis - previousFallCheckTime
    >=
    FALL_CHECK_INTERVAL
  )
  {
    previousFallCheckTime = currentMillis;


    readMPU6050();


    processFallDetection();
  }


  // --------------------------------------------------------------
  // GPS
  // --------------------------------------------------------------

  if (
    gpsAvailable
    &&
    currentMillis - previousGPSCheckTime
    >=
    GPS_CHECK_INTERVAL
  )
  {
    previousGPSCheckTime = currentMillis;


    readGPS();
  }


  // --------------------------------------------------------------
  // ECG
  // --------------------------------------------------------------

  if (
    currentMillis - previousECGCheckTime
    >=
    ECG_CHECK_INTERVAL
  )
  {
    previousECGCheckTime = currentMillis;


    readECG();
  }


  // --------------------------------------------------------------
  // 5-MINUTE MEASUREMENT
  // --------------------------------------------------------------

  if (
    currentMillis - previousMeasurementTime
    >=
    MEASUREMENT_INTERVAL
  )
  {
    previousMeasurementTime = currentMillis;


    performMeasurementCycle();
  }


  delay(5);
}


// ================================================================
// WIFI CONNECTION
// ================================================================

void connectWiFi()
{
  Serial.println();

  Serial.print(
    "Connecting to WiFi"
  );


  WiFi.mode(WIFI_STA);


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  unsigned long startTime = millis();


  while (
    WiFi.status() != WL_CONNECTED
    &&
    millis() - startTime < 20000
  )
  {
    Serial.print(".");


    delay(500);
  }


  Serial.println();


  if (
    WiFi.status() == WL_CONNECTED
  )
  {
    Serial.println(
      "WiFi Connected"
    );


    Serial.print(
      "IP Address: "
    );


    Serial.println(
      WiFi.localIP()
    );
  }

  else
  {
    Serial.println(
      "WiFi Connection Failed"
    );
  }
}


// ================================================================
// WIFI RECONNECTION
// ================================================================

void reconnectWiFi()
{
  Serial.println(
    "Reconnecting WiFi..."
  );


  WiFi.disconnect();


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );
}


// ================================================================
// NTP TIME
// ================================================================

void initializeTime()
{
  Serial.println(
    "Synchronizing Time..."
  );


  configTime(
    GMT_OFFSET_SECONDS,
    DAYLIGHT_OFFSET_SECONDS,
    NTP_SERVER_1,
    NTP_SERVER_2
  );


  struct tm timeInfo;


  if (
    getLocalTime(
      &timeInfo,
      10000
    )
  )
  {
    Serial.println(
      "Time Synchronized"
    );
  }

  else
  {
    Serial.println(
      "Time Synchronization Failed"
    );
  }
}


// ================================================================
// FIREBASE
// ================================================================

void initializeFirebase()
{
  Serial.println(
    "Initializing Firebase..."
  );


  config.api_key = API_KEY;


  config.database_url = DATABASE_URL;


  auth.user.email = USER_EMAIL;


  auth.user.password = USER_PASSWORD;


  config.token_status_callback =
  tokenStatusCallback;


  Firebase.reconnectWiFi(true);


  Firebase.begin(
    &config,
    &auth
  );


  Serial.println(
    "Firebase Started"
  );
}


// ================================================================
// SENSOR INITIALIZATION
// ================================================================

void initializeSensors()
{
  // --------------------------------------------------------------
  // TMP117
  // --------------------------------------------------------------

  Serial.println(
    "Checking TMP117..."
  );


  if (
    tmp117.begin(
      0x48,
      &Wire
    )
  )
  {
    tmp117Available = true;


    Serial.println(
      "TMP117 Connected"
    );
  }

  else
  {
    Serial.println(
      "TMP117 Not Found"
    );
  }


  // --------------------------------------------------------------
  // MPU6050
  // --------------------------------------------------------------

  Serial.println(
    "Checking MPU6050..."
  );


  if (
    mpu.begin(
      0x68,
      &Wire
    )
  )
  {
    mpu6050Available = true;


    mpu.setAccelerometerRange(
      MPU6050_RANGE_8_G
    );


    mpu.setGyroRange(
      MPU6050_RANGE_500_DEG
    );


    mpu.setFilterBandwidth(
      MPU6050_BAND_21_HZ
    );


    Serial.println(
      "MPU6050 Connected"
    );
  }

  else
  {
    Serial.println(
      "MPU6050 Not Found"
    );
  }


  // --------------------------------------------------------------
  // MAX30102
  // --------------------------------------------------------------

  Serial.println(
    "Checking MAX30102..."
  );


  if (
    max30102.begin(
      Wire,
      I2C_SPEED_FAST
    )
  )
  {
    max30102Available = true;


    max30102.setup(
      60,
      4,
      2,
      100,
      411,
      4096
    );


    Serial.println(
      "MAX30102 Connected"
    );
  }

  else
  {
    Serial.println(
      "MAX30102 Not Found"
    );
  }


  // --------------------------------------------------------------
  // GPS
  // --------------------------------------------------------------

  Serial.println(
    "Checking NEO-M8L..."
  );


  GPSSerial.begin(
    GPS_BAUD_RATE,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );


  delay(1000);


  if (
    gps.begin(GPSSerial)
  )
  {
    gpsAvailable = true;


    Serial.println(
      "GPS Connected"
    );
  }

  else
  {
    Serial.println(
      "GPS Not Found"
    );
  }


  // --------------------------------------------------------------
  // AD8232
  // --------------------------------------------------------------

  pinMode(
    AD8232_OUTPUT_PIN,
    INPUT
  );


  pinMode(
    AD8232_LO_PLUS_PIN,
    INPUT
  );


  pinMode(
    AD8232_LO_MINUS_PIN,
    INPUT
  );


  pinMode(
    AD8232_SDN_PIN,
    OUTPUT
  );


  digitalWrite(
    AD8232_SDN_PIN,
    HIGH
  );


  Serial.println(
    "AD8232 Initialized"
  );
}


// ================================================================
// MEASUREMENT CYCLE
// ================================================================

void performMeasurementCycle()
{
  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "STARTING MEASUREMENT"
  );

  Serial.println(
    "========================================"
  );


  if (
    tmp117Available
  )
  {
    readTemperature();
  }


  if (
    max30102Available
  )
  {
    readHeartRateAndSpO2();
  }


  readGPS();


  readECG();


  classifyVitalSigns();


  displaySensorData();


  uploadRealtimeData();


  uploadHistoryData();


  uploadDeviceStatus();


  Serial.println(
    "Next Measurement After 5 Minutes"
  );
}


// ================================================================
// READ TEMPERATURE
// ================================================================

void readTemperature()
{
  sensors_event_t event;


  tmp117.getEvent(
    &event
  );


  temperatureC =
  event.temperature;
}


// ================================================================
// READ HEART RATE AND SPO2
// ================================================================

void readHeartRateAndSpO2()
{
  Serial.println(
    "Collecting MAX30102 samples..."
  );


  max30102.clearFIFO();


  for (
    int i = 0;
    i < MAX30102_SAMPLE_COUNT;
    i++
  )
  {
    unsigned long timeout =
    millis();


    while (
      !max30102.available()
    )
    {
      max30102.check();


      if (
        millis() - timeout
        >
        2000
      )
      {
        Serial.println(
          "MAX30102 Timeout"
        );


        heartRateBPM = 0;


        spo2Value = 0;


        return;
      }
    }


    redBuffer[i] =
    max30102.getRed();


    irBuffer[i] =
    max30102.getIR();


    max30102.nextSample();
  }


  // --------------------------------------------------------------
  // FINGER CONTACT CHECK
  // --------------------------------------------------------------

  if (
    irBuffer[MAX30102_SAMPLE_COUNT - 1]
    <
    50000
  )
  {
    Serial.println(
      "Finger Not Detected"
    );


    heartRateBPM = 0;


    spo2Value = 0;


    return;
  }


  // --------------------------------------------------------------
  // CALCULATE HR AND SPO2
  // --------------------------------------------------------------

  maxim_heart_rate_and_oxygen_saturation(
    irBuffer,
    MAX30102_SAMPLE_COUNT,
    redBuffer,
    &calculatedSpO2,
    &validSpO2,
    &calculatedHeartRate,
    &validHeartRate
  );


  // --------------------------------------------------------------
  // VALIDATE HEART RATE
  // --------------------------------------------------------------

  if (
    validHeartRate
    &&
    calculatedHeartRate >= 30
    &&
    calculatedHeartRate <= 220
  )
  {
    heartRateBPM =
    calculatedHeartRate;
  }

  else
  {
    heartRateBPM = 0;
  }


  // --------------------------------------------------------------
  // VALIDATE SPO2
  // --------------------------------------------------------------

  if (
    validSpO2
    &&
    calculatedSpO2 >= 70
    &&
    calculatedSpO2 <= 100
  )
  {
    spo2Value =
    calculatedSpO2;
  }

  else
  {
    spo2Value = 0;
  }
}


// ================================================================
// CLASSIFY VITAL SIGNS
// ================================================================

void classifyVitalSigns()
{
  // --------------------------------------------------------------
  // TEMPERATURE
  // --------------------------------------------------------------

  if (
    temperatureC < 36.5
  )
  {
    temperatureStatus =
    "Below Reference Range";
  }

  else if (
    temperatureC > 37.3
  )
  {
    temperatureStatus =
    "Above Reference Range";
  }

  else
  {
    temperatureStatus =
    "Within Reference Range";
  }


  // --------------------------------------------------------------
  // HEART RATE
  // --------------------------------------------------------------

  if (
    heartRateBPM == 0
  )
  {
    heartRateStatus =
    "Invalid Reading";
  }

  else if (
    heartRateBPM < 60
  )
  {
    heartRateStatus =
    "Below Reference Range";
  }

  else if (
    heartRateBPM > 100
  )
  {
    heartRateStatus =
    "Above Reference Range";
  }

  else
  {
    heartRateStatus =
    "Within Reference Range";
  }


  // --------------------------------------------------------------
  // SPO2
  // --------------------------------------------------------------

  if (
    spo2Value == 0
  )
  {
    spo2Status =
    "Invalid Reading";
  }

  else if (
    spo2Value < 95
  )
  {
    spo2Status =
    "Below Reference Range";
  }

  else
  {
    spo2Status =
    "Within Reference Range";
  }
}


// ================================================================
// READ MPU6050
// ================================================================

void readMPU6050()
{
  sensors_event_t acceleration;

  sensors_event_t gyro;

  sensors_event_t sensorTemperature;


  mpu.getEvent(
    &acceleration,
    &gyro,
    &sensorTemperature
  );


  accelerationX =
  acceleration.acceleration.x;


  accelerationY =
  acceleration.acceleration.y;


  accelerationZ =
  acceleration.acceleration.z;


  totalAcceleration =
  sqrt(
    accelerationX * accelerationX
    +
    accelerationY * accelerationY
    +
    accelerationZ * accelerationZ
  );
}


// ================================================================
// FALL DETECTION
// ================================================================

void processFallDetection()
{
  // --------------------------------------------------------------
  // FREE FALL
  // --------------------------------------------------------------

  if (
    totalAcceleration < 3.0
    &&
    !freeFallDetected
  )
  {
    freeFallDetected = true;


    freeFallStartTime = millis();
  }


  // --------------------------------------------------------------
  // IMPACT
  // --------------------------------------------------------------

  if (
    freeFallDetected
    &&
    totalAcceleration > 25.0
    &&
    millis() - freeFallStartTime < 2000
  )
  {
    fallDetected = true;


    fallDetectedTime = millis();


    freeFallDetected = false;


    Serial.println(
      "WARNING: FALL DETECTED"
    );


    uploadFallAlert();
  }


  // --------------------------------------------------------------
  // FREE-FALL TIMEOUT
  // --------------------------------------------------------------

  if (
    freeFallDetected
    &&
    millis() - freeFallStartTime > 2000
  )
  {
    freeFallDetected = false;
  }


  // --------------------------------------------------------------
  // RESET FALL STATE
  // --------------------------------------------------------------

  if (
    fallDetected
    &&
    millis() - fallDetectedTime > 30000
  )
  {
    fallDetected = false;
  }
}


// ================================================================
// READ GPS
// ================================================================

void readGPS()
{
  if (
    !gpsAvailable
  )
  {
    return;
  }


  gps.checkUblox();


  long rawLatitude =
  gps.getLatitude();


  long rawLongitude =
  gps.getLongitude();


  if (
    rawLatitude != 0
    &&
    rawLongitude != 0
  )
  {
    latitude =
    rawLatitude / 10000000.0;


    longitude =
    rawLongitude / 10000000.0;


    locationValid = true;
  }
}


// ================================================================
// READ ECG
// ================================================================

void readECG()
{
  bool leadsOff =

  digitalRead(
    AD8232_LO_PLUS_PIN
  )

  ||

  digitalRead(
    AD8232_LO_MINUS_PIN
  );


  if (
    leadsOff
  )
  {
    ecgLeadsConnected = false;


    ecgValue = 0;
  }

  else
  {
    ecgLeadsConnected = true;


    ecgValue =
    analogRead(
      AD8232_OUTPUT_PIN
    );
  }
}


// ================================================================
// DISPLAY SENSOR DATA
// ================================================================

void displaySensorData()
{
  Serial.println();

  Serial.println(
    "========================================"
  );

  Serial.println(
    "SENSOR DATA"
  );

  Serial.println(
    "========================================"
  );


  Serial.print(
    "Temperature: "
  );

  Serial.print(
    temperatureC,
    2
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Temperature Status: "
  );

  Serial.println(
    temperatureStatus
  );


  Serial.print(
    "Heart Rate: "
  );

  Serial.print(
    heartRateBPM,
    0
  );

  Serial.println(
    " BPM"
  );


  Serial.print(
    "Heart Rate Status: "
  );

  Serial.println(
    heartRateStatus
  );


  Serial.print(
    "SpO2: "
  );

  Serial.print(
    spo2Value,
    0
  );

  Serial.println(
    " %"
  );


  Serial.print(
    "SpO2 Status: "
  );

  Serial.println(
    spo2Status
  );


  Serial.print(
    "Fall Detected: "
  );

  Serial.println(
    fallDetected ? "YES" : "NO"
  );


  Serial.print(
    "Latitude: "
  );

  Serial.println(
    latitude,
    7
  );


  Serial.print(
    "Longitude: "
  );

  Serial.println(
    longitude,
    7
  );


  Serial.print(
    "ECG Value: "
  );

  Serial.println(
    ecgValue
  );


  Serial.print(
    "ECG Leads Connected: "
  );

  Serial.println(
    ecgLeadsConnected ? "YES" : "NO"
  );


  Serial.println(
    "========================================"
  );
}


// ================================================================
// FIREBASE REALTIME DATA
// ================================================================

void uploadRealtimeData()
{
  if (
    WiFi.status() != WL_CONNECTED
    ||
    !Firebase.ready()
  )
  {
    Serial.println(
      "Firebase Not Ready"
    );


    return;
  }


  String path =

  "/devices/"

  +

  String(DEVICE_ID)

  +

  "/realtime";


  FirebaseJson json;


  json.set(
    "temperature/value",
    temperatureC
  );


  json.set(
    "temperature/status",
    temperatureStatus
  );


  json.set(
    "heartRate/value",
    heartRateBPM
  );


  json.set(
    "heartRate/status",
    heartRateStatus
  );


  json.set(
    "spo2/value",
    spo2Value
  );


  json.set(
    "spo2/status",
    spo2Status
  );


  json.set(
    "fallDetected",
    fallDetected
  );


  json.set(
    "location/latitude",
    latitude
  );


  json.set(
    "location/longitude",
    longitude
  );


  json.set(
    "location/valid",
    locationValid
  );


  json.set(
    "ecg/value",
    ecgValue
  );


  json.set(
    "ecg/leadsConnected",
    ecgLeadsConnected
  );


  json.set(
    "timestamp",
    getTimestamp()
  );


  if (
    Firebase.RTDB.updateNode(
      &fbdo,
      path,
      &json
    )
  )
  {
    Serial.println(
      "Realtime Data Uploaded"
    );
  }

  else
  {
    Serial.print(
      "Realtime Upload Error: "
    );


    Serial.println(
      fbdo.errorReason()
    );
  }
}


// FIREBASE HISTORY

void uploadHistoryData()
{
  if (
    WiFi.status() != WL_CONNECTED
    ||
    !Firebase.ready()
  )
  {
    return;
  }


  String path =

  "/devices/"

  +

  String(DEVICE_ID)

  +

  "/history/"

  +

  getCurrentDate()

  +

  "/"

  +

  getCurrentTime();


  FirebaseJson json;


  json.set(
    "temperature",
    temperatureC
  );


  json.set(
    "temperatureStatus",
    temperatureStatus
  );


  json.set(
    "heartRate",
    heartRateBPM
  );


  json.set(
    "heartRateStatus",
    heartRateStatus
  );


  json.set(
    "spo2",
    spo2Value
  );


  json.set(
    "spo2Status",
    spo2Status
  );


  json.set(
    "fallDetected",
    fallDetected
  );


  json.set(
    "latitude",
    latitude
  );


  json.set(
    "longitude",
    longitude
  );


  json.set(
    "locationValid",
    locationValid
  );


  json.set(
    "ecg",
    ecgValue
  );


  json.set(
    "ecgLeadsConnected",
    ecgLeadsConnected
  );


  json.set(
    "timestamp",
    getTimestamp()
  );


  if (
    Firebase.RTDB.setJSON(
      &fbdo,
      path,
      &json
    )
  )
  {
    Serial.println(
      "History Saved"
    );
  }

  else
  {
    Serial.print(
      "History Upload Error: "
    );


    Serial.println(
      fbdo.errorReason()
    );
  }
}


// DEVICE STATUS

void uploadDeviceStatus()
{
  if (
    WiFi.status() != WL_CONNECTED
    ||
    !Firebase.ready()
  )
  {
    return;
  }


  String path =

  "/devices/"

  +

  String(DEVICE_ID)

  +

  "/status";


  FirebaseJson json;


  json.set(
    "online",
    true
  );


  json.set(
    "wifiRSSI",
    WiFi.RSSI()
  );


  json.set(
    "uptimeSeconds",
    millis() / 1000
  );


  json.set(
    "measurementIntervalSeconds",
    300
  );


  json.set(
    "lastSeen",
    getTimestamp()
  );


  if (
    Firebase.RTDB.updateNode(
      &fbdo,
      path,
      &json
    )
  )
  {
    Serial.println(
      "Device Status Updated"
    );
  }

  else
  {
    Serial.print(
      "Status Upload Error: "
    );


    Serial.println(
      fbdo.errorReason()
    );
  }
}


// FALL ALERT

void uploadFallAlert()
{
  if (
    WiFi.status() != WL_CONNECTED
    ||
    !Firebase.ready()
  )
  {
    return;
  }


  String path =

  "/devices/"

  +

  String(DEVICE_ID)

  +

  "/alerts/falls";


  FirebaseJson json;


  json.set(
    "type",
    "FALL_DETECTED"
  );


  json.set(
    "latitude",
    latitude
  );


  json.set(
    "longitude",
    longitude
  );


  json.set(
    "locationValid",
    locationValid
  );


  json.set(
    "timestamp",
    getTimestamp()
  );


  if (
    Firebase.RTDB.pushJSON(
      &fbdo,
      path,
      &json
    )
  )
  {
    Serial.println(
      "Fall Alert Uploaded"
    );
  }

  else
  {
    Serial.print(
      "Fall Alert Error: "
    );


    Serial.println(
      fbdo.errorReason()
    );
  }
}

// GET CURRENT DATE

String getCurrentDate()
{
  struct tm timeInfo;


  if (
    !getLocalTime(
      &timeInfo
    )
  )
  {
    return "DATE_UNAVAILABLE";
  }


  char buffer[11];


  strftime(
    buffer,
    sizeof(buffer),
    "%Y-%m-%d",
    &timeInfo
  );


  return String(buffer);
}


// GET CURRENT TIME

String getCurrentTime()
{
  struct tm timeInfo;


  if (
    !getLocalTime(
      &timeInfo
    )
  )
  {
    return String(millis());
  }


  char buffer[9];


  strftime(
    buffer,
    sizeof(buffer),
    "%H-%M-%S",
    &timeInfo
  );


  return String(buffer);
}



// GET TIMESTAMP


String getTimestamp()
{
  return

  getCurrentDate()

  +

  " "

  +

  getCurrentTime();
}