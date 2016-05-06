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

// Global variables
String cmd = "";
WiFiClient tcpClient;
bool cmdMode = true;
#define SWITCH_PIN 0     // GPIO0 (programmind mode pin)
#define DEFAULT_BPS 2400 // 2400 safe for all old computers including C64
//#define USE_SWITCH 1

/**
 * Arduino main init function
 */
void setup()
{
  Serial.begin(DEFAULT_BPS);

#ifdef USE_SWITCH
  pinMode(SWITCH_PIN, INPUT);
  digitalWrite(SWITCH_PIN, HIGH);
#endif

  Serial.println("Virtual modem");
  Serial.println("=============");
  Serial.println();
  Serial.println("To connect to WIFI: ATWIFI<ssid>,<key>");
  Serial.println("To change baud rate: AT<baud>");
  Serial.println("To connect by TCP: ATDT<host>:<port>");
  Serial.println();
  Serial.println("OK");
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

  /**** Just AT ****/
  if (upCmd == "AT") Serial.println("OK");
  
  /**** Dial to host ****/
  else if (upCmd.indexOf("ATDT") == 0)
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
    if (tcpClient.connect(hostChr, portInt))
    {
      Serial.println("CONNECT");
      cmdMode = false;
      Serial.flush();
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
  else if (upCmd == "AT300") Serial.begin(300);
  else if (upCmd == "AT1200") Serial.begin(1200);
  else if (upCmd == "AT2400") Serial.begin(2400);
  else if (upCmd == "AT9600") Serial.begin(9600);
  else if (upCmd == "AT19200") Serial.begin(19200);
  else if (upCmd == "AT38400") Serial.begin(38400);
  else if (upCmd == "AT57600") Serial.begin(57600);
  else if (upCmd == "AT115200") Serial.begin(115200);

  /**** Unknown command ****/
  else Serial.println("ERROR");

  cmd = "";
}

/**
 * Arduino main loop function
 */     
void loop()
{  
  if (cmdMode == true)
  {
    if (Serial.available())
    {
      char chr = Serial.read();
      if ((chr == '\n') || (chr == '\r')) command();
      else 
      {
        cmd.concat(chr);
        Serial.print(chr);
      }
    }
  }
  else
  {
    if (Serial.available()) tcpClient.write(Serial.read());
    if (tcpClient.available() && Serial.availableForWrite()) Serial.write(tcpClient.read());
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
  }
}
