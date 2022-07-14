# sdmon

This program reads out the health data of *some* industrial grade SD Cards. Unfortunately there is no standard way of doing this.
Sdmon uses CMD56 of the SD card specification and currently does this for:
- [Apacer Industrial SD Cards](https://industrial.apacer.com/en-ww/SSD-Industrial-Card/microSD), and some others from branded distributors
- [Kingston Industrial SD Cards](https://www.kingston.com/en/memory-cards/industrial-grade-microsd-uhs-i-u3) (SDCIT/32GB and the SDCIT2/32GB where reported to work with the -a option, see Issue #6)
- [SanDisk Industrial SD Cards](https://documents.westerndigital.com/content/dam/doc-library/en_us/assets/public/western-digital/product/embedded-flash/product-brief/product-brief-western-digital-industrial-sd-microsd.pdf) (code contributed by William Croft)

Although some of the above cards have been tested, there is no guarantee that a particular card of the above manufacturers will work. 

The output of the program is JSON so that it can be parsed easier in applications using sdmon.  

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
The executable is statically linked, therefor it should work without any dependencies.

On a raspberry pi:
```
sudo ./sdmon /dev/mmcblk0
```

The -a option was introduced to allow extra time between the 2 CMD56 commands (1 second).
Try first without it and if not successful try using the -a option like:
```
sudo ./sdmon /dev/mmcblk0 -a
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
Apacer Industrial micro SD card (99.97% healthy):
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
Sandisk Industrial SD card (99% healthy)
```
sudo ./sdmon /dev/mmcblk0
{
"version": "v0.3.2-3 (a04b618) arm64",
"date": "2022-04-14T08:27:15.000Z",
"device":"/dev/mmcblk0",
"signature":"0x44 0x53",
"SanDisk":"true",
"manufactureYYMMDD": "211122",
"healthStatusPercentUsed": 1,
"featureRevision": "0x1f",
"generationIdentifier": 5,
"productString": "SanDisk                         ",
"success":true
}
```

(c) 2018 - today, **OGI-IT**, Ognian Tschakalov and contributors, released under GNU GPL v2