#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "time.h"
#include "preferences.h" //file containing User preferences

// Define the pins for the LCD display
#define SDA 13 //Define SDA pin
#define SCL 14 //Define SCL pin

// Initialize the LiquidCrystal_I2C object with the I2C address
LiquidCrystal_I2C lcd(0x27, 20, 4); // Change the address (0x27) if needed

#define SWITCH_PIN 25 // sets pin for direction toggle switch connect other end to GND

// This defines the direction variable differently depending on if you are using a toggle switch
#ifndef DIRECTION
char *direction = "S";
#else
const char *direction = DIRECTION;
#endif

// Declaring global variables
unsigned long lastRequestTime = 0;
unsigned long lastDisplayTime = 0;
time_t currentEpochTime;
struct tm currentTime;
char display[20];
char displayList[8][20];
byte listCount = 1;
bool forceRefresh = true;
bool switchState = true;
char url1[sizeof(serverIP) + 22];
char url2[sizeof(serverIP) + 22];
char *url;
byte numberOfArrivals;
byte timeToStation = timeToStation1;

bool forceWeatherRefresh = true;
unsigned long lastWeatherRequestTime = 0;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("Initializing...");

  // Initialize the LCD display
  Wire.begin(SDA, SCL);
  lcd.init();
  lcd.backlight(); // Turn on the backlight
  lcd.setCursor(0, 0);
  lcd.print("setting up...!");

  // Sets up toggle switch pin with internal pullup resistor
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Connect to the WiFi network
  connectWifi();

  // Configure DST-aware datetime for NYC
  initTime("EST5EDT,M3.2.0,M11.1.0");

  Serial.print("Local time: ");
  printLocalTime();
  Serial.println(" - Success!");

  // Populate url1 and url2 with the respective URLs
  sprintf(url1, "http://%s:8080/by-id/%s", serverIP, station1);
  sprintf(url2, "http://%s:8080/by-id/%s", serverIP, station2);

  // Initialize url to point to url1
  url = url1;

  delay(1000);
  lcd.clear();

  // Print initial free heap memory
  Serial.print("Free heap memory: ");
  Serial.println(ESP.getFreeHeap());

}

void loop()
{

  // Print free heap memory periodically
  static unsigned long lastHeapPrintTime = 0;
  if (millis() - lastHeapPrintTime > 5000) // Print every 5 seconds
  {
    Serial.print("Free heap memory: ");
    Serial.println(ESP.getFreeHeap());
    lastHeapPrintTime = millis();
  }

  switchHandler();

  updateClockDisplay();

  if ( forceWeatherRefresh  || (millis() - lastWeatherRequestTime) > weatherRequestInterval )
  {
    displayWeather();
    lastWeatherRequestTime = millis();
    forceWeatherRefresh = false;
  }


  // Send an HTTP GET request every time interval
  if (forceRefresh || (millis() - lastRequestTime) > requestInterval)
  {

    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED)
    {

      // gets current local time in epoch and tm format respectively
      time(&currentEpochTime);
      getLocalTime(&currentTime);

      // Make server request and parse into JSON object
      JSONVar obj = JSON.parse(httpGETRequest(url, 2));

      // JSON.typeof(jsonVar) can be used to get the type of the var
      if (JSON.typeof(obj) == "undefined")
      {
        Serial.println("Parsing input failed!");
        return;
      }

      // Pulls out the relevant data as an JSONVar array
      JSONVar arrivalsArr = obj["data"][0][direction];
      numberOfArrivals = arrivalsArr.length();

      // Initiate a count of arrivals that will be missed per timeToStation
      byte missed = 0;

      // Clear the displayList
      memset(&displayList, 0, sizeof(displayList));

      // Iterates through each pending arrival
      for (byte i = 0; i < numberOfArrivals; i++)
      {

        // Extracts the name of the train for the given arrival
        String trainName = JSON.stringify(arrivalsArr[i]["route"]).substring(1, 2);

        // Extracts the arrival time of the train in epoch time

        // DEBUG
        // Serial.print("Arrival time stringified: ");
        // Serial.println(JSON.stringify(arrivalsArr[i]["time"]));

        long arrivalTime = convertToEpoch(JSON.stringify(arrivalsArr[i]["time"]));

        // Calculates how many minutes to arrival by comparing arrival time to current time
        int minutesAway = (arrivalTime - currentEpochTime) / 60;

        // Filters out trains that you can't possibly catch
        if (minutesAway >= timeToStation)
        {

          // Constructs the display string
          sprintf(display, "%d. (%s) %s %dMin", i + 1 - missed, trainName, (direction == "N") ? "UP" : "DN", minutesAway);
          Serial.println(display);

          // Adds the given arrival to the display list for the lcd
          strcpy(displayList[i - missed], display);
        }
        else
        {
          missed++; // increment count of missed trains per timeToStation
        }
      }

      // Display the next arriving train on the first line of the lcd
      lcd.setCursor(0, 2);
      lcd.print("                    "); // needed to clear the line
      lcd.setCursor(0, 2);
      lcd.print(displayList[0]);
    }
    else
    {
      Serial.println("WiFi Disconnected");
      delay(1000);
      connectWifi();
    }
    lastRequestTime = millis();
  }

  // Rotate the arrival displayed on the second line at specified time interval
  if ( forceRefresh || (millis() - lastDisplayTime) > displayInterval)
  {

    lcd.setCursor(0, 3);
    lcd.print("                    "); // needed to clear the line if the previous display was longer
    lcd.setCursor(0, 3);
    lcd.print(displayList[listCount]);

    listCount++;
    if (listCount > moreArrivals || listCount >= numberOfArrivals)
      listCount = 1;

    lastDisplayTime = millis();

    forceRefresh = false;
  }

}

void connectWifi()
{

  Serial.print("Connecting to Wifi: ");
  Serial.print(ssid);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Joining Wifi");
  lcd.setCursor(0, 1);
  lcd.print(ssid);

  WiFi.begin(ssid, password);
  int timeout_counter = 0;

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    // lcd.print(".");
    timeout_counter++;
    if(timeout_counter >= 1200){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Restarting");
      delay(1000);
      ESP.restart();
    }

  }

  lcd.setCursor(0, 0);
  lcd.print("Connected to:");
  delay(1000);
  lcd.clear();
  Serial.println("Success!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

String httpGETRequest(char *_url, int line)
{
  HTTPClient http;

  Serial.print("Pinging: ");
  Serial.println(_url);

  // Connect to server url
  http.begin(_url);

  http.setTimeout(10000); // Set timeout to 10 seconds

  // Send HTTP GET request
  byte httpResponseCode = http.GET();

  if (httpResponseCode == 200)
  {
    Serial.print("Updated: ");
    Serial.println(&currentTime);
    return http.getString();
  }
  else
  {
    Serial.print("HTTP Error code: ");
    Serial.println(httpResponseCode);

    lcd.setCursor(0, line);
    lcd.print("                    "); // needed to clear the line
    lcd.setCursor(0, line);
    lcd.print("HTTP ERROR: ");
    lcd.print(httpResponseCode);
    delay(1000);
  }
}

bool isDST(const String &dateTime)
{
  if (dateTime.endsWith("-04:00\""))
  {
    return true;
  }
  else if (dateTime.endsWith("-04:00"))
  {
    return true;
  }
  return false;
}

// Manually parses the timeStamp from the train arrival and returns in epoch time
long convertToEpoch(String timeStamp)
{
  struct tm t;
  memset(&t, 0, sizeof(tm));                            // Initalize to all 0's
  t.tm_year = timeStamp.substring(1, 5).toInt() - 1900; // This is year-1900, so 112 = 2012
  t.tm_mon = timeStamp.substring(6, 8).toInt() - 1;     // It has -1 because the months are 0-11
  t.tm_mday = timeStamp.substring(9, 11).toInt();
  t.tm_hour = timeStamp.substring(12, 14).toInt();
  t.tm_min = timeStamp.substring(15, 17).toInt();
  t.tm_sec = timeStamp.substring(18, 20).toInt();

  // Run a check to see if MTA arrival times are DST or not
  bool dst = isDST(timeStamp);
  t.tm_isdst = dst;
  // Debug:
  // Serial.println(dst ? "It is DST" : "It is not DST");

  time_t epoch = mktime(&t);
  return epoch;
}

// Function to handle direction toggle switch
void switchHandler() {
    // toggles with switch
    if (digitalRead(SWITCH_PIN) != switchState) {
        switchState = !switchState;

        // Sets url pointer to point to url1 or url2 based on switch state
        if (switchState) {
            url = url1;
            timeToStation = timeToStation1;
        }
        else {
            url = url2;
            timeToStation = timeToStation2;
        }

        // Forces refresh on the next loop
        forceRefresh = true;
        delay(500);
    }
}

void setTimezone(String timezone)
{
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1); //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  tzset();
}

void initTime(String timezone)
{
  Serial.println("Setting up time");
  int retries = 0;
  const int maxRetries = 10; // Maximum number of retries
  const int retryDelay = 4000; // Delay between retries in milliseconds

  while (retries < maxRetries)
  {
    configTime(0, 0, "pool.ntp.org"); // First connect to NTP server, with 0 TZ offset
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      Serial.println("Got the time from NTP");
      // Now we can set the real timezone
      setTimezone(timezone);
      return;
    }
    else
    {
      Serial.println("Failed to obtain time. Retrying...");
      retries++;
      delay(retryDelay);
    }
  }

  Serial.println("Exceeded maximum retries. Giving up.");
}

void printLocalTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time (in printLocalTime())");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S zone %Z %z ");
}

void updateClockDisplay()
{
  // Get current time
  if (getLocalTime(&currentTime))
  {
      // Format time string
      char timeString[8]; // HH:MMXM\0
      int hour = currentTime.tm_hour;
      const char* suffix = (hour < 12) ? "AM" : "PM";
      
      // Convert 24-hour format to 12-hour format
      if (hour == 0) {
          hour = 12; // Midnight (0:00) is displayed as 12:00 AM
      } else if (hour > 12) {
          hour -= 12;
      }
      
      sprintf(timeString, "%02d:%02d%s", hour, currentTime.tm_min, suffix);
    // Greet the user based on the time of day
    char greeting[20]; // For storing the greeting message
    if (currentTime.tm_hour < 12)
    {
      snprintf(greeting, sizeof(greeting), "Good morning %s!", name);
    }
    else if (currentTime.tm_hour < 18)
    {
      snprintf(greeting, sizeof(greeting), "Afternoon, %s!", name);
    }
    else
    {
      snprintf(greeting, sizeof(greeting), "Good evening %s!", name);
    }

    // Print the time on the first line
    lcd.setCursor(0, 0);
    lcd.print(timeString);

    // Print the greeting on the second line
    lcd.setCursor(0, 1);
    lcd.print(greeting);
  }
}

void displayWeather() {
  Serial.println("Inside displayWeather()");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected. Cannot fetch weather data.");
    return;
  }

  HTTPClient http;

  String endpoint = "http://api.openweathermap.org/data/2.5/onecall?lat=" + String(LATITUDE) + "&lon=" + String(LONGITUDE) + "&exclude=hourly,daily&appid=" + String(OPENWEATHERMAP_API_KEY) + "&units=imperial";

  Serial.print("Requesting weather data from: ");
  Serial.println(endpoint);

  http.begin(endpoint);

  int httpResponseCode = http.GET();

  if (httpResponseCode == 200) {
    String payload = http.getString();
    JSONVar weatherData = JSON.parse(payload);
    
    if (!weatherData.hasOwnProperty("current")) {
      Serial.println("Failed to parse weather data.");
      // lcd.setCursor(0, 1);
      // lcd.print("                    "); // needed to clear the line
      // lcd.setCursor(0, 1);
      // lcd.print("Parsing error");
      return;
    }

    int currentTemp = int(weatherData["current"]["temp"]); // Convert temperature to integer
    // lcd.clear();

    int cursorPosition = (currentTemp >= 100 || currentTemp < 0) ? 15 : 16;

    // Print current temperature on the second line
    lcd.setCursor(cursorPosition, 0);
    lcd.print(String(currentTemp));
    lcd.print((char)223); // Degree symbol
    lcd.print("F");

    // Generate the second line text
    // String secondLineText = generateSecondLineText(weatherData);

    // Display the second line text on the LCD
    // String currentWeatherDescription = weatherData["current"]["weather"][0]["description"];
    // lcd.setCursor(0, 2);
    // lcd.print(currentWeatherDescription);


  } else {
    Serial.print("Error fetching weather data. HTTP error code: ");
    Serial.println(httpResponseCode);
    lcd.setCursor(0, 1);
    lcd.print("                    "); // needed to clear the line
    lcd.setCursor(0, 1);
    delay(1000);
    lcd.setCursor(0, 1);
    lcd.print("                    "); // needed to clear the line
    forceWeatherRefresh = true;
  }

  http.end();
}

