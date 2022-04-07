# sdmon

This program reads out the health data of *some* industrial grade SD Cards. Unfortunately there is no standard way of doing this.
Sdmon uses CMD56 of the SD card specification and currently does this for:
- Apacer Industrial SD Cards, and some others from branded distributors
- SanDisk Industrial SD Cards (contributed by William Croft) - work in progress

The output is JSON so that it can be parsed easier in applications using sdmon.  

## Installation

###Development Release
```
curl -L https://github.com/Ognian/sdmon/releases/download/latest/sdmon-arm64.tar.gz | tar zxf - 
```
## Usage

on a raspberry pi:
```
sudo ./sdmon /dev/mmcblk0
```

## Example Output

```
sudo ./sdmon /dev/mmcblk0
{
"version": "v.0.01-dirty (314f9dc)",
"date": "2022-04-07T16:18:24.000Z",
"device":"/dev/mmcblk0",
"idata.response[]":"0x900 0x00 0x00 0x00",
"error":"2nd CMD56 CALL FAILED: Operation timed out"
}

```

```
```

(c) 2018 - today, OGI-IT, Ognian Tschakalov and contributors released under GNU GPL v2