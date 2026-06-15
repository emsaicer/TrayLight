# <img src="TrayLight.ico" alt="TrayLight Icon" width="42" height="42" align="top"> TrayLight

`TrayLight` is a simple, lightweight, portable utility for Windows that gives you the ability to **toggle the system theme in one click** using a **tray icon**. You can make a simple configuration in the menu right in the tray.

<img width="473" height="251" alt="TrayLight_Menu" src="https://github.com/user-attachments/assets/98e42c3c-b85a-4a29-8a1f-47382b39cbd5" />

## How to Use

1. Run `TrayLight.exe` (the icon in the tray will appear).
2. **Left-click** on the icon to **toggle the theme**.
3. **Right-click** to **change settings**.

## Features

- Toggle light and dark mode in **one click**.
- Easily **add the program to startup**.
- Choose between toggling the theme for **apps only** or **for apps and the system**.

## How to Build

```
windres resources.rc -o resources.o && clang main.c resources.o -o TrayLight.exe "-Wl,--gc-sections" -lole32 -luuid -mwindows -s -Os && del resources.o
```