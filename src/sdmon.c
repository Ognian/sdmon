// (c) 2018-2022 ogi-it, Ognian Tschakalov

// #include <ctype.h>
// #include <dirent.h>
#include <fcntl.h>
// #include <libgen.h>
// #include <limits.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// this include defines MMC_BLOCK_MAJOR the magic number for the calculation of
// MMC_IOC_CMD
#include <linux/major.h>

// since this include/linux/mmc/core.h is not available we pasted it in here
/* From kernel linux/mmc/core.h */
#define MMC_RSP_PRESENT (1 << 0)
#define MMC_RSP_136 (1 << 1)    /* 136 bit response */
#define MMC_RSP_CRC (1 << 2)    /* expect valid crc */
#define MMC_RSP_BUSY (1 << 3)   /* card may send busy */
#define MMC_RSP_OPCODE (1 << 4) /* response contains opcode */

#define MMC_CMD_AC (0 << 5)
#define MMC_CMD_ADTC (1 << 5)

#define MMC_RSP_SPI_S1 (1 << 7)    /* one status byte */
#define MMC_RSP_SPI_BUSY (1 << 10) /* card may send busy */

#define MMC_RSP_SPI_R1 (MMC_RSP_SPI_S1)
#define MMC_RSP_SPI_R1B (MMC_RSP_SPI_S1 | MMC_RSP_SPI_BUSY)

#define MMC_RSP_NONE (0)
#define MMC_RSP_R1 (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R1B (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)

// these are not needed actually
// #include "mmc.h"
// #include "mmc_cmds.h"

#include <errno.h>
// this is the include defining the mmc_ioc_cmd struct and mmc_ioc_cmd_set_data
// helper etc ...
#include <linux/mmc/ioctl.h>

#include <time.h>

// CMD56 implementation
#define SD_GEN_CMD 56
#define SD_BLOCK_SIZE 512

#define formatBool(b) ((b) ? "true" : "false")

int CMD56_data_in(int fd, int cmd56_arg, unsigned char *lba_block_data) {
  int ret = 0;
  struct mmc_ioc_cmd idata;

  memset(&idata, 0, sizeof(idata));
  memset(lba_block_data, 0, sizeof(__u8) * SD_BLOCK_SIZE);

  idata.write_flag = 0;
  idata.opcode = SD_GEN_CMD;
  idata.arg = cmd56_arg;
  idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
  idata.blksz = SD_BLOCK_SIZE;
  idata.blocks = 1;
  mmc_ioc_cmd_set_data(idata, lba_block_data);
  ret = ioctl(fd, MMC_IOC_CMD, &idata);
  //  if (ret)
  //    printf("ioctl errror\n");
  return ret;
}

int CMD56_write(int fd, int cmd56_arg) {
  int ret = 0;
  struct mmc_ioc_cmd idata;
  char data_out[SD_BLOCK_SIZE];

  memset(&idata, 0, sizeof(idata));
  memset(&data_out[0], 0, sizeof(__u8) * SD_BLOCK_SIZE);

  idata.write_flag = 1;
  idata.opcode = SD_GEN_CMD;
  idata.arg = cmd56_arg;

  // data_in[0]=0x10; // also cmd56_arg macht keinen Unterschied ...

  // idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_AC | MMC_CMD_ADTC; //this works (and that matches the spec. Table 4.29 of Part1_Physical_Layer_Simplified_Specification_Ver6.00.pdf)
  idata.flags = MMC_RSP_R1 | MMC_CMD_ADTC; // this works (and that matches the spec. Table 4.29 of Part1_Physical_Layer_Simplified_Specification_Ver6.00.pdf)
  // this looks better and works also
  // idata.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

  // useless tests
  // idata.flags &= ~MMC_RSP_CRC; /* Ignore CRC */
  // idata.flags =MMC_RSP_PRESENT;
  // idata.flags = MMC_RSP_NONE | MMC_CMD_AC;

  idata.blksz = SD_BLOCK_SIZE;
  idata.blocks = 1; // set to 0 NO  -> like now send an empty block with 0 and correct block size
  mmc_ioc_cmd_set_data(idata, data_out);

  // idata.data_timeout_ns=4000000000; // 4s this is nearly max, but it has no consequence
  // idata.cmd_timeout_ms=20000;  // 20s, but it has no consequence
  // idata.cmd_timeout_ms = 10000; //set 10 sec

  ret = ioctl(fd, MMC_IOC_CMD, &idata);
  //  if (ret)
  //    printf("ioctl errror\n");

  // see idata.response[0] -[3] but it is 0
  printf("\"idata.response[]\":\"0x%02x 0x%02x 0x%02x 0x%02x\",\n", idata.response[0], idata.response[1], idata.response[2], idata.response[3]);

  return ret;
}

// helper
void dump_data_block(char *lba_block_data) {
  int count = 0;
  printf("CMD56 data block dumping:");
  while (count < SD_BLOCK_SIZE) {
    if (count % 16 == 0)
      printf("\n%03d:  ", count);
    printf("%02x ", lba_block_data[count]);
    count++;
  }
  printf("\n");
  return;
}

int bytes_to_int(unsigned char byte1, unsigned char byte2, unsigned char byte3, unsigned char byte4) { return (byte1 << 24) | (byte2 << 16) | (byte3 << 8) | byte4; }

int nword_to_int(unsigned char *data, int offset, int size) {
  if (size == 4) {
    return ((data[offset + 3] << 24) | (data[offset + 2] << 16) | (data[offset + 1] << 8) | data[offset]);
  } else if (size == 8) {
    return (((long long)data[offset + 3] << 56) | ((long long)data[offset + 3] << 48) | ((long long)data[offset + 3] << 40) | ((long long)data[offset + 3] << 32) |
            ((long long)data[offset + 3] << 24) | ((long long)data[offset + 2] << 16) | ((long long)data[offset + 1] << 8) | (long long)data[offset]);
  } else {
    return -1;
  }
}

int main(int argc, const char *argv[]) {
  int fd;
  const char *device;
  int cmd56_arg;
  unsigned char data_in[SD_BLOCK_SIZE];
  int ret;

  int i = 0;
  unsigned long sum = 0;

  time_t t = time(NULL);
  struct tm tm = *localtime(&t);

  char tmpstr[100];
  bool addTime = false;
  const char *option;

  // json output
  printf("{\n");
  printf("\"version\": \"%s\",\n", VERSION);

  printf("\"date\": \"%d-%02d-%02dT%02d:%02d:%02d.000Z\",\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  // this is more or less static...
  // "MMC_IOC_CMD": "c048b300", => 0xc048b300
  // printf("\"MMC_IOC_CMD\": \"%lx\",\n", MMC_IOC_CMD);

  if (argc < 2 || argc > 3) {
    printf("\"usage\":\"wrong argument count, sdmon <device> [-a]\"\n}\n");
    exit(1);
  }
  device = argv[1];
  printf("\"device\":\"%s\",\n", device);

  if (argc > 2) {
    option = argv[2];
    if (strcmp(option, "-a") == 0) {
      addTime = true;
    } else {
      printf("\"usage\":\"wrong option, sdmon <device> [-a]\"\n}\n");
      exit(1);
    }
  }
  printf("\"addTime\": \"%s\",\n", formatBool(addTime));

  fd = open(device, O_RDWR);
  if (fd < 0) {
    printf("\"error\":\"device open failed\"\n}\n");
    exit(1);
  }

  // first try if read with arg 1 returns anything
  cmd56_arg = 0x00000001;
  ret = CMD56_data_in(fd, cmd56_arg, data_in);
  // we assume success when the call was successful AND the signature is not 0xff 0xff
  if (ret == 0 && !((data_in[0] == 0xff && data_in[1] == 0xff) || (data_in[0] == 0x00 && data_in[1] == 0x00))) {
    printf("\"signature\":\"0x%x 0x%x\",\n", data_in[0], data_in[1]);

    if (data_in[0] == 0x70 && data_in[1] == 0x58) {
      printf("\"Longsys\":\"true\",\n");
      printf("\"SMARTVersions\": %d,\n", nword_to_int(data_in, 4, 4));
      printf("\"sizeOfDevSMART\": %d,\n", nword_to_int(data_in, 12, 4));
      printf("\"originalBadBlock\": %d,\n", nword_to_int(data_in, 16, 4));
      printf("\"increaseBadBlock\": %d,\n", nword_to_int(data_in, 20, 4));
      printf("\"writeAllSectNum\": %d Sector(512Byte),\n", nword_to_int(data_in, 24, 8));
      printf("\"replaceBlockLeft\": %d,\n", nword_to_int(data_in, 32, 4));
      printf("\"degreOfWear\": %.02f Cycle,\n", (float)nword_to_int(data_in, 36, 4) / 1000);
      printf("\"sectorTotal\": %d,\n", nword_to_int(data_in, 40, 4));
      printf("\"remainLifeTime\": %d%%,\n", nword_to_int(data_in, 44, 4));
      printf("\"remainWrGBNum\": %.02fTB,\n", (float)nword_to_int(data_in, 48, 4) / 1024);
      printf("\"lifeTimeTotal\": %.02f Cycle,\n", (float)nword_to_int(data_in, 52, 4));
      printf("\"phyWrGBNum\": %.02fTB,\n", (float)nword_to_int(data_in, 56, 4) / 1024);

      close(fd);
      printf("\"success\":true\n}\n");
      exit(0);
    }

    if (data_in[0] == 0x44 && (data_in[1] == 0x53 || data_in[1] == 0x57)) {
      printf("\"SanDisk\":\"true\",\n");
    }

    /*
          Health Status is an estimated percent life used based on the amount of TBW the NAND
          memory has experienced relative to the SD card device TBW ability. Values reported in
          hexadecimal in 1% increments with 0x01 representing 0.0% to 0.99% used. A value of
          0x64 indicates 99 to 99.99% of the ability have been used. The SD card storage device
          may accommodate writes in excess of the 100% expected life limit. Note that although
          this is possible, entry into a read only mode could occur upon the next write cycle.
    */

    strncpy(tmpstr, (char *)&data_in[2], 6);
    tmpstr[6] = 0;
    printf("\"manufactureYYMMDD\": \"%s\",\n", tmpstr);
    printf("\"healthStatusPercentUsed\": %d,\n", data_in[8]);
    printf("\"featureRevision\": \"0x%x\",\n", data_in[11]);
    printf("\"generationIdentifier\": %d,\n", data_in[14]);
    strncpy(tmpstr, (char *)&data_in[49], 32);
    tmpstr[32] = 0;
    printf("\"productString\": \"%s\",\n", tmpstr);
    printf("\"powerOnTimes\": %d,\n", bytes_to_int(0, data_in[24], data_in[25], data_in[26]));
    close(fd);
    printf("\"success\":true\n}\n");
    exit(0);
  }

  // try micron argument
  cmd56_arg = 0x110005fb;
  ret = CMD56_data_in(fd, cmd56_arg, data_in);
  // we assume success when the call was successful AND the signature is not 0xff 0xff
  if (ret == 0 && !((data_in[0] == 0xff && data_in[1] == 0xff) || (data_in[0] == 0x00 && data_in[1] == 0x00))) {
    printf("\" signature \":\"0x%x 0x%x\",\n", data_in[0], data_in[1]);
    if (data_in[0] == 0x4d && data_in[1] == 0x45) {
      printf("\"Micron\":\"true\",\n");
      printf("\"Percentange step utilization\": %d,\n", (int)(data_in[7]));
      printf("\"TLC area utilization\": %d,\n", (int)(data_in[8]));
      printf("\"SLC area utilization\": %d,\n", (int)(data_in[9]));
      close(fd);
      printf("\"success\":true\n}\n");
      exit(0);
    }
  }

  // try swissbit argument
  cmd56_arg = 0x53420001;
  ret = CMD56_data_in(fd, cmd56_arg, data_in);
  // we assume success when the call was successful AND the signature is not 0xff 0xff
  if (ret == 0 && !((data_in[0] == 0xff && data_in[1] == 0xff) || (data_in[0] == 0x00 && data_in[1] == 0x00))) {
    printf("\"signature\":\"0x%x 0x%x\",\n", data_in[0], data_in[1]);

    if (data_in[0] == 0x53 && data_in[1] == 0x77) {
      printf("\"Swissbit\":\"true\",\n");
      printf("\"fwVersion\": [%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c,%c],\n", data_in[32], data_in[33], data_in[34], data_in[35], data_in[36], data_in[37], data_in[38], data_in[39], data_in[40],
             data_in[41], data_in[42], data_in[43], data_in[44], data_in[45], data_in[46], data_in[47]); // show char
      printf("\"User area rated cycles\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[48], data_in[49], data_in[50], data_in[51]);
      printf("\"User area max cycle cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[52], data_in[53], data_in[54], data_in[55]);
      printf("\"User area total cycle cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[56], data_in[57], data_in[58], data_in[59]);
      printf("\"User area average cycle cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[60], data_in[61], data_in[62], data_in[63]);
      printf("\"System area max cycle cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[68], data_in[69], data_in[70], data_in[71]);
      printf("\"System area total cycle cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[72], data_in[73], data_in[74], data_in[75]);
      printf("\"System area average cycle cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[76], data_in[77], data_in[78], data_in[79]);
      printf("\"Remaining Lifetime Percent\": %d %,\n", (int)(data_in[80]));
      switch (data_in[86]) {
      case 0x00:
        printf("\"Speed mode\": Default speed\n");
        break;
      case 0x01:
        printf("\"Speed mode\": High speed\n");
        break;
      case 0x10:
        printf("\"Speed mode\": SDR12 speed\n");
        break;
      case 0x11:
        printf("\"Speed mode\": SDR25 speed\n");
        break;
      case 0x12:
        printf("\"Speed mode\": SDR50 speed\n");
        break;
      case 0x14:
        printf("\"Speed mode\": DDR50 speed\n");
        break;
      case 0x18:
        printf("\"Speed mode\": SDR104 speed\n");
        break;
      }
      switch (data_in[87]) {
      case 0x00:
        printf("\"Bus width\": 1 bit\n");
        break;
      case 0x10:
        printf("\"Bus width\": 4 bits\n");
        break;
      }
      printf("\"User area spare blocks cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[88], data_in[89], data_in[90], data_in[91]);
      printf("\"System area spare blocks cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[92], data_in[93], data_in[94], data_in[95]);
      printf("\"User area runtime bad blocks cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[96], data_in[97], data_in[98], data_in[99]);
      printf("\"System area runtime bad blocks cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[100], data_in[101], data_in[102], data_in[103]);
      printf("\"User area refresh cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[104], data_in[105], data_in[106], data_in[107]);
      printf("\"System area refresh cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[108], data_in[109], data_in[110], data_in[111]);
      printf("\"Interface crc cnt\": [\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n", data_in[112], data_in[113], data_in[114], data_in[115]);
      printf("\"Power cycle cnt\": [\"%d\",\"%d\",\"%d\",\"%d\"],\n", data_in[116], data_in[117], data_in[118], data_in[119]);
      close(fd);
      printf("\"success\":true\n}\n");
      exit(0);
    }
  }

  if (ret == 0) {
    printf("\"read_via_cmd56_arg_1\":\"read successful but signature 0x%x 0x%x\",\n", data_in[0], data_in[1]);
  } else {
    printf("\"read_via_cmd56_arg_1\":\"not implemented: %s\",\n", strerror(errno));
  }

  // prepare for health command
  cmd56_arg = 0x00000010; // all other are 0
  ret = CMD56_write(fd, cmd56_arg);
  if (ret) {
    printf("\"error1\":\"1st CMD56 CALL FAILED: %s\",\n", strerror(errno));
    // printf("\"error\":\"1st CMD56 CALL FAILED: %s\"\n}\n", strerror(errno));
    // exit(1);
  }

  if (addTime) {
    usleep(1000000);
  }

  // do query smart data
  cmd56_arg = 0x00000021;
  ret = CMD56_data_in(fd, cmd56_arg, data_in);
  if (ret) {
    printf("\"error2\":\"2nd CMD56 CALL FAILED: %s\"\n}\n", strerror(errno));
    exit(1);
  }

  // unfortunately this is not json but good for debugging
  //  printf("\"rawData\":\"");
  //  dump_data_block(data_in);
  //  printf("\",\n");

  printf("\"flashId\": "
         "[\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
         "\"0x%02x\",\"0x%02x\",\"0x%02x\"],\n",
         data_in[0], data_in[1], data_in[2], data_in[3], data_in[4], data_in[5], data_in[6], data_in[7], data_in[8]);
  printf("\"icVersion\": [\"0x%02x\",\"0x%02x\"],\n", data_in[9], data_in[10]);
  printf("\"fwVersion\": [%02d,%02d],\n", data_in[11],
         data_in[12]); // show in decimal
  printf("\"ceNumber\": \"0x%02x\",\n", data_in[14]);

  // printf("\"badBlockReplaceMaximum\": [\"0x%02x\",\"0x%02x\"],\n", data_in[16], data_in[17]);
  // badBlockReplaceMaximum is spareBlockCount
  printf("\"spareBlockCount\": %d,\n", (int)((data_in[16] << 8) + data_in[17]));

  //  printf("\"badBlockCountPerDie1\": "
  //         "[\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\"],\n",
  //         data_in[32], data_in[33], data_in[34], data_in[35], data_in[36],
  //         data_in[37], data_in[38], data_in[39], data_in[40], data_in[41],
  //         data_in[42], data_in[43], data_in[44], data_in[45], data_in[46],
  //         data_in[47], data_in[48], data_in[49], data_in[50], data_in[51],
  //         data_in[52], data_in[53], data_in[54], data_in[55], data_in[56],
  //         data_in[57], data_in[58], data_in[59], data_in[60], data_in[61],
  //         data_in[62], data_in[63]);
  // sum up to get initial bad block count
  sum = 0;
  for (i = 32; i < 64; i++)
    sum += data_in[i];
  printf("\"initialBadBlockCount\": %ld,\n", sum);

  // printf("\"goodBlockRatePercentBytes\": [\"0x%02x\",\"0x%02x\"],\n", data_in[64], data_in[65]);
  // printf("\"goodBlockRatePercent\": %d,\n", (int)((data_in[64]<<8)+data_in[65]));
  printf("\"goodBlockRatePercent\": %2.2f,\n", (float)((float)((int)((data_in[64] << 8) + data_in[65])) / 100));

  printf("\"totalEraseCount\": %ld,\n", (long)((data_in[80] << 24) + (data_in[81] << 16) + (data_in[82] << 8) + data_in[83]));

  // printf("\"enduranceRemainLifePercentBytes\": [\"0x%02x\",\"0x%02x\"],\n", data_in[96], data_in[97]);
  // printf("\"enduranceRemainLifePercent\": %d,\n", (int)((data_in[96]<<8)+data_in[97]));
  printf("\"enduranceRemainLifePercent\": %2.2f,\n", (float)((float)((int)((data_in[96] << 8) + data_in[97])) / 100));

  printf("\"avgEraseCount\": %ld,\n", (long)((data_in[104] << 24) + (data_in[105] << 16) + (data_in[98] << 8) + data_in[99]));
  printf("\"minEraseCount\": %ld,\n", (long)((data_in[106] << 24) + (data_in[107] << 16) + (data_in[100] << 8) + data_in[101]));
  printf("\"maxEraseCount\": %ld,\n", (long)((data_in[108] << 24) + (data_in[109] << 16) + (data_in[102] << 8) + data_in[103]));

  printf("\"powerUpCount\": %ld,\n", (long)((data_in[112] << 24) + (data_in[113] << 16) + (data_in[114] << 8) + data_in[115]));
  printf("\"abnormalPowerOffCount\": %d,\n", (int)((data_in[128] << 8) + data_in[129]));
  printf("\"totalRefreshCount\": %d,\n", (int)((data_in[160] << 8) + data_in[161]));
  printf("\"productMarker\": "
         "[\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
         "\"0x%02x\",\"0x%02x\"],\n",
         data_in[176], data_in[177], data_in[178], data_in[179], data_in[180], data_in[181], data_in[182], data_in[183]);
  //  printf("\"badBlockCountPerDie2\": "
  //         "[\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\",\"0x%02x\","
  //         "\"0x%02x\",\"0x%02x\"],\n",
  //         data_in[184], data_in[185], data_in[186], data_in[187], data_in[188],
  //         data_in[189], data_in[190], data_in[191], data_in[192], data_in[193],
  //         data_in[194], data_in[195], data_in[196], data_in[197], data_in[198],
  //         data_in[199], data_in[200], data_in[201], data_in[202], data_in[203],
  //         data_in[204], data_in[205], data_in[206], data_in[207], data_in[208],
  //         data_in[209], data_in[210], data_in[211], data_in[212], data_in[213],
  //         data_in[214], data_in[215]);
  // sum up to get later bad block count
  sum = 0;
  for (i = 184; i < 216; i++)
    sum += data_in[i];
  printf("\"laterBadBlockCount\": %ld,\n", sum);

  close(fd);
  printf("\"success\":true\n}\n");

  exit(0);
}
