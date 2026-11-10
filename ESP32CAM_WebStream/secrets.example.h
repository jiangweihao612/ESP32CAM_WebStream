/*
 *  secrets.example.h —— 凭据模板
 *  ------------------------------------------------------------------
 *  用法: 把本文件复制一份改名成 secrets.h, 再填你自己的 WiFi 信息。
 *        secrets.h 已被 .gitignore 排除, 不会误提交。
 *
 *  Windows:  copy secrets.example.h secrets.h
 *  macOS/Linux:  cp secrets.example.h secrets.h
 *  ------------------------------------------------------------------
 */
#pragma once

static const char *STA_SSID = "YOUR_WIFI_SSID";       // 要连接的路由器名称
static const char *STA_PASS = "YOUR_WIFI_PASSWORD";   // WiFi 密码

static const char *AP_SSID  = "ESP32-CAM";            // 板子自己发出的热点
static const char *AP_PASS  = "12345678";             // 热点密码, 至少 8 位
