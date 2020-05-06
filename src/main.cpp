#include <Arduino.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
//____________________________________________________________

#define ServerVersion "1.0"
String webpage = "";
bool SPIFFS_present = false;

#include "FS.h"
#include "CSS.h"
#include "SD.h"
#include <SPI.h>

#include <WiFi.h> // Built-in
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include "SPIFFS.h"

WebServer server(80);

#include <SPI.h>
#include <Wire.h>
#include "RTClib.h"
#include <Adafruit_Sensor.h>

#include "DHT.h"
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

String dataMessage;
String dataMessage2;
String dataMessage3;
String iniciar = "no";

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

String str2;
String str1;
float temp_1;
float hum_1;

float temp_0;
float hum_0;

byte yawData[4];

float temperature;
float h;
float f;

byte yawData0[4];

float temperature0;
float h0;
float f0;

int value;
int sensorPin = 32; //no usar pines asociados al grabado de memoria como el pin 15 ->HS2_CMD 14 ->HS2_CLK / pin2 conectado a pin0
int sensorValue = 0;
const int PIN_AP = 15;

float Vout = 0;
float P = 0;
float RP = 0;
String RPs;

const char ssid[] = "ESP32-CAM";
const char password[] = "esp32cam!";

//RTC
#define I2C_SDA 21 //<-esp32 wover / version esp32-cam-> 13
#define I2C_SCL 22 //<- version / esp32-cam ->16

RTC_DS1307 RTC;

#define RXD2 16 //16
#define TXD2 17 //17

#define RXD1 4 //16 editar el framework HardwareSerial.cpp para cambiar los pines por defecto 9 para Rx1 y 10 para TX1
#define TXD1 2 //17
/*#define RXD1 4 //16 editar el framework HardwareSerial.cpp para cambiar los pines por defecto 9 para Rx1 y 10 para TX1
#define TXD1 2 //17*/

// Replace with your network credentials
//const char *ssid = "Amsterdam";
//const char *password = "1216004710";

/*const char *ssid = "PROTOBAM2";
const char *password = "protome123!";*/
void SendHTML_Header()
{
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "-1");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Content()
{
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Stop()
{
  server.sendContent("");
  server.client().stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String command, String arg_calling_name)
{
  SendHTML_Header();
  webpage += F("<h3>");
  webpage += heading1 + "</h3>";
  webpage += F("<FORM action='/");
  webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
  webpage += F("<input type='text' name='");
  webpage += arg_calling_name;
  webpage += F("' value=''><br>");
  webpage += F("<type='submit' name='");
  webpage += arg_calling_name;
  webpage += F("' value=''><br><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportSPIFFSNotPresent()
{
  SendHTML_Header();
  webpage += F("<h3>No SPIFFS Card present</h3>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportFileNotPresent(String target)
{
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>");
  webpage += F("<a href='/");
  webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportCouldNotCreateFile(String target)
{
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>");
  webpage += F("<a href='/");
  webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
String file_size(int bytes)
{
  String fsize = "";
  if (bytes < 1024)
    fsize = String(bytes) + " B";
  else if (bytes < (1024 * 1024))
    fsize = String(bytes / 1024.0, 3) + " KB";
  else if (bytes < (1024 * 1024 * 1024))
    fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
  else
    fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
  return fsize;
}
void HomePage()
{
  SendHTML_Header();
  webpage += F("<a href='/download'><button>Download</button></a>");
  webpage += F("<a href='/upload'><button>Upload</button></a>");
  webpage += F("<a href='/stream'><button>Stream</button></a>");
  webpage += F("<a href='/delete'><button>Delete</button></a>");
  webpage += F("<a href='/dir'><button>Directory</button></a>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop(); // Stop is needed because no content length was sent
}
void DownloadFile(String filename)
{
  if (SPIFFS_present)
  {
    File download = SPIFFS.open("/" + filename, "r");
    if (download)
    {
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename=" + filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      download.close();
    }
    else
      ReportFileNotPresent("download");
  }
  else
    ReportSPIFFSNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Download()
{ // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  iniciar = "si";
  if (server.args() > 0)
  { // Arguments were received
    if (server.hasArg("download"))
      DownloadFile(server.arg(0));
  }
  else
    SelectInput("Enter filename to download", "download", "download");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Upload()
{
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>");
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fupload' id = 'fupload' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html", webpage);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
File UploadFile;
void handleFileUpload()
{                                           // upload a new file to the Filing system
  HTTPUpload &uploadfile = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                            // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if (uploadfile.status == UPLOAD_FILE_START)
  {
    String filename = uploadfile.filename;
    if (!filename.startsWith("/"))
      filename = "/" + filename;
    Serial.print("Upload File Name: ");
    Serial.println(filename);
    SPIFFS.remove(filename);                 // Remove a previous version, otherwise data is appended the file again
    UploadFile = SPIFFS.open(filename, "w"); // Open the file for writing in SPIFFS (create it, if doesn't exist)
  }
  else if (uploadfile.status == UPLOAD_FILE_WRITE)
  {
    if (UploadFile)
      UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  }
  else if (uploadfile.status == UPLOAD_FILE_END)
  {
    if (UploadFile) // If the file was successfully created
    {
      UploadFile.close(); // Close the file again
      Serial.print("Upload Size: ");
      Serial.println(uploadfile.totalSize);
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>");
      webpage += F("<h2>Uploaded File Name: ");
      webpage += uploadfile.filename + "</h2>";
      webpage += F("<h2>File Size: ");
      webpage += file_size(uploadfile.totalSize) + "</h2><br>";
      append_page_footer();
      server.send(200, "text/html", webpage);
    }
    else
    {
      ReportCouldNotCreateFile("upload");
    }
  }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifdef ESP32
void printDirectory(const char *dirname, uint8_t levels)
{
  File root = SPIFFS.open(dirname);
  if (!root)
  {
    return;
  }
  if (!root.isDirectory())
  {
    return;
  }
  File file = root.openNextFile();
  while (file)
  {
    if (webpage.length() > 1000)
    {
      SendHTML_Content();
    }
    if (file.isDirectory())
    {
      webpage += "<tr><td>" + String(file.isDirectory() ? "Dir" : "File") + "</td><td>" + String(file.name()) + "</td><td></td></tr>";
      printDirectory(file.name(), levels - 1);
    }
    else
    {
      webpage += "<tr><td>" + String(file.name()) + "</td>";
      webpage += "<td>" + String(file.isDirectory() ? "Dir" : "File") + "</td>";
      int bytes = file.size();
      String fsize = "";
      if (bytes < 1024)
        fsize = String(bytes) + " B";
      else if (bytes < (1024 * 1024))
        fsize = String(bytes / 1024.0, 3) + " KB";
      else if (bytes < (1024 * 1024 * 1024))
        fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
      else
        fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
      webpage += "<td>" + fsize + "</td></tr>";
    }
    file = root.openNextFile();
  }
  file.close();
}
void SPIFFS_dir()
{
  if (SPIFFS_present)
  {
    File root = SPIFFS.open("/");
    if (root)
    {
      root.rewindDirectory();
      SendHTML_Header();
      webpage += F("<h3 class='rcorners_m'>SD Card Contents</h3><br>");
      webpage += F("<table align='center'>");
      webpage += F("<tr><th>Name/Type</th><th style='width:20%'>Type File/Dir</th><th>File Size</th></tr>");
      printDirectory("/", 0);
      webpage += F("</table>");
      SendHTML_Content();
      root.close();
    }
    else
    {
      SendHTML_Header();
      webpage += F("<h3>No Files Found</h3>");
    }
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop(); // Stop is needed because no content length was sent
  }
  else
    ReportSPIFFSNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#endif
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#ifdef ESP8266
void SPIFFS_dir()
{
  String str;
  if (SPIFFS_present)
  {
    Dir dir = SPIFFS.openDir("/");
    SendHTML_Header();
    webpage += F("<h3 class='rcorners_m'>SPIFFS Card Contents</h3><br>");
    webpage += F("<table align='center'>");
    webpage += F("<tr><th>Name/Type</th><th style='width:40%'>File Size</th></tr>");
    while (dir.next())
    {
      Serial.print(dir.fileName());
      webpage += "<tr><td>" + String(dir.fileName()) + "</td>";
      str = dir.fileName();
      str += " / ";
      if (dir.fileSize())
      {
        File f = dir.openFile("r");
        Serial.println(f.size());
        int bytes = f.size();
        String fsize = "";
        if (bytes < 1024)
          fsize = String(bytes) + " B";
        else if (bytes < (1024 * 1024))
          fsize = String(bytes / 1024.0, 3) + " KB";
        else if (bytes < (1024 * 1024 * 1024))
          fsize = String(bytes / 1024.0 / 1024.0, 3) + " MB";
        else
          fsize = String(bytes / 1024.0 / 1024.0 / 1024.0, 3) + " GB";
        webpage += "<td>" + fsize + "</td></tr>";
        f.close();
      }
      str += String(dir.fileSize());
      str += "\r\n";
      Serial.println(str);
    }
    webpage += F("</table>");
    SendHTML_Content();
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop(); // Stop is needed because no content length was sent
  }
  else
    ReportSPIFFSNotPresent();
}
#endif
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SPIFFS_file_stream(String filename)
{
  if (SPIFFS_present)
  {
    File dataFile = SPIFFS.open("/" + filename, "r"); // Now read data from SPIFFS Card
    if (dataFile)
    {
      if (dataFile.available())
      { // If data is available and present
        String dataType = "application/octet-stream";
        if (server.streamFile(dataFile, dataType) != dataFile.size())
        {
          Serial.print(F("Sent less data than expected!"));
        }
      }
      dataFile.close(); // close the file:
    }
    else
      ReportFileNotPresent("Cstream");
  }
  else
    ReportSPIFFSNotPresent();
}
void File_Stream()
{
  if (server.args() > 0)
  { // Arguments were received
    if (server.hasArg("stream"))
      SPIFFS_file_stream(server.arg(0));
  }
  else
    SelectInput("Enter a File to Stream", "stream", "stream");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SPIFFS_file_delete(String filename)
{ // Delete the file
  if (SPIFFS_present)
  {
    SendHTML_Header();
    File dataFile = SPIFFS.open("/" + filename, "r"); // Now read data from SPIFFS Card
    if (dataFile)
    {
      if (SPIFFS.remove("/" + filename))
      {
        Serial.println(F("File deleted successfully"));
        webpage += "<h3>File '" + filename + "' has been erased</h3>";
        webpage += F("<a href='/delete'>[Back]</a><br><br>");
      }
      else
      {
        webpage += F("<h3>File was not deleted - error</h3>");
        webpage += F("<a href='delete'>[Back]</a><br><br>");
      }
    }
    else
      ReportFileNotPresent("delete");
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();
  }
  else
    ReportSPIFFSNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Delete()
{
  if (server.args() > 0)
  { // Arguments were received
    if (server.hasArg("delete"))
      SPIFFS_file_delete(server.arg(0));
  }
  else
    SelectInput("Select a File to Delete", "delete", "delete");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void createDir(fs::FS &fs, const char *path)
{
  Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path))
  {
    Serial.println("Dir created");
  }
  else
  {
    Serial.println("mkdir failed");
  }
}
void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.name(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

float_t *getReadings()
{

  yawData[0] = Serial2.read();
  yawData[1] = Serial2.read();
  yawData[2] = Serial2.read();
  yawData[3] = Serial2.read();
  float yawTemp = *((float *)(yawData));

  yawData[0] = Serial2.read();
  yawData[1] = Serial2.read();
  yawData[2] = Serial2.read();
  yawData[3] = Serial2.read();
  float yawHum = *((float *)(yawData));
  /*if (isnan(yawTemp) || isnan(yawHum))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return
  }*/
  Serial.print("Temperature 1: ");
  Serial.println(yawTemp);
  Serial.print("Humidity 1: ");
  Serial.println(yawHum);

  static float_t values[2];
  values[0] = yawTemp;
  values[1] = yawHum;

  return values;
}

float_t *getReadings0()
{

  yawData0[0] = Serial1.read();
  yawData0[1] = Serial1.read();
  yawData0[2] = Serial1.read();
  yawData0[3] = Serial1.read();
  float yawTemp0 = *((float *)(yawData0));

  yawData0[0] = Serial1.read();
  yawData0[1] = Serial1.read();
  yawData0[2] = Serial1.read();
  yawData0[3] = Serial1.read();
  float yawHum0 = *((float *)(yawData0));
  /*
  if (isnan(yawTemp0) || isnan(yawHum0))
  {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }*/
  Serial.print("Temperature 2: ");
  Serial.println(yawTemp0);
  Serial.print("Humidity 2: ");
  Serial.println(yawHum0);

  static float_t values0[2];
  values0[0] = yawTemp0;
  values0[1] = yawHum0;

  return values0;
}

//____________________________________________

void getTimeStamp()
{
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }

  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.println(timeStamp);
} // Write to the SD card (DON'T MODIFY THIS FUNCTION)

void getTimeStamp0()
{
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }

  formattedDate = timeClient.getFormattedDate();
  Serial.println(formattedDate);

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  Serial.println(dayStamp);
  // Extract time
  timeStamp = formattedDate.substring(splitT + 1, formattedDate.length() - 1);
  Serial.println(timeStamp);
} // Write to the SD card (DON'T MODIFY THIS FUNCTION)

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

// Append data to the SD card (DON'T MODIFY THIS FUNCTION)
void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

void logSDCard1(String str)
{
  dataMessage = String(str) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage);
  appendFile(SD, "/data.txt", dataMessage.c_str());
}

void logSDCard2(String str2)
{
  dataMessage2 = String(str2) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage2);
  appendFile(SD, "/data.txt", dataMessage2.c_str());
}

void logSDCard3(String str1)
{
  dataMessage3 = String(str1) + "\r\n";
  Serial.print("Save data: ");
  Serial.println(dataMessage3);
  appendFile(SD, "/data.txt", dataMessage3.c_str());
}

//____________________________________________________________

//____________________________________________________________________

// All supporting functions from here...
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup()
{
  esp_log_level_set("*", ESP_LOG_ERROR);    // set all components to ERROR level
  esp_log_level_set("wifi", ESP_LOG_WARN);  // enable WARN logs from WiFi stack
  esp_log_level_set("dhcpc", ESP_LOG_INFO); // enable INFO logs from DHCP client
  Serial.begin(115200);
  Serial.println("Connecting to ");
  Serial.println(ssid);
  pinMode(PIN_AP, INPUT);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial1.begin(9600, SERIAL_8N1, RXD1, TXD1);

  Serial.println("Serial Txd is on pin: " + String(TX));
  Serial.println("Serial Rxd is on pin: " + String(RX));

  Serial.println(F("DHTxx test!"));
  SD.begin();

  //connect to your local wi-fi network
  WiFi.softAP(ssid, password);

  delay(20);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  server.begin();
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("\nConnected to " + WiFi.SSID() + " Use IP address: " + WiFi.localIP().toString()); // Report which SSID and IP is in use
#ifdef ESP8266
  if (!SPIFFS.begin())
  {
#else
  if (!SPIFFS.begin(true))
  {
#endif
    Serial.println("SPIFFS initialisation failed...");
    SPIFFS_present = false;
  }
  else
  {
    Serial.println(F("SPIFFS initialised... file access enabled..."));
    SPIFFS_present = true;
  }
  //----------------------------------------------------------------------
  ///////////////////////////// Server Commands
  server.on("/", HomePage);
  server.on("/download", File_Download);
  server.on("/upload", File_Upload);
  server.on(
      "/fupload", HTTP_POST, []() { server.send(200); }, handleFileUpload);
  server.on("/stream", File_Stream);
  server.on("/delete", File_Delete);
  server.on("/dir", SPIFFS_dir);

  listDir(SD, "/", 0);
  createDir(SD, "/stream");
  if (WiFi.status() == WL_CONNECTED)
  {

    // Print ESP32 Local IP Address
    Serial.println(WiFi.localIP());

    timeClient.begin();
    // Set offset time in seconds to adjust for your timezone, for example:
    // GMT +1 = 3600
    // GMT +8 = 28800
    // GMT -1 = -3600
    // GMT 0 = 0
    timeClient.setTimeOffset(-14400);
    timeClient.update();
    //RTC
    while (!timeClient.update())
    {

      timeClient.forceUpdate();
    }
    //internet
    formattedDate = timeClient.getFormattedDate();
    String FechaNTP;
    FechaNTP = timeClient.getFormattedDate();
    int a, m, d, h, n, s;

    a = FechaNTP.substring(0, 4).toInt();

    m = FechaNTP.substring(5, 7).toInt();

    d = FechaNTP.substring(8, 10).toInt();

    h = FechaNTP.substring(11, 13).toInt();

    n = FechaNTP.substring(14, 16).toInt();

    s = FechaNTP.substring(17, 19).toInt();

    RTC.adjust(DateTime(a, m, d, h, n, s));
    DateTime now = RTC.now(); // Obtiene la fecha y hora del RTC
    Serial.println(formattedDate);
    getTimeStamp();
  }

  ///////////////////////////// End of Request commands
  server.begin();
  Serial.println("HTTP server started");
  delay(500);
  RTC.begin(); // Inicia la comunicación (con el RTC
  delay(100);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop(void)
{

  //DateTime now = RTC.now(); // Obtiene la fecha y hora del RTC
  delay(200);

  //____________________________________________
  int i = 0;
  int sum = 0;
  int offset = 0;
  Serial.println("init...");
  //calibración inicial para obtener el offset

  while (i < 20)
  {

    sensorValue = analogRead(sensorPin) - 1792; //-1792valor analogo medio
    sum += sensorValue;
    delay(50);
    i++;
  }
  offset = sum / 20;
  float temp_cal = 0;
  float hum_cal = 0;
  /*

  int x = 0;

  int u = 0;
  int v = 0;
  float sum_temp_1 = 0;
  float sum_hum_1 = 0;
  float prom_temp_1 = 0;
  float prom_hum_1 = 0;
  float sum_temp_2 = 0;
  float sum_hum_2 = 0;
  float prom_temp_2 = 0;
  float prom_hum_2 = 0;

  if (x == 0)
  {
    while (u < 5)
    {
      if (Serial2.available())
      {

        float_t *returned = getReadings();
        delay(50);
        temp_1 = returned[0];
        hum_1 = returned[1];
        sum_temp_1 += temp_1;
        sum_hum_1 += hum_1;
        delay(2000);
        u++;
        //si ocurre error repetir calibración
      }
    }

    while (v < 5)
    {
      if (Serial1.available())
      {

        float_t *returned0 = getReadings0();
        delay(50);
        temp_0 = returned0[0];
        hum_0 = returned0[1];
        sum_temp_2 += temp_0;
        sum_hum_2 += hum_0;
        delay(2000);
        v++;
      }

      //si ocurre error repetir calibración
    }
    prom_temp_1 = sum_temp_1 / 5;
    prom_hum_1 = sum_hum_1 / 5;
    prom_temp_2 = sum_temp_2 / 5;
    prom_hum_2 = sum_hum_2 / 5;
    temp_cal = prom_temp_1 - prom_temp_2;
    hum_cal = prom_hum_1 - prom_hum_2;

    x++;
    Serial.println("Calibración: ");
    Serial.println("Offset ");
    Serial.println(offset);
    Serial.println("Temperatura1");
    Serial.println(prom_temp_1);
    Serial.println("Humedad1");
    Serial.println(prom_hum_1);
    Serial.println("Temperatura2");
    Serial.println(prom_temp_2);
    Serial.println("Humedad2");
    Serial.println(prom_hum_2);
  }
  */
  if (Serial2.available())
  {

    float_t *returned = getReadings();
    delay(50);
    temp_1 = returned[0];
    hum_1 = returned[1];

    delay(2000);

    //si ocurre error repetir calibración
  }

  if (Serial1.available())
  {

    float_t *returned0 = getReadings0();
    delay(50);
    temp_0 = returned0[0];
    hum_0 = returned0[1];

    delay(2000);
  }

  temp_cal = temp_1 - temp_0;
  hum_cal = hum_1 - hum_0;
  Serial.println("Temp_cal: ");
  Serial.println(temp_cal);
  Serial.println("Hum_cal: ");
  Serial.println(hum_cal);
  delay(1000);
  while (1)
  {

    int u = 0;
    float IFA = 0;
    float sum2 = 0;
    float IFA2 = 0;
    float bits = 0;

    sensorValue = analogRead(sensorPin); //sensorValue = analogRead(sensorPin) - offset; - 1792 - offset
    Serial.println("BITS***: ");
    Serial.println(sensorValue);
    //IFA = (0.0013 * sensorValue - 2.1225) * 1000; //Factor flujo de aire - Presion en Pa
    float Vout1 = (2.887 * sensorValue) / 3583;
    for (u = 0; u < 1000; u++)
    {
      IFA = 1.5823 * sensorValue - 2855.1;

      // IFA = (0.0013 * sensorValue - 2.1225) * 1000; //valor analogo medio
      sum2 += IFA;
    }
    IFA2 = sum2 / 1000;
    //IFA2 = IFA2 + 146.5; //delta 181 Pa
    float IFAmmh2o = IFA2 * (1 / 9.8);

    Serial.println("Indice de Flujo de Aire en PA: ");
    Serial.println(IFA2);
    Serial.println("Indice de Flujo de Aire en PA: ");

    Serial.println("Indice de Flujo de Aire en mmH20: ");
    Serial.println(IFAmmh2o);

    DateTime now = RTC.now(); // Obtiene la fecha y hora del RTC
    delay(200);
    if (!RTC.isrunning())
    {
      Serial.println("RTC is not running");
      RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    /*if (!RTC.begin())
    {
      Serial.println("Couldn't find RTC");
      while (1);
    }*/
    char str[50]; //declara la string como arreglo de chars

    sprintf(str, "Pres_Dif_1 %02d-%02d-%02d %02d:%02d:%02d %s %s %s", //%d allows to print an integer to the string
            now.year(),                                               //get year method
            now.month(),                                              //get month method
            now.day(),                                                //get day method
            now.hour(),                                               //get hour method
            now.minute(),                                             //get minute method
            now.second(),
            String(IFA2).c_str(),
            String(sensorValue).c_str(),
            String(Vout1).c_str() //get second method
    );

    delay(1000);
    logSDCard1(str);
    //Serial.flush();
    delay(3000);

    if (Serial2.available())
    {

      float_t *returned = getReadings();
      temp_1 = returned[0] - 0.4 - temp_cal; //0.7 diferencia con respecto a temp de referencia de sensor de temp
      hum_1 = returned[1] - 4.7 - hum_cal;   //4.8 diferencia con respecto a hum de referencia de sensor de hum
      Serial.print("Temperature_2: ");
      Serial.println(temp_1); // prints 1

      Serial.print("Humidity_2: ");
      Serial.println(hum_1); // prints 2

      DateTime now = RTC.now(); // Obtiene la fecha y hora del RTC
      delay(200);
      char str2[50]; //declara la string como arreglo de chars

      sprintf(str2, "Temp_Hum_2 %02d-%02d-%02d %02d:%02d:%02d %s %s", //%d allows to print an integer to the string
              now.year(),                                             //get year method
              now.month(),                                            //get month method
              now.day(),                                              //get day method
              now.hour(),                                             //get hour method
              now.minute(),                                           //get minute method
              now.second(),
              String(temp_1).c_str(),
              String(hum_1).c_str() //get second method
      );
      //Serial.println(str2); // prints 2

      //getTimeStamp();
      delay(2000);
      logSDCard2(str2);

      //Serial2.flush();
      delay(3000);
      //0.9 dif
    }

    if (Serial1.available())
    {

      float_t *returned0 = getReadings0();
      temp_0 = returned0[0] - 0.4; //0.4 diferencia con respecto a temp de referencia de sensor de temp
      hum_0 = returned0[1] - 4.7;  //4.7 diferencia con respecto a hum de referencia de sensor de hum
      Serial.print("Temperature_1: ");
      Serial.println(temp_0); // prints 1

      Serial.print("Humidity_1: ");
      Serial.println(hum_0); // prints 2

      DateTime now = RTC.now(); // Obtiene la fecha y hora del RTC
      delay(2000);
      char str1[50]; //declara la string como arreglo de chars

      sprintf(str1, "Temp_Hum_1 %02d-%02d-%02d %02d:%02d:%02d %s %s", //%d allows to print an integer to the string
              now.year(),                                             //get year method
              now.month(),                                            //get month method
              now.day(),                                              //get day method
              now.hour(),                                             //get hour method
              now.minute(),                                           //get minute method
              now.second(),
              String(temp_0).c_str(),
              String(hum_0).c_str() //get second method
      );
      // Serial.println(str1); // prints 2
      //getTimeStamp0();
      delay(2000);
      logSDCard3(str1);

      //Serial1.flush();
      delay(3000);
    }
    /*while (!timeClient.update())
    {
      timeClient.forceUpdate();
    }*/

    while (digitalRead(PIN_AP) == HIGH)
    {

      server.handleClient();

      if (!SD.begin())
      {
        SD.begin();
      }
      if (!SPIFFS.begin())
      {
        SPIFFS.begin();
      }
      delay(50);
      File sourceFile = SD.open("/data.txt");
      File destFile = SPIFFS.open("/data.txt", FILE_WRITE);
      static uint8_t buf[512];
      while (sourceFile.read(buf, 512))
      {
        destFile.write(buf, 512);
      }
      destFile.close();
      sourceFile.close();
      delay(50);
    }
  }
}
//asasddddd
