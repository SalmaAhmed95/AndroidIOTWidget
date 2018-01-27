#include <Arduino.h>
#include <DS3231.h>
#include <string.h>
#include <Adafruit_GFX.h>
#include "SWTFT.h"
#include <SD.h>
#define POST_ID "post"
#define REM_ID "rem"
#define TEMP_ID "temp"
#define CLK_ID "date"
#define DELIM ';'
#define SD_CS 53
#define R_PIN 12
#define G_PIN 13
#define B_PIN 11
#define MAX 10
#define BUFFPIXEL 20
#define HR_X 53
#define HR_Y 190
#define DAY_X 65
#define DAY_Y 170
#define DATE_X 57
#define DATE_Y 150
#define IMAGE_SIZE 48
#define IMAGE_X 95
#define IMAGE_Y 20
#define TEMP_X 30
#define TEMP_Y 260
#define NOTIFICATION_X 33
#define NOTIFICATION_Y 120

struct notification {
  String name;
  bool posted;
  int r, g, b;
  int count;
  int entryTime;
};

struct notification notifs[10];

int notificationCount = 0;
int i = 0;
int counter = 0;
int curEntry = 0;
int no_notif = true;
DS3231 rtc(20, 21);
SWTFT tft;

//TFT variables
Time currentTime;
uint8_t phour, pminutes, pseconds;

void parseMsg(String msg);
void postNotif(String msg);
void remNotif(String msg);
void setupTemp(String msg);
void setupClk(String msg);
void illuminate(int r, int g, int b);
void displayTime();
void displayNotifs();
void print_notification(String notif, int r, int g, int b,int count);
int DOWtoInt(String DOW);

void initNotifs() {
  for (int j = 0; j < MAX; j++)
    notifs[j] = {"", false, 0, 0, 0, 0, 0};
}

void setupTFT() {
  tft.reset();
  uint16_t identifier = tft.readID();
  tft.begin(identifier);
  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(SD_CS)) {
    Serial.println(F("failed!"));
    return;
  }
  Serial.println(F("OK!"));
  tft.setRotation(4);
  tft.fillScreen(0xFFFF);
  tft.setTextColor(0x0000);
  tft.setTextSize(5);
  bmpDraw("weather.bmp",TEMP_X, TEMP_Y);
}

void setupRTC (uint8_t dayName, uint8_t hr, uint8_t minutes,
               uint8_t seconds, uint8_t day, uint8_t month, uint16_t yr) {
  tft.fillRect(HR_X, HR_Y, 150, 60, 0xFFFF);
  tft.fillRect(DAY_X, DAY_Y, 150, 30, 0xFFFF);
  tft.fillRect(DATE_X, DATE_Y, 150, 30, 0xFFFF);
  rtc.setDOW(dayName);     // Set Day-of-Week to SUNDAY
  rtc.setTime(hr, minutes, seconds);     // Set the time to 12:00:00 (24hr format)
  rtc.setDate(day, month, yr);   // Set the date to January 1st, 2014


  tft.setTextSize(2);
  tft.setCursor(DAY_X, DAY_Y);
  tft.print(rtc.getDOWStr());
  tft.setCursor(DATE_X, DATE_Y);
  tft.print(rtc.getDateStr());

  phour = hr;
  pminutes = minutes;
  pseconds = seconds;

}

void setup() {
  Serial.begin(9600);         //Sets the data rate in bits per second (baud) for serial data transmission

  pinMode(13, OUTPUT);        //Sets digital pin 13 as output pin
  pinMode(R_PIN, OUTPUT);
  pinMode(G_PIN, OUTPUT);
  pinMode(B_PIN, OUTPUT);

  initNotifs();
  setupTFT();
   rtc.begin();
  setupRTC(SUNDAY, 0, 0, 0, 28, 1, 2018);
}

void loop() {
  if (Serial.available() > 0) {
    String msg = Serial.readStringUntil((char) 3); //EOT
    Serial.print("msg: ");
    Serial.println(msg);
    parseMsg(msg);
  }
  
  displayTime();
  displayNotifs();
}

void parseMsg(String msg) {
  int index = msg.indexOf(DELIM);
  int nextMsgIndex = msg.indexOf('|');

  if (index == -1) {
    return;
  }
  String header = msg.substring(0, index);
  
  if (header.equals(POST_ID))
    postNotif(msg.substring(index + 1, nextMsgIndex));
  else if (header.equals(REM_ID))
    remNotif(msg.substring(index + 1, nextMsgIndex));
  else if (header.equals(TEMP_ID))
    setupTemp(msg.substring(index + 1, nextMsgIndex));
  else if (header.equals(CLK_ID))
    setupClk(msg.substring(index + 1, nextMsgIndex));

  if (nextMsgIndex != -1 && nextMsgIndex != msg.length() - 1)
    parseMsg(msg.substring(nextMsgIndex + 1));
}

void postNotif(String msg) {
  String name;
  int r, g, b;
  int index[3];

  index[0] = msg.indexOf(DELIM);
  for (int j = 1; j < 3; j++)
    index[j] = msg.indexOf(DELIM, index[j - 1] + 1);

  name = msg.substring(0, index[0]);
  r = msg.substring(index[0] + 1, index[1]).toInt();
  g = msg.substring(index[1] + 1, index[2]).toInt();
  b = msg.substring(index[2] + 1).toInt();

  Serial.println("name: " + name + "(R,G,B): (" + r + ", " + g + ", " + b + ")");

  // Put a new notification in queue.
  // TODO: Remove popped notification (if a popped notification exists).
  int indToPost = -1;
  int minEntryInd = 0;
  int count = 0;

  for (int j = 0; j < MAX; j++) {
    if (notifs[j].name.equals(name)) {
      indToPost = j;
      count = notifs[j].count;
      break;
    } else if (!notifs[j].posted)
      indToPost = j;
    else if (notifs[j].entryTime < notifs[minEntryInd].entryTime)
      minEntryInd = j;
  }
  indToPost = indToPost != -1 ? indToPost : minEntryInd;

  notifs[indToPost] = {name, true, r, g, b, ++count, curEntry++};
}

void remNotif(String msg) {
  String name = msg;
  Serial.print("name: "); Serial.println(msg);

  for (int j = 0; j < MAX; j++)
    if (notifs[j].name.equals(name))
      notifs[j].posted = false;
}

void setupClk(String msg) {
  int dd, mm, yy, hr, mn, ss, dw;
  int index[6];

  index[0] = msg.indexOf(DELIM);
  for (int j = 1; j < 6; j++)
    index[j] = msg.indexOf(DELIM, index[j - 1] + 1);

  dd = msg.substring(0, index[0]).toInt();
  mm = msg.substring(index[0] + 1, index[1]).toInt();
  yy = msg.substring(index[1] + 1, index[2]).toInt();
  hr = msg.substring(index[2] + 1, index[3]).toInt();
  mn = msg.substring(index[3] + 1, index[4]).toInt();
  ss = msg.substring(index[4] + 1, index[5]).toInt();
  dw = DOWtoInt(msg.substring(index[5] + 1));

  setupRTC(dw, hr, mn, ss, dd, mm, yy);
}

void setupTemp(String msg) {
  int temp = msg.toInt();
  tft.setCursor(TEMP_X + 70,TEMP_Y + 20);
  tft.setTextSize(2);
  tft.print(msg);
  tft.print("C");
  
}

void illuminate(int r, int g, int b) {
  analogWrite(R_PIN, r);
  analogWrite(G_PIN, g);
  analogWrite(B_PIN, b);
}

int DOWtoInt(String DOW) {
      if (DOW.equals("Sat")) {
      return SATURDAY;
      }
     if(DOW.equals("Sun")) {
      return SUNDAY;
      }
     if(DOW.equals("Mon")) {
      return MONDAY;
      }
      if(DOW.equals("Tue")) {
      return TUESDAY;
      }
      if(DOW.equals("Wed")) {
      return WEDNESDAY;
      }
       if(DOW.equals("Thu")) {
      return THURSDAY;
      }
      if(DOW.equals("Fri")) {
      return FRIDAY;
      }
      return -1;
}
void displayNotifs() {
  if (notifs[i].posted) {
    no_notif = false;
    if (counter == 100) {
      //clear text
      tft.fillRect(NOTIFICATION_X + 40, NOTIFICATION_Y - 20, 110, 20, 0xFFFF);
      counter = 0;
      i = (i + 1) % 10;
    } else {
      // drawIcon region
      if (counter == 0) {
        print_notification(notifs[i].name, notifs[i].r, notifs[i].g, notifs[i].b,notifs[i].count);
        String notif_name=notifs[i].name;
        notif_name.toLowerCase();
        String imageFile = notif_name + ".bmp";
        char buff[50];
        imageFile.toCharArray(buff, 50);
        Serial.println(buff);
        bmpDraw(buff, IMAGE_X, IMAGE_Y);
        illuminate(notifs[i].r, notifs[i].g, notifs[i].b);
      }
      counter++;
    }
  } else {
    // Clear image.
    tft.fillRect(IMAGE_X, IMAGE_Y, IMAGE_SIZE, IMAGE_SIZE, 0xFFFF);
    //clear all text
    if (i == 9 && no_notif) {
      tft.fillRect(NOTIFICATION_X, NOTIFICATION_Y - 70, 220, 90, 0xFFFF);
       illuminate(0, 0, 0);
    }
   
    i = (i + 1) % 10;
    if ( i == 0) {
        no_notif = true;
      }
  }
}

void displayTime() {
  tft.setCursor(HR_X , HR_Y);
  tft.setTextSize(5);
  tft.setTextColor(0);
  int16_t  x1, y1;
  uint16_t w, h;
  uint8_t chour, cminutes, cseconds;
  currentTime = rtc.getTime();
  chour = currentTime.hour;
  cminutes = currentTime.min;
  cseconds = currentTime.sec;

  if ( chour != phour) {
    tft.fillRect(HR_X, HR_Y, 80, 60, 0xFFFF);
    phour = chour;
  }
  if (cminutes != pminutes) {
    tft.fillRect(HR_X + 70, HR_Y + 0, 80, 60, 0xFFFF);
    pminutes = cminutes;
  }
  if (cseconds != pseconds) {

    pseconds = cseconds;
  }
  String clk = String(rtc.getTimeStr());
  clk = clk.substring(0, 5);
  tft.println(clk);

}
void print_notification(String notif, int r , int g, int b,int count) {
  tft.setTextSize(2);
  int rgb = r;
  rgb = (rgb << 8) + g;
  rgb = (rgb << 8) + b;
  tft.setTextColor(rgb);
  tft.setCursor(NOTIFICATION_X, NOTIFICATION_Y);
  tft.print("You have ");
  tft.fillRect(NOTIFICATION_X + 105,NOTIFICATION_Y,15,20,0xFFFF); 
  tft.print(count);
  tft.print(" new");
  tft.setCursor(NOTIFICATION_X + 40, NOTIFICATION_Y - 20);
  tft.print(notif);
  tft.setCursor(NOTIFICATION_X + 20, NOTIFICATION_Y - 40);
  tft.print("notification!");
}
void bmpDraw(char *filename, int x, int y) {
  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3 * BUFFPIXEL]; // pixel in buffer (R+G+B per pixel)
  uint16_t lcdbuffer[BUFFPIXEL];  // pixel out buffer (16-bit per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0, startTime = millis();
  uint8_t  lcdidx = 0;
  boolean  first = true;

  if ((x >= tft.width()) || (y >= tft.height())) return;

  Serial.println();
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');
  if (!strcmp(filename, "messenger.bmp")) {
       filename = "facebook.bmp";
    }
  
  // Open requested file on SD card
  if ((bmpFile = SD.open(filename)) == NULL) {
    Serial.println(F("File not found"));
    return;
  }

  // Parse BMP header
  if (read16(bmpFile) == 0x4D42) { // BMP signature
    Serial.println(F("File size: ")); Serial.println(read32(bmpFile));
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    Serial.print(F("Image Offset: ")); Serial.println(bmpImageoffset, DEC);
    // Read DIB header
    Serial.print(F("Header size: ")); Serial.println(read32(bmpFile));
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if (read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      Serial.print(F("Bit Depth: ")); Serial.println(bmpDepth);
      if ((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        Serial.print(F("Image size: "));
        Serial.print(bmpWidth);
        Serial.print('x');
        Serial.println(bmpHeight);

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if (bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if ((x + w - 1) >= tft.width())  w = tft.width()  - x;
        if ((y + h - 1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x + w - 1, y + h - 1);

        for (row = 0; row < h; row++) { // For each scanline...
          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if (flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if (bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col = 0; col < w; col++) { // For each column...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              // Push LCD buffer to the display first
              if (lcdidx > 0) {
                tft.pushColors(lcdbuffer, lcdidx, first);
                lcdidx = 0;
                first  = false;
              }
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            lcdbuffer[lcdidx++] = tft.color565(r, g, b);
          } // end pixel
        } // end scanline
        // Write any remaining data to LCD
        if (lcdidx > 0) {
          tft.pushColors(lcdbuffer, lcdidx, first);
        }
        Serial.print(F("Loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(" ms");
      } // end goodBmp
    }
  }

  bmpFile.close();
  if (!goodBmp) Serial.println(F("BMP format not recognized."));
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}
