#pragma once
#include <WebServer.h>
#include <ESPmDNS.h>
#include <EEPROM.h>

class SettingsParser{
public:
  SettingsParser(int eepromAddress,bool writeOnly);

  void raiseServer(const String& serverName);
  void tickServer();
  static int getReservedSizeEEPROM();

  void updateIPConfig(IPAddress staticIP, IPAddress gateway, IPAddress subnet, IPAddress primaryDNS, IPAddress secondaryDNS);

  String getSSID();
  String getPassword();
  String getBotToken();
  String getChatId();
  String getUserId();
  String getMqttPassword();

  IPAddress getStaticIP();
  IPAddress getGateway();
  IPAddress getSubnet(); 
  IPAddress getPrimaryDNS();
  IPAddress getSecondaryDNS();
private:
  String ssid;
  String password;
  String botToken;
  String chatId;
  String userId;
  String mqttPassword;


  IPAddress staticIP;
  IPAddress gateway;
  IPAddress subnet; 
  IPAddress primaryDNS;
  IPAddress secondaryDNS;

  int eepromAddress;

  static const int ssidSize;
  static const int passwordSize;
  static const int botTokenSize;
  static const int chatIdSize;
  static const int userIdSize;
  static const int mqttPasswordSize;
  static const int IP4Size;

  WebServer* server;

  void loadSettingsFromEEPROM();
  void writeSettingsToEEPROM();

  void handleNewClient();
  void handleForm();

  void writeIPToEEPROM(int eepromAddress,IPAddress ip);
  IPAddress readIPFromEEPROM(int eepromAddress);
  void writeStringToEEPROM(int eepromAddress,const String& str);
  String readStringFromEEPROM(int eepromAddress);

  String convertIPToStr(IPAddress ip);
  bool convertStrToIP(const String& ipStr,IPAddress& ip);
};




