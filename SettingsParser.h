#pragma once
#include <WebServer.h>
#include <ESPmDNS.h>
#include <EEPROM.h>

class SettingsParser{
public:
  SettingsParser(int eepromAddress);

  void raiseServer(const String& serverName);
  void tickServer();
  static int getReservedSizeEEPROM();

  String getSSID();
  String getPassword();
  String getBotToken();
  String getChatId();
  String getUserId();
  String getMqttPassword();
private:
  String ssid;
  String password;
  String botToken;
  String chatId;
  String userId;
  String mqttPassword;
  int eepromAddress;

  static const int ssidSize;
  static const int passwordSize;
  static const int botTokenSize;
  static const int chatIdSize;
  static const int userIdSize;
  static const int mqttPasswordSize;

  WebServer* server;

  void loadSettingsFromEEPROM();
  void writeSettingsToEEPROM();

  void handleNewClient();
  void handleForm();

  void writeStringToEEPROM(int eepromAddress,const String& str);
  String readStringFromEEPROM(int eepromAddress);
};




