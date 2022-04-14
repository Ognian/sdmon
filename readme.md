# sdmon

This program reads out the health data of *some* industrial grade SD Cards. Unfortunately there is no standard way of doing this.
Sdmon uses CMD56 of the SD card specification and currently does this for:
- Apacer Industrial SD Cards, and some others from branded distributors
- SanDisk Industrial SD Cards (contributed by William Croft) - work in progress

The output is JSON so that it can be parsed easier in applications using sdmon.  

## Installation
### Released Version
For available versions see [releases](../../releases). Choose the correct archive for your os, i.e.  sdmon-arm64.tar.gz for raspberry pi os 64bit or sdmon-armv7.tar.gz raspberry pi os 32bit.
Adjust the path to the desired version by replacing `v0.2.0` in the below example with the desired one.
```
curl -L https://github.com/Ognian/sdmon/releases/download/v0.2.0/sdmon-arm64.tar.gz | tar zxf - 
```
### Development Release
The `latest` development [release](../../releases) may be also available.
```
curl -L https://github.com/Ognian/sdmon/releases/download/latest/sdmon-arm64.tar.gz | tar zxf - 
```
## Usage

on a raspberry pi:
```
sudo ./sdmon /dev/mmcblk0
```

## Example Output
This is the output when the card doesn't support health status:
```
sudo ./sdmon /dev/mmcblk0
{
"version": "v.0.01-dirty (314f9dc)",
"date": "2022-04-07T16:18:24.000Z",
"device":"/dev/mmcblk0",
"idata.response[]":"0x900 0x00 0x00 0x00",
"error1":"1st CMD56 CALL FAILED: Operation timed out",
"error2":"2nd CMD56 CALL FAILED: Operation timed out"
}

```
Apacer Industrial micro SD card (97.56% healthy):
```
{
"version": "v0.3.2-dirty (262e866)",
"date": "2022-04-14T11:23:48.000Z",
"device":"/dev/mmcblk0",
"read_via_cmd56_arg_1":"read successful but signature 0xff 0xff"
"idata.response[]":"0x900 0x00 0x00 0x00",
"flashId": ["0x98","0x3c","0x98","0xb3","0x76","0x72","0x08","0x0e","0x00"],
"icVersion": ["0x1f","0xc3"],
"fwVersion": [83,201],
"ceNumber": "0x01",
"spareBlockCount": 46,
"initialBadBlockCount": 72,
"goodBlockRatePercent": 97.56,
"totalEraseCount": 1380,
"enduranceRemainLifePercent": 99.97,
"avgEraseCount": 1,
"minEraseCount": 1,
"maxEraseCount": 5,
"powerUpCount": 55,
"abnormalPowerOffCount": 0,
"totalRefreshCount": 0,
"productMarker": ["0x00","0x00","0x00","0x00","0x00","0x00","0x00","0x00"],
"laterBadBlockCount": 0,
"success":true
}

```

(c) 2018 - today, OGI-IT, Ognian Tschakalov and contributors, released under GNU GPL v2