#ifndef CONFIG_H
#define CONFIG_H

#include <string.h>
#include <Arduino.h>

struct WifiConfig {
  String ssid;               // wifi ssid
  String wifipassword;       // wifi password
};

struct HttpConfig {
  String httpuser;           // username to access web admin
  String httppassword;       // password to access web admin
  int webserverporthttp;     // http port number for web admin
};



#endif // CONFIG_H
