/*
 * ESP8266 based virtual modem
 * Copyright (C) 2016 Jussi Salin <salinjus@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ESP8266WiFi.h>
#include <algorithm>

// Global variables
String cmd = "";           // Gather a new AT command to this string from serial
bool cmdMode = true;       // Are we in AT command mode or connected mode
bool telnet = true;        // Is telnet control code handling enabled
#define SWITCH_PIN 0       // GPIO0 (programmind mode pin)
#define DEFAULT_BPS 115200 // 2400 safe for all old computers including C64
//#define USE_SWITCH 1     // Use a software reset switch
//#define DEBUG 1          // Print additional debug information to serial channel
#undef DEBUG
#undef USE_SWITCH
#define LISTEN_PORT 23     // Listen to this if not connected. Set to zero to disable.
#define RING_INTERVAL 3000 // How often to print RING when having a new incoming connection (ms)
WiFiClient tcpClient;
WiFiServer tcpServer(LISTEN_PORT);
unsigned long lastRingMs=0;// Time of last "RING" message (millis())
long myBps;                // What is the current BPS setting
#define MAX_CMD_LENGTH 256 // Maximum length for AT command
char plusCount = 0;        // Go to AT mode at "+++" sequence, that has to be counted
#define LED_PIN 2          // Status LED
#define LED_TIME 1         // How many ms to keep LED on at activity
unsigned long ledTime = 0;
#define TX_BUF_SIZE 256    // Buffer where to read from serial before writing to TCP
                           // (that direction is very blocking by the ESP TCP stack,
                           // so we can't do one byte a time.)
uint8_t txBuf[TX_BUF_SIZE];

// Telnet codes
#define DO 0xfd
#define WONT 0xfc
#define WILL 0xfb
#define DONT 0xfe

/**
 * Arduino main init function
 */
void setup()
{
  Serial.begin(DEFAULT_BPS);
  myBps = DEFAULT_BPS;

#ifdef USE_SWITCH
  pinMode(SWITCH_PIN, INPUT);
  digitalWrite(SWITCH_PIN, HIGH);
#endif

  Serial.println("Virtual modem");
  Serial.println("=============");
  Serial.println();
  Serial.println("Connect to WIFI: ATWIFI<ssid>,<key>");
  Serial.println("Change terminal baud rate: AT<baud>");
  Serial.println("Connect by TCP: ATDT<host>:<port>");
  Serial.println("See my IP address: ATIP");
  Serial.println("Disable telnet command handling: ATNET0");
  Serial.println("HTTP GET: ATGET<URL>");
  Serial.println();
  if (LISTEN_PORT > 0)
  {
    Serial.print("Listening to connections at port ");
    Serial.print(LISTEN_PORT);
    Serial.println(", which result in RING and you can answer with ATA.");
    tcpServer.begin();
  }
  else
  {
    Serial.println("Incoming connections are disabled.");
  }
  Serial.println("");
  Serial.println("OK");

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
}

/**
 * Turn on the LED and store the time, so the LED will be shortly after turned off
 */
void led_on(void)
{
  digitalWrite(LED_PIN, LOW);
  ledTime = millis();
}

/**
 * Perform a command given in command mode
 */
void command()
{
  cmd.trim();
  if (cmd == "") return;
  Serial.println();
  String upCmd = cmd;
  upCmd.toUpperCase();

  long newBps = 0;

  /**** Just AT ****/
  if (upCmd == "AT") Serial.println("OK");
  
  /**** Dial to host ****/
  else if ((upCmd.indexOf("ATDT") == 0) || (upCmd.indexOf("ATDP") == 0) || (upCmd.indexOf("ATDI") == 0))
  {
    int portIndex = cmd.indexOf(":");
    String host, port;
    if (portIndex != -1)
    {
      host = cmd.substring(4, portIndex);
      port = cmd.substring(portIndex + 1, cmd.length());
    }
    else
    {
      host = cmd.substring(4, cmd.length());
      port = "23"; // Telnet default
    }
    Serial.print("Connecting to ");
    Serial.print(host);
    Serial.print(":");
    Serial.println(port);
    char *hostChr = new char[host.length() + 1];
    host.toCharArray(hostChr, host.length() + 1);
    int portInt = port.toInt();
    tcpClient.setNoDelay(true); // Try to disable naggle
    if (tcpClient.connect(hostChr, portInt))
    {
      tcpClient.setNoDelay(true); // Try to disable naggle
      Serial.print("CONNECT ");
      Serial.println(myBps);
      cmdMode = false;
      Serial.flush();
      if (LISTEN_PORT > 0) tcpServer.stop();
    }
    else
    {
      Serial.println("NO CARRIER");
    }
    delete hostChr;
  }

  /**** Connect to WIFI ****/
  else if (upCmd.indexOf("ATWIFI") == 0)
  {
    int keyIndex = cmd.indexOf(",");
    String ssid, key;
    if (keyIndex != -1)
    {
      ssid = cmd.substring(6, keyIndex);
      key = cmd.substring(keyIndex + 1, cmd.length());
    }
    else
    {
      ssid = cmd.substring(6, cmd.length());
      key = "";
    }
    char *ssidChr = new char[ssid.length() + 1];
    ssid.toCharArray(ssidChr, ssid.length() + 1);
    char *keyChr = new char[key.length() + 1];
    key.toCharArray(keyChr, key.length() + 1);
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.print("/");
    Serial.println(key);
    WiFi.begin(ssidChr, keyChr);
    for (int i=0; i<100; i++)
    {
      delay(100);
      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("OK");
        break;
      }
    }
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("ERROR");
    }
    delete ssidChr;
    delete keyChr;
  }

  /**** Change baud rate from default ****/
  else if (upCmd == "AT300") newBps = 300;
  else if (upCmd == "AT1200") newBps = 1200;
  else if (upCmd == "AT2400") newBps = 2400;
  else if (upCmd == "AT9600") newBps = 9600;
  else if (upCmd == "AT19200") newBps = 19200;
  else if (upCmd == "AT38400") newBps = 38400;
  else if (upCmd == "AT57600") newBps = 57600;
  else if (upCmd == "AT115200") newBps = 115200;

  /**** Change telnet mode ****/
  else if (upCmd == "ATNET0")
  {
    telnet = false;
    Serial.println("OK");
  }
  else if (upCmd == "ATNET1")
  {
    telnet = true;
    Serial.println("OK");
  }

  /**** Answer to incoming connection ****/
  else if ((upCmd == "ATA") && tcpServer.hasClient())
  {
    tcpClient = tcpServer.available();
    tcpClient.setNoDelay(true); // try to disable naggle
    tcpServer.stop();
    Serial.print("CONNECT ");
    Serial.println(myBps);
    cmdMode = false;
    Serial.flush();
  }

  /**** See my IP address ****/
  else if (upCmd == "ATIP")
  {
    Serial.println(WiFi.localIP());
    Serial.println("OK");
  }

  /**** HTTP GET request ****/
  else if (upCmd.indexOf("ATGET") == 0)
  {
    // From the URL, aquire required variables
    // (12 = "ATGEThttp://")
    int portIndex = cmd.indexOf(":", 12); // Index where port number might begin
    int pathIndex = cmd.indexOf("/", 12); // Index first host name and possible port ends and path begins
    int port;
    String path, host;
    if (pathIndex < 0)
    {
      pathIndex = cmd.length();
    }
    if (portIndex < 0)
    {
      port = 80;
      portIndex = pathIndex;
    }
    else
    {
      port = cmd.substring(portIndex+1, pathIndex).toInt();
    }
    host = cmd.substring(12, portIndex);
    path = cmd.substring(pathIndex, cmd.length());
    if (path == "") path = "/";
    char *hostChr = new char[host.length() + 1];
    host.toCharArray(hostChr, host.length() + 1);

    // Debug
    Serial.print("Getting path ");
    Serial.print(path);
    Serial.print(" from port ");
    Serial.print(port);
    Serial.print(" of host ");
    Serial.print(host);
    Serial.println("...");

    // Establish connection
    if (!tcpClient.connect(hostChr, port))
    {
      Serial.println("NO CARRIER");
    }
    else
    {
      Serial.print("CONNECT ");
      Serial.println(myBps);
      cmdMode = false;

      // Send a HTTP request before continuing the connection as usual
      String request = "GET ";
      request += path;
      request += " HTTP/1.1\r\nHost: ";
      request += host;
      request += "\r\nConnection: close\r\n\r\n";
      tcpClient.print(request);
    }
    delete hostChr;
  }

  /**** Unknown command ****/
  else Serial.println("ERROR");

  /**** Tasks to do after command has been parsed ****/
  if (newBps)
  {
    Serial.println("OK");
    delay(150); // Sleep enough for 4 bytes at any previous baud rate to finish ("\nOK\n")
    Serial.begin(newBps);
    myBps = newBps;
  }

  cmd = "";
}

/**
 * Arduino main loop function
 */     
void loop()
{
  /**** AT command mode ****/
  if (cmdMode == true)
  {
    // In command mode but new unanswered incoming connection on server listen socket
    if ((LISTEN_PORT > 0) && (tcpServer.hasClient()))
    {
      // Print RING every now and then while the new incoming connection exists
      if ((millis() - lastRingMs) > RING_INTERVAL)
      {
        Serial.println("RING");
        lastRingMs = millis();
      }
    }
    
    // In command mode - don't exchange with TCP but gather characters to a string
    if (Serial.available())
    {
      char chr = Serial.read();

      // Return, enter, new line, carriage return.. anything goes to end the command
      if ((chr == '\n') || (chr == '\r')) 
      {
        command();
      }
      // Backspace or delete deletes previous character
      else if ((chr == 8) || (chr == 127))
      {
        cmd.remove(cmd.length() - 1);
        // We don't assume that backspace is destructive
        // Clear with a space
        Serial.write(8);
        Serial.write(' ');
        Serial.write(8);
      }
      else
      {
        if (cmd.length() < MAX_CMD_LENGTH) cmd.concat(chr);
        Serial.print(chr);
      }
    }
  }
  /**** Connected mode ****/
  else
  {
    // Transmit from terminal to TCP
    if (Serial.available() && (!tcpClient.available()))
    {
      led_on();

      // In telnet in worst case we have to escape every byte
      // so leave half of the buffer always free
      int max_buf_size;
      if (telnet == true)
        max_buf_size = TX_BUF_SIZE / 2;
      else
        max_buf_size = TX_BUF_SIZE;

      // Read from serial, the amount available up to
      // maximum size of the buffer
      size_t len = std::min(Serial.available(), max_buf_size);
      Serial.readBytes(&txBuf[0], len);
      
      // Disconnect if going to AT mode with "+++" sequence
      for (int i=0; i<(int)len; i++)
      {
        if (txBuf[i] == '+') plusCount++; else plusCount = 0;
        if (plusCount >= 3)
        {
          tcpClient.stop();
          return;
        }
      }

      // Double (escape) every 0xff for telnet, shifting following bytes
      // towards the end of the buffer at that point
      if (telnet == true)
      {
        for (int i = len - 1; i >= 0; i--)
        {
          if (txBuf[i] == 0xff)
          {
            for (int j = TX_BUF_SIZE - 1; j > i; j--)
            {
              txBuf[j] = txBuf[j - 1];
            }
            len++;
          }
        }
      }

      // Write the buffer to TCP finally
      tcpClient.write(&txBuf[0], len);
      yield();
    }

    // Transmit from TCP to terminal (if TX buffer is not full)
    if (tcpClient.available() && Serial.availableForWrite()) 
    {
      led_on();
      uint8_t rxByte = tcpClient.read();

      // Is a telnet control code starting?
      if ((telnet == true) && (rxByte == 0xff))
      {
        #ifdef DEBUG
        Serial.print("<telnet>");
        #endif
        rxByte = tcpClient.read();
        if (rxByte == 0xff)
        {
          // 2 times 0xff is just an escaped real 0xff
          Serial.write(0xff);
        }
        else
        {
          // rxByte has now the first byte of the actual non-escaped control code
          #ifdef DEBUG
          Serial.print(rxByte);
          Serial.print(",");
          #endif
          uint8_t cmdByte1 = rxByte;
          rxByte = tcpClient.read();
          uint8_t cmdByte2 = rxByte;
          // rxByte has now the second byte of the actual non-escaped control code
          #ifdef DEBUG
          Serial.print(rxByte);
          #endif
          // We are asked to do some option, respond we won't
          if (cmdByte1 == DO) 
          {
            tcpClient.write((uint8_t)255); tcpClient.write((uint8_t)WONT); tcpClient.write(cmdByte2);
          }
          // Server wants to do any option, allow it
          else if (cmdByte1 == WILL) 
          {
            tcpClient.write((uint8_t)255); tcpClient.write((uint8_t)DO); tcpClient.write(cmdByte2);
          }
        }
        #ifdef DEBUG
        Serial.print("</telnet>");
        #endif
      }
      else
      {
        // Non-control codes pass through freely
        Serial.write(rxByte);
      }
    }
  }

  // Disconnect if programming mode PIN (GPIO0) is switched to GND
#ifdef USE_SWITCH
  if ((tcpClient.connected()) && (digitalRead(SWITCH_PIN) == LOW))
  {
    tcpClient.stop();
  }
#endif

  // Go to command mode if TCP disconnected and not in command mode
  if ((!tcpClient.connected()) && (cmdMode == false))
  {
    cmdMode = true;
    Serial.println("NO CARRIER");
    if (LISTEN_PORT > 0) tcpServer.begin();
  }

  if (millis() - ledTime > LED_TIME) digitalWrite(LED_PIN, HIGH);
}
