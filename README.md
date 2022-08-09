## Overview
This is a project to repeat BLE pairing crash when WiFi is connected (esp-idf v4.1.2 + BLE pairing patch, issue#8928). 
The demo code is based on [(gatt_security_server)](https://github.com/espressif/esp-idf/tree/v4.1.2/examples/bluetooth/bluedroid/ble/gatt_security_server)

device: ESP32_DevKitc_V4 (ESP32_WROVER-E)  
esp-idf: v4.1.2  
patch applied: [(refer to issue#8928)](https://github.com/espressif/esp-idf/issues/8928#issuecomment-1180421136)  
sdkconfig (major configs to repeat the crash)
```
   I (1773) TEST:     CONFIG_SPIRAM_USE_MALLOC                           1
   I (1783) TEST:     CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL                1
   I (1793) TEST:     CONFIG_SPIRAM_MALLOC_RESERVE_INTERNAL              32768
   I (1803) TEST:     CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY        1
   I (1813) TEST:     CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST             1
   I (1823) TEST:     CONFIG_BT_BLE_DYNAMIC_ENV_MEMORY                   1
```
## Steps to repeat the crash
1. Refer to sdkconfig the key configs that are required for the demo  
NOTE: small CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL is the key to repeat the crash
2. Edit wifi.cfg and enter Wi-Fi ssid and password. This is a single line text file in format <ssid>,<pwd> (e.g. `test-ssid,1234567890`)  
NOTE: a newline should exist at the end of the file. This is the Wi-Fi config to connect to AP.
3. When the ROM starts, it connects to the AP according to ssid/pwd in wifi.cfg.  
If Wi-Fi is able to connect to AP, log is seen.  
`I (2843) TEST: (STA) IP address: 192.168.43.29`  
If Wi-Fi fails to connect, disconnected log is seen.  
`E (2103) TEST: Failed to connect to AP`
5. Use Android nRFConnect App to scan/connect to the ROM.  When BLE is pairing, nRFConnect prompts to confirm.  After confirmation, the ROM is very easy to crash if **Wi-Fi is connected**.  
```
I (2855) BTDM_INIT: BT controller compile version [2fcbe89]
I (2865) system_api: Base MAC address is not set, read default base MAC address from BLK0 of EFUSE
I (2875) phy_init: phy_version 4660,0162888,Dec 23 2020
I (3135) SEC_GATTS_DEMO: app_main init bluetooth
I (3315) SEC_GATTS_DEMO: The number handle = 8
I (3345) SEC_GATTS_DEMO: advertising start success
I (15885) SEC_GATTS_DEMO: ESP_GATTS_CONNECT_EVT
W (18255) BT_SMP: FOR LE SC LTK IS USED INSTEAD OF STK
I (18485) SEC_GATTS_DEMO: key type = ESP_LE_KEY_LENC
I (18495) SEC_GATTS_DEMO: key type = ESP_LE_KEY_PENC
I (18495) SEC_GATTS_DEMO: key type = ESP_LE_KEY_LID
I (18675) SEC_GATTS_DEMO: key type = ESP_LE_KEY_PID
Guru Meditation Error: Core  0 panic'ed (Cache disabled but cached memory region accessed)
Core 0 register dump:
PC      : 0x40090db3  PS      : 0x00060634  A0      : 0x800840cd  A1      : 0x3ffbe330
A2      : 0xbad00bad  A3      : 0x00000000  A4      : 0x0000a604  A5      : 0x3ffbf608
A6      : 0x00000000  A7      : 0x00000000  A8      : 0xbad00bad  A9      : 0x00000001
A10     : 0x00000015  A11     : 0x00000000  A12     : 0x0000a604  A13     : 0x3ffbf608
A14     : 0x00000000  A15     : 0x3ffb8360  SAR     : 0x0000000b  EXCCAUSE: 0x00000007
EXCVADDR: 0x00000000  LBEG    : 0x40091b18  LEND    : 0x40091b34  LCOUNT  : 0x00000000
Core 0 was running in ISR context:
EPC1    : 0x40083f94  EPC2    : 0x00000000  EPC3    : 0x00000000  EPC4    : 0x40090db3

ELF file SHA256: fecfe94e0f32e9dc

Backtrace: 0x40090db0:0x3ffbe330 0x400840ca:0x3ffbe350 0x40086b43:0x3ffbe370 0x400844c1:0x3ffbe390 0x4008e6af:0x3ffbe3b0 0x4008dd37:0x3ffbe3d0 0x4008df55:0x3ffbe3f0 0x4008db42:0x3ffbe430 0x40086c09:0x3ffbe450 0x4008a7ad:0x3ffbe470 0x400875d6:0x3ffbe4b0 0x4008cd6e:0x3ffbe4d0 0x4008d887:0x3ffbe4f0 0x40084e2d:0x3ffbe510 0x4000c053:0x3ffd93b0 0x40008544:0x3ffd93c0 0x40085e49:0x3ffd93e0 0x4009a33e:0x3ffd9400 0x4009a1a1:0x3ffd9440 0x40085aa1:0x3ffd9460 0x40085dd8:0x3ffd94a0 0x40085666:0x3ffd94c0 0x400deead:0x3ffd94e0 0x400df05e:0x3ffd9500 0x400dca56:0x3ffd9520 0x400dd0a5:0x3ffd95a0 0x400dd5d1:0x3ffd9610 0x400dc30e:0x3ffd9630 0x40129a11:0x3ffd9660 0x40102a43:0x3ffd96c0 0x40101bd8:0x3ffd96e0 0x40101c85:0x3ffd9710 0x4010227d:0x3ffd9750 0x4012cfb1:0x3ffd9770 0x4012d093:0x3ffd97a0 0x4012d396:0x3ffd97d0 0x401284eb:0x3ffd9870 0x4012a915:0x3ffd9890 0x40093b59:0x3ffd98b0
```
