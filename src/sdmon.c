// (c) 2018-2022 ogi-it, Ognian Tschakalov
// vim: set et ts=2 sw=2 sts=2 ft=c:

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
#include <stdarg.h>
#include <inttypes.h>

#include "json-builder.h"

// this include defines MMC_BLOCK_MAJOR the magic number for the calculation of
// MMC_IOC_CMD
#if defined(__linux__)
#include <linux/major.h>
#endif

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
#if defined(__linux__)
#include <linux/mmc/ioctl.h>
#elif defined(__FreeBSD__)
#include <dev/mmc/mmc_ioctl.h>
#endif

#include <time.h>

// default verbosity is 1
int opt_verbosity = 1;
#define verbose0(...) if (opt_verbosity >= 0) fprintf(stderr, __VA_ARGS__)
#define verbose1(...) if (opt_verbosity >= 1) fprintf(stderr, __VA_ARGS__)
#define verbose2(...) if (opt_verbosity >= 2) fprintf(stderr, __VA_ARGS__)
#define verbose3(...) if (opt_verbosity >= 3) fprintf(stderr, __VA_ARGS__)

const char *allowed_methods = "auto, sandisk, adata, transcend, micron, swissbit, 2step, innodisk";
#define METHOD_AUTO 0
#define METHOD_SANDISK 1
#define METHOD_ADATA 2
#define METHOD_TRANSCEND 3
#define METHOD_MICRON 4
#define METHOD_SWISSBIT 5
#define METHOD_2STEP 6
#define METHOD_INNODISK 7

// CMD56 implementation
#define SD_GEN_CMD 56
#define SD_BLOCK_SIZE 512

// helper
void dump_data_block(unsigned char *lba_block_data) {
  int count = 0;
  while (count < SD_BLOCK_SIZE) {
    if (count % 16 == 0)
      verbose3("\n%03d:  ", count);
    verbose3("%02x ", lba_block_data[count]);
    count++;
  }
  verbose3("\n");
  return;
}

int CMD56_data_in(int fd, int cmd56_arg, unsigned char *lba_block_data, const char *mock_file, const char *output_file) {
  int ret = 0;
  struct mmc_ioc_cmd idata;

  if (mock_file) {
    FILE *mock = fopen(mock_file, "rb");
    if (!mock) {
      verbose0("Failed to open mocking test file '%s': %s\n", mock_file, strerror(errno));
      return -EBADF;
    }
    if (fread(lba_block_data, 1, 512, mock) != 512) {
      fclose(mock);
      verbose0("Less than 512 bytes could be read from file '%s'\n", mock_file);
      return -EBADF;
    }
    fclose(mock);
    verbose3("CMD56(mocked) output with arg1=0x%x:\n", cmd56_arg);
    dump_data_block(lba_block_data);
    return 0;
  }

  memset(&idata, 0, sizeof(idata));
#if defined(__linux__)
  memset(lba_block_data, 0, sizeof(__u8) * SD_BLOCK_SIZE);
#elif defined(__FreeBSD__)
  memset(lba_block_data, 0, sizeof(SD_BLOCK_SIZE));
#endif

  idata.write_flag = 0;
  idata.opcode = SD_GEN_CMD;
  idata.arg = cmd56_arg;
  idata.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;
  idata.blksz = SD_BLOCK_SIZE;
  idata.blocks = 1;
  mmc_ioc_cmd_set_data(idata, lba_block_data);
  ret = ioctl(fd, MMC_IOC_CMD, &idata);
  if (ret) {
    verbose2("ioctl error: %s\n", strerror(errno));
  }
  else {
    verbose3("CMD56 output with arg1=0x%x:\n", cmd56_arg);
    dump_data_block(lba_block_data);
    if (output_file) {
      FILE *outfh = fopen(output_file, "wb");
      if (!outfh) {
        verbose0("Failed to open output file '%s' for write: %s\n", output_file, strerror(errno));
        // continue anyway
      }
      else {
        if (fwrite(lba_block_data, 1, 512, outfh) != 512) {
          verbose0("Less than 512 bytes could be written to file '%s'\n", output_file);
          // continue anyway
        }
        fclose(outfh);
      }
    }
  }
  return ret;
}

// returns false if the whole block is only 0x00 or 0xFF
bool is_data_valid(unsigned char *data) {
  bool only00 = true;
  bool onlyff = true;
  for (int i = 0; i < SD_BLOCK_SIZE && (only00 || onlyff); i++) {
    if (data[i] != 0x00)
      only00 = false;
    else if (data[i] != 0xff)
      onlyff = false;
  }
  return (!only00 && !onlyff);
}

int CMD56_write(int fd, int cmd56_arg) {
  int ret = 0;
  struct mmc_ioc_cmd idata;
  char data_out[SD_BLOCK_SIZE];

  memset(&idata, 0, sizeof(idata));
#if defined(__linux__)
  memset(&data_out[0], 0, sizeof(__u8) * SD_BLOCK_SIZE);
#elif defined(__FreeBSD__)
  memset(&data_out[0], 0, sizeof(SD_BLOCK_SIZE));
#endif

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
  if (ret)
    verbose2("ioctl error: %s\n", strerror(errno));

  // see idata.response[0] -[3] but it is 0
  verbose2("idata.response[]: 0x%02x 0x%02x 0x%02x 0x%02x\n", idata.response[0], idata.response[1], idata.response[2], idata.response[3]);

  return ret;
}

// convert Little Endian words to int
json_int_t nwordle_to_int(unsigned char *data, int offset, int size) {
  uint64_t v = 0;
  for (int i = 0; i < size; i++) {
    v |= (uint64_t)data[offset + i] << (i * 8);
  }
  return v;
}

// convert Big Endian words to int
json_int_t nwordbe_to_int(unsigned char *data, int offset, int size) {
  uint64_t v = 0;
  for (int i = 0; i < size; i++) {
    v |= (uint64_t)data[offset + i] << ((size - i - 1) * 8);
  }
  return v;
}

// take a json_value, print it and free it
void json_print_and_free(json_value *j) {
  json_serialize_opts jso;
  memset(&jso, 0, sizeof(jso));
  jso.indent_size = 2;
  jso.mode = json_serialize_mode_multiline;
  // jso.opts = json_serialize_opt_no_space_after_comma | json_serialize_opt_no_space_after_colon;

  char *printbuf = malloc(json_measure_ex(j, jso));
  json_serialize_ex(printbuf, j, jso);
  puts(printbuf);
  free(printbuf);
  json_builder_free(j);
}

// helper to build a formatted string and its json_value
// we first declare the func to tell the compiler that it
// should check the params as for printf
json_value *json_sprintf_new(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));

// then we define the func, it returns the built json_value*
json_value *json_sprintf_new(const char *fmt, ...) {
  va_list args;
  char dststr[128];
  va_start(args, fmt);
  vsnprintf(dststr, 128, fmt, args);
  va_end(args);
  return json_string_new(dststr);
}

// helper to build a json array of formatted strings
// representing an array of values, return the
// corresponding json_value* we've built
json_value *json_array_build(const char *fmt, const unsigned char *data, size_t offset, size_t size) {
  json_value *arr = json_array_new(size);
  for (size_t i = 0; i < size; i++)
    json_array_push(arr, json_sprintf_new(fmt, data[offset + i]));
  return arr;
}

void print_usage(void) {
  verbose0(
"sdmon v%s - SD Card Health Monitoring\n"
"\n"
"  Usage: sdmon [options] <device>\n"
"  Options:\n"
"    -v         Increase message verbosity, up to -vv\n"
"    -q         Decrease message verbosity, down to -qq\n"
"    -m METHOD  Specify which query method should be used, one of:\n"
"                 %s.\n"
"                 By default, the 'auto' method is used\n"
"    -a         For 2step method, force a delay between the 2 calls,\n"
"                 this can help for some SD cards\n"
"    -i FILE    Mock cmd56 call and consider FILE as the response,\n"
"                 requires -m. For debug only.\n"
"    -o FILE    Dump raw cmd56 output to FILE.\n"
"\n", VERSION, allowed_methods);
}

int main(int argc, char* const* argv) {
  int fd;
  const char *device;
  int cmd56_arg;
  unsigned char data_in[SD_BLOCK_SIZE];
  bool cmd56_arg1_tried = false;
  int ret;

  int i = 0;
  unsigned long sum = 0;

  time_t t = time(NULL);
  struct tm tm = *localtime(&t);

  char tmpstr[100];

  int opt;
  bool opt_force = false;
  bool opt_addTime = false;
  int opt_method = METHOD_AUTO;
  char *opt_method_name = strdup("auto"); // needs to be free()-able
  char *opt_input_file = NULL;
  char *opt_output_file = NULL;

  // this is more or less static...
  // "MMC_IOC_CMD": "c048b300", => 0xc048b300
  // printf("\"MMC_IOC_CMD\": \"%lx\",\n", MMC_IOC_CMD);

  json_value *j = json_object_new(0);
  json_object_push(j, "version", json_string_new(VERSION));
  json_object_push(j, "date", json_sprintf_new("%d-%02d-%02dT%02d:%02d:%02d.000", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec));

  while ((opt = getopt(argc, argv, "aqvfm:i:o:h")) != -1) {
    switch (opt) {
      case 'a':
        opt_addTime = true;
        break;
      case 'q':
        opt_verbosity--;
        break;
      case 'v':
        opt_verbosity++;
        break;
      case 'f':
        opt_force = true;
        break;
      case 'm':
        verbose3("want method %s\n", optarg);
        if (strcmp(optarg, "auto") == 0)
          opt_method = METHOD_AUTO;
        else if (strcmp(optarg, "sandisk") == 0)
          opt_method = METHOD_SANDISK;
        else if (strcmp(optarg, "adata") == 0)
          opt_method = METHOD_ADATA;
        else if (strcmp(optarg, "transcend") == 0)
          opt_method = METHOD_TRANSCEND;
        else if (strcmp(optarg, "micron") == 0)
          opt_method = METHOD_MICRON;
        else if (strcmp(optarg, "swissbit") == 0)
          opt_method = METHOD_SWISSBIT;
        else if (strcmp(optarg, "2step") == 0)
          opt_method = METHOD_2STEP;
        else if (strcmp(optarg, "innodisk") == 0)
          opt_method = METHOD_INNODISK;
        else {
          verbose0("Invalid option to -m (%s), expected one of: %s.\n", optarg, allowed_methods);
          json_builder_free(j);
          exit(2);
        }
        free(opt_method_name); // if -m is called multiple times. no-op if NULL
        opt_method_name = strdup(optarg);
        verbose3("opt_method = %d\n", opt_method);
        break;
      case 'i':
        verbose3("input test file %s\n", optarg);
        free(opt_input_file); // if -i is called multiple times. no-op if NULL
        opt_input_file = strdup(optarg);
        break;
      case 'o':
        verbose3("output file %s\n", optarg);
        free(opt_output_file); // if -o is called multiple times. no-op if NULL
        opt_output_file = strdup(optarg);
        break;
      case 'h':
        print_usage();
        json_builder_free(j);
        exit(0);
      default:
        print_usage();
        json_builder_free(j);
        exit(1);
    }
  }

  if (optind >= argc) {
      print_usage();
      verbose0("Missing device name.\n");
      json_builder_free(j);
      exit(2);
  }

  device = argv[optind];

  json_object_push(j, "device", json_string_new(device));
  json_object_push(j, "addTime", json_boolean_new(opt_addTime ? 1 : 0));
  json_object_push(j, "method", json_string_new(opt_method_name));

  free(opt_method_name); // no longer needed
  opt_method_name = NULL;

  if (opt_input_file && opt_method == METHOD_AUTO) {
    print_usage();
    verbose0("Option -i requires non-auto method (see option -m).\n");
    json_builder_free(j);
    exit(2);
  }

  /*
   * With FreeBSD, make sure kern.geom.debugflags sysctl
   * flag has value of 16. To change, invoke the command
   * "sysctl kern.geom.debugflags=16" or write it in the
   * /etc/sysctl.conf.
   */
  fd = open(device, O_RDWR);
  if (fd < 0) {
    json_object_push(j, "error", json_sprintf_new("device open failed (%s)", strerror(errno)));
    json_print_and_free(j);
    exit(1);
  }

  if (opt_method == METHOD_AUTO || opt_method == METHOD_SANDISK) {
    // first try if read with arg 1 returns anything
    verbose1("Trying sandisk...\n");
    cmd56_arg = 0x00000001;
    ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);
    cmd56_arg1_tried = true;
    // we assume success when the call was successful AND the signature is not 0xff 0xff
    if (ret == 0 && (opt_force || is_data_valid(data_in))) {
      json_object_push(j, "signature", json_sprintf_new("0x%x 0x%x", data_in[0], data_in[1]));

      if (data_in[0] == 0x70 && data_in[1] == 0x58) {
        json_object_push(j, "Longsys", json_boolean_new(1));
        json_object_push(j, "SMARTVersions", json_integer_new(nwordle_to_int(data_in, 4, 4)));
        json_object_push(j, "sizeOfDevSMART", json_integer_new(nwordle_to_int(data_in, 12, 4)));
        json_object_push(j, "originalBadBlock", json_integer_new(nwordle_to_int(data_in, 16, 4)));
        json_object_push(j, "increaseBadBlock", json_integer_new(nwordle_to_int(data_in, 20, 4)));
        json_object_push(j, "writeAllSectNum", json_integer_new(nwordle_to_int(data_in, 24, 8)));
        json_object_push(j, "replaceBlockLeft", json_integer_new(nwordle_to_int(data_in, 32, 4)));
        json_object_push(j, "degreeOfWear", json_sprintf_new("%.02f Cycle", (float)nwordle_to_int(data_in, 36, 4) / 1000.0));
        json_object_push(j, "sectorTotal", json_integer_new(nwordle_to_int(data_in, 40, 4)));
        json_object_push(j, "remainLifeTime", json_sprintf_new("%" PRId64 "%%", nwordle_to_int(data_in, 44, 4)));
        json_object_push(j, "remainWrGBNum", json_sprintf_new("%.02fTB", (float)nwordle_to_int(data_in, 48, 4) / 1024));
        json_object_push(j, "lifeTimeTotal", json_sprintf_new("%.02f Cycle", (float)nwordle_to_int(data_in, 52, 4)));
        json_object_push(j, "phyWrGBNum", json_sprintf_new("%.02fTB", (float)nwordle_to_int(data_in, 56, 4) / 1024));

        close(fd);
        json_object_push(j, "success", json_boolean_new(1));
        json_print_and_free(j);
        exit(0);
      }

      if (data_in[0] == 0x44 && (data_in[1] == 0x53 || data_in[1] == 0x57)) {
        json_object_push(j, "SanDisk", json_boolean_new(1));
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
      json_object_push(j, "manufactureYYMMDD", json_string_new(tmpstr));
      json_object_push(j, "healthStatusPercentUsed", json_integer_new(data_in[8]));
      json_object_push(j, "featureRevision", json_sprintf_new("0x%x", data_in[11]));
      json_object_push(j, "generationIdentifier", json_integer_new(data_in[14]));
      strncpy(tmpstr, (char *)&data_in[49], 32);
      tmpstr[32] = 0;
      json_object_push(j, "productString", json_string_new(tmpstr));
      json_object_push(j, "powerOnTimes", json_integer_new(nwordbe_to_int(data_in, 23, 3)));
      close(fd);
      json_object_push(j, "success", json_boolean_new(1));
      json_print_and_free(j);
      exit(0);
    }
  }

  if (opt_method == METHOD_AUTO || opt_method == METHOD_INNODISK) {
    // try innodisk argument
    verbose1("Trying innodisk...\n");
    cmd56_arg = 0x110005fd;
    ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);

    // we assume success when the call was successful AND the signature is not 0xff 0xff
    if (ret == 0 && (opt_force || is_data_valid(data_in))) {
      json_object_push(j, "signature", json_sprintf_new("0x%x 0x%x", data_in[0], data_in[1]));
      if (data_in[0] == 0x4c && data_in[1] == 0x58) {
        json_object_push(j, "Innodisk", json_boolean_new(1));
        switch (data_in[16]) {
        case 0x00:
          json_object_push(j, "Bus width", json_string_new("1 bit"));
          break;
        case 0x10:
          json_object_push(j, "Bus width", json_string_new("4 bits"));
          break;
        }
        switch (data_in[18]) {
        case 0x00:
          json_object_push(j, "Speed mode", json_string_new("Class 0"));
          break;
        case 0x01:
          json_object_push(j, "Speed mode", json_string_new("Class 2"));
          break;
        case 0x02:
          json_object_push(j, "Speed mode", json_string_new("Class 4"));
          break;
        case 0x03:
          json_object_push(j, "Speed mode", json_string_new("Class 6"));
          break;
        case 0x04:
          json_object_push(j, "Speed mode", json_string_new("Class 10"));
          break;
        }
        switch (data_in[19]) {
        case 0x00:
          json_object_push(j, "UHS speed grade", json_string_new("Less than 10MB/s"));
          break;
        case 0x01:
          json_object_push(j, "UHS speed grade", json_string_new("10MB/s and higher"));
          break;
        case 0x03:
          json_object_push(j, "UHS speed grade", json_string_new("30MB/s and higher"));
          break;
        default:
          json_object_push(j, "UHS speed grade", json_sprintf_new("Unknown: 0x%x", data_in[19]));
          break;
        }
        json_object_push(j, "Total spare blocks cnt", json_integer_new((int)(data_in[24])));
        json_object_push(j, "Factory bad blocks cnt", json_integer_new((int)(data_in[25])));
        json_object_push(j, "Runtime bad blocks cnt", json_integer_new((int)(data_in[26])));
        json_object_push(j, "Spare utilization rate", json_integer_new((int)(data_in[27])));
        json_object_push(j, "SPOR failure cnt", json_integer_new(nwordbe_to_int(data_in, 28, 4)));
        json_object_push(j, "Minimum erase cnt", json_integer_new(nwordbe_to_int(data_in, 32, 4)));
        json_object_push(j, "Maximum erase cnt", json_integer_new(nwordbe_to_int(data_in, 36, 4)));
        json_object_push(j, "Total erase cnt", json_integer_new(nwordbe_to_int(data_in, 40, 4)));
        json_object_push(j, "Average erase cnt", json_integer_new(nwordbe_to_int(data_in, 44, 4)));
        strncpy(tmpstr, (char *)&data_in[53], 16);
        tmpstr[16] = 0;
        json_object_push(j, "FW version", json_string_new(tmpstr));
        close(fd);
        json_object_push(j, "success", json_boolean_new(1));
        json_print_and_free(j);
        exit(0);
      }
    }
  }

  if (opt_method == METHOD_AUTO || opt_method == METHOD_ADATA) {
    //try adata argument
    verbose1("Trying adata...\n");
    cmd56_arg = 0x110005f1;
    ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);
    cmd56_arg1_tried = true;
    // we assume success when the call was successful AND the signature is not 0xff 0xff
    if (ret == 0 && (opt_force || is_data_valid(data_in))) {
      json_object_push(j, "signature", json_sprintf_new("0x%x 0x%x", data_in[0], data_in[1]));
      if (data_in[0] == 0x09 && data_in[1] == 0x41) {
        json_object_push(j, "Adata", json_boolean_new(1));
        json_object_push(j, "Factory bad block cnt", json_integer_new((int)((data_in[24] << 8) + data_in[25])));
        json_object_push(j, "Grown bad block cnt", json_integer_new((int)(data_in[26])));
        json_object_push(j, "Spare SLC block cnt", json_integer_new((int)(data_in[27])));
        json_object_push(j, "Spare block cnt", json_integer_new((int)((data_in[30] << 8) + data_in[31])));
        json_object_push(j, "Data area minimum erase cnt", json_integer_new((long)((data_in[32] << 24) + (data_in[33] << 16) + (data_in[34] << 8) + data_in[35])));
        json_object_push(j, "Data area maximum erase cnt", json_integer_new((long)((data_in[36] << 24) + (data_in[37] << 16) + (data_in[38] << 8) + data_in[39])));
        json_object_push(j, "Data area total erase cnt", json_integer_new((long)((data_in[40] << 24) + (data_in[41] << 16) + (data_in[42] << 8) + data_in[43])));
        json_object_push(j, "Data area average erase cnt", json_integer_new((long)((data_in[44] << 24) + (data_in[45] << 16) + (data_in[46] << 8) + data_in[47])));
        json_object_push(j, "System area minimum erase cnt", json_integer_new((long)((data_in[48] << 24) + (data_in[49] << 16) + (data_in[50] << 8) + data_in[51])));
        json_object_push(j, "System area maximum erase cnt", json_integer_new((long)((data_in[52] << 24) + (data_in[53] << 16) + (data_in[54] << 8) + data_in[55])));
        json_object_push(j, "System area total erase count", json_integer_new((long)((data_in[56] << 24) + (data_in[57] << 16) + (data_in[58] << 8) + data_in[59])));
        json_object_push(j, "System area average erase cnt", json_integer_new((long)((data_in[60] << 24) + (data_in[61] << 16) + (data_in[62] << 8) + data_in[63])));
        json_object_push(j, "Raw card capacity", json_sprintf_new("%ld MB", (long)((data_in[64] << 24) + (data_in[65] << 16) + (data_in[66] << 8) + data_in[67])));
        json_object_push(j, "PE Cycle life", json_integer_new((long)((data_in[68] << 8) + data_in[69])));
        json_object_push(j, "Remaining life", json_sprintf_new("%d%%", (int)data_in[70]));
        json_object_push(j, "Power cycle cnt", json_integer_new((long)((data_in[76] << 24) + (data_in[77] << 16) + (data_in[78] << 8) + data_in[79])));
        json_object_push(j, "Flash ID", json_sprintf_new("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x", data_in[80], data_in[81], data_in[82], data_in[83], data_in[84], data_in[85], data_in[86]));
        json_object_push(j, "Controller", json_sprintf_new("%c%c%c%c%c%c", (char)data_in[88], (char)data_in[89], (char)data_in[90], (char)data_in[91], (char)data_in[92], (char)data_in[93]));
        json_object_push(j, "TLC read reclaim", json_integer_new((long)((data_in[96] << 8) + data_in[97])));
        json_object_push(j, "SLC read reclaim", json_integer_new((long)((data_in[98] << 8) + data_in[99])));
        json_object_push(j, "Firmware block refresh", json_integer_new((long)((data_in[100] << 8) + data_in[101])));
        json_object_push(j, "TLC read threshold", json_integer_new((long)((data_in[104] << 24) + (data_in[105] << 16) + (data_in[106] << 8) + data_in[107])));
        json_object_push(j, "SLC read threshold", json_integer_new((long)((data_in[108] << 24) + (data_in[109] << 16) + (data_in[110] << 8) + data_in[111])));
        json_object_push(j, "FW version", json_sprintf_new("%c%c%c%c%c%c", (char)data_in[128], (char)data_in[129], (char)data_in[130], (char)data_in[131], (char)data_in[132], (char)data_in[133]));
        json_object_push(j, "TLC refresh cnt", json_integer_new((int)((data_in[136] << 24) + (data_in[137] << 16) + (data_in[138] << 8) + data_in[139])));
        json_object_push(j, "SLC refresh cnt", json_integer_new((int)((data_in[140] << 24) + (data_in[141] << 16) + (data_in[143] << 8) + data_in[144])));
        close(fd);
        json_object_push(j, "success", json_boolean_new(1));
        json_print_and_free(j);
        exit(0);
      }
    }
  }

  if (opt_method == METHOD_AUTO || opt_method == METHOD_TRANSCEND) {
    //try transcend argument
    verbose1("Trying transcend...\n");
    cmd56_arg = 0x110005f9;
    ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);
    cmd56_arg1_tried = true;
    // we assume success when the call was successful AND the signature is not 0xff 0xff
    if (ret == 0 && (opt_force || is_data_valid(data_in))) {
      json_object_push(j, "signature", json_sprintf_new("0x%x 0x%x", data_in[0], data_in[1]));
      if (data_in[0] == 0x54 && data_in[1] == 0x72) {
        json_object_push(j, "Transcend", json_boolean_new(1));
        json_object_push(j, "Secured mode", json_integer_new((int)(data_in[11])));
        switch (data_in[16]) {
        case 0x00:
          json_object_push(j, "Bus width", json_string_new("1 bit"));
          break;
        case 0x10:
          json_object_push(j, "Bus width", json_string_new("4 bits"));
          break;
        }
        switch (data_in[18]) {
        case 0x00:
          json_object_push(j, "Speed mode", json_string_new("Class 0"));
          break;
        case 0x01:
          json_object_push(j, "Speed mode", json_string_new("Class 2"));
          break;
        case 0x02:
          json_object_push(j, "Speed mode", json_string_new("Class 4"));
          break;
        case 0x03:
          json_object_push(j, "Speed mode", json_string_new("Class 6"));
          break;
        case 0x04:
          json_object_push(j, "Speed mode", json_string_new("Class 10"));
          break;
        }
        switch (data_in[19]) {
        case 0x00:
          json_object_push(j, "UHS speed grade", json_string_new("Less than 10MB/s"));
          break;
        case 0x01:
          json_object_push(j, "UHS speed grade", json_string_new("10MB/s and higher"));
          break;
        case 0x03:
          json_object_push(j, "UHS speed grade", json_string_new("30MB/s and higher"));
          break;
        }
        json_object_push(j, "New bad blocks cnt", json_sprintf_new("0x%02x", data_in[26]));
        json_object_push(j, "Runtime spare blocks cnt", json_sprintf_new("0x%02x", data_in[27]));
        json_object_push(j, "Abnormal power loss", json_integer_new((long)((data_in[31] << 24) + (data_in[30] << 16) + (data_in[29] << 8) + data_in[28])));
        json_object_push(j, "Minimum erase cnt", json_integer_new((long)((data_in[35] << 24) + (data_in[34] << 16) + (data_in[33] << 8) + data_in[32])));
        json_object_push(j, "Maximum erase cnt", json_integer_new((long)((data_in[39] << 24) + (data_in[38] << 16) + (data_in[37] << 8) + data_in[36])));
        json_object_push(j, "Total erase cnt", json_integer_new((long)((data_in[43]) + (data_in[42]) + (data_in[41]) + data_in[40])));
        json_object_push(j, "Average erase cnt", json_integer_new((long)((data_in[47] << 24) + (data_in[46] << 16) + (data_in[45] << 8) + data_in[44])));

        json_object_push(j, "Remaining card life", json_sprintf_new("%d%%", (int)(data_in[70])));
        json_object_push(j, "Total write CRC cnt", json_integer_new(nwordbe_to_int(data_in, 72, 4)));
        json_object_push(j, "Power cycle cnt", json_integer_new(nwordbe_to_int(data_in, 76, 2)));

        json_object_push(j, "NAND flash ID", json_sprintf_new("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x", data_in[80], data_in[81], data_in[82], data_in[83], data_in[84], data_in[85]));
        json_object_push(j, "IC", json_sprintf_new("%c%c%c%c%c%c%c%c", data_in[87], data_in[88], data_in[89], data_in[90], data_in[91], data_in[92], data_in[93], data_in[94]));
        json_object_push(j, "fw version", json_sprintf_new("%c%c%c%c%c%c", data_in[128], data_in[129], data_in[130], data_in[131], data_in[132], data_in[133]));

        close(fd);
        json_object_push(j, "success", json_boolean_new(1));
        json_print_and_free(j);
        exit(0);
      }
    }
  }

  if (opt_method == METHOD_AUTO || opt_method == METHOD_MICRON) {
    // try micron argument
    verbose1("Trying micron...\n");
    cmd56_arg = 0x110005fb;
    ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);
    cmd56_arg1_tried = true;
    // we assume success when the call was successful AND the signature is not 0xff 0xff
    if (ret == 0 && (opt_force || is_data_valid(data_in))) {
      json_object_push(j, "signature", json_sprintf_new("0x%x 0x%x", data_in[0], data_in[1]));
      if (data_in[0] == 0x4d && data_in[1] == 0x45) {
        json_object_push(j, "Micron", json_boolean_new(1));
        json_object_push(j, "Percentange step utilization", json_integer_new((int)(data_in[7])));
        json_object_push(j, "TLC area utilization", json_integer_new((int)(data_in[8])));
        json_object_push(j, "SLC area utilization", json_integer_new((int)(data_in[9])));
        close(fd);
        json_object_push(j, "success", json_boolean_new(1));
        json_print_and_free(j);
        exit(0);
      }
    }
  }

  if (opt_method == METHOD_AUTO || opt_method == METHOD_SWISSBIT) {
    // try swissbit argument
    verbose1("Trying swissbit...\n");
    cmd56_arg = 0x53420001;
    ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);
    cmd56_arg1_tried = true;
    // we assume success when the call was successful AND the signature is not 0xff 0xff
    if (ret == 0 && (opt_force || is_data_valid(data_in))) {
      json_object_push(j, "signature", json_sprintf_new("0x%x 0x%x", data_in[0], data_in[1]));

      if (data_in[0] == 0x53 && data_in[1] == 0x77) {
        json_object_push(j, "Swissbit", json_boolean_new(1));
        strncpy(tmpstr, (char *)&data_in[32], 16);
        tmpstr[16] = 0;
        json_object_push(j, "fwVersion", json_string_new(tmpstr));
        json_object_push(j, "User area rated cycles", json_integer_new(nwordbe_to_int(data_in, 48, 4)));
        json_object_push(j, "User area max cycle cnt", json_integer_new(nwordbe_to_int(data_in, 52, 4)));
        json_object_push(j, "User area total cycle cnt", json_integer_new(nwordbe_to_int(data_in, 56, 4)));
        json_object_push(j, "User area average cycle cnt", json_integer_new(nwordbe_to_int(data_in, 60, 4)));
        json_object_push(j, "User area max cycle cnt", json_integer_new(nwordbe_to_int(data_in, 68, 4)));
        json_object_push(j, "User area total cycle cnt", json_integer_new(nwordbe_to_int(data_in, 72, 4)));
        json_object_push(j, "User area average cycle cnt", json_integer_new(nwordbe_to_int(data_in, 76, 4)));
        json_object_push(j, "Remaining Lifetime Percent", json_integer_new((int)(data_in[80])));
        switch (data_in[86]) {
        case 0x00:
          json_object_push(j, "Speed mode", json_string_new("Default speed"));
          break;
        case 0x01:
          json_object_push(j, "Speed mode", json_string_new("High speed"));
          break;
        case 0x10:
          json_object_push(j, "Speed mode", json_string_new("SDR12 speed"));
          break;
        case 0x11:
          json_object_push(j, "Speed mode", json_string_new("SDR25 speed"));
          break;
        case 0x12:
          json_object_push(j, "Speed mode", json_string_new("SDR50 speed"));
          break;
        case 0x14:
          json_object_push(j, "Speed mode", json_string_new("DDR50 speed"));
          break;
        case 0x18:
          json_object_push(j, "Speed mode", json_string_new("SDR104 speed"));
          break;
        }
        switch (data_in[87]) {
        case 0x00:
          json_object_push(j, "Bus width", json_string_new("1 bit"));
          break;
        case 0x10:
          json_object_push(j, "Bus width", json_string_new("4 bits"));
          break;
        }
        json_object_push(j, "User area spare blocks cnt", json_integer_new(nwordbe_to_int(data_in, 88, 4)));
        json_object_push(j, "System area spare blocks cnt", json_integer_new(nwordbe_to_int(data_in, 92, 4)));
        json_object_push(j, "User area runtime bad blocks cnt", json_integer_new(nwordbe_to_int(data_in, 96, 4)));
        json_object_push(j, "System area runtime bad blocks cnt", json_integer_new(nwordbe_to_int(data_in, 100, 4)));
        json_object_push(j, "User area refresh cnt", json_integer_new(nwordbe_to_int(data_in, 104, 4)));
        json_object_push(j, "System area refresh cnt", json_integer_new(nwordbe_to_int(data_in, 108, 4)));
        json_object_push(j, "Interface crc cnt", json_integer_new(nwordbe_to_int(data_in, 112, 4)));
        json_object_push(j, "Power cycle cnt", json_integer_new(nwordbe_to_int(data_in, 116, 4)));
        close(fd);
        json_object_push(j, "success", json_boolean_new(1));
        json_print_and_free(j);
        exit(0);
      }
    }
  }

  if (cmd56_arg1_tried) {
    // at least one method has been tried above
    if (opt_method != METHOD_AUTO) {
      // a specific method was asked, wether it worked or not, stop here
      if (ret == 0) {
        json_object_push(j, "read_via_cmd56_arg_1", json_sprintf_new("read successful but signature 0x%x 0x%x", data_in[0], data_in[1]));
      }
      else {
        json_object_push(j, "read_via_cmd56_arg_1", json_sprintf_new("not implemented: %s", strerror(errno)));
      }
      json_print_and_free(j);
      exit(1);
    }
    else {
      // we're in auto mode, store cmd56 info and continue trying ore methods below
      if (ret == 0) {
        json_object_push(j, "read_via_cmd56_arg_1", json_sprintf_new("read successful but signature 0x%x 0x%x", data_in[0], data_in[1]));
      } else {
        json_object_push(j, "read_via_cmd56_arg_1", json_sprintf_new("not implemented: %s", strerror(errno)));
      }
    }
  }

  if (opt_method == METHOD_AUTO || opt_method == METHOD_2STEP) {
    verbose1("Trying 2step...\n");
    // prepare for health command
    cmd56_arg = 0x00000010; // all other are 0
    ret = CMD56_write(fd, cmd56_arg);
    if (ret) {
      json_object_push(j, "error1", json_sprintf_new("1st CMD56 CALL FAILED: %s", strerror(errno)));
      // exit(1);
    }

    if (opt_addTime) {
      usleep(1000000);
    }

    // do query smart data
    cmd56_arg = 0x00000021;
    ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);
    if (ret) {
      json_object_push(j, "error2", json_sprintf_new("2nd CMD56 CALL FAILED: %s", strerror(errno)));
      json_print_and_free(j);
      exit(1);
    }

    // if opt_addTime has not been specified, and the returned block is invalid (filled with 0x00 of 0xFF),
    // then try to sleep and re-issue the command, just in case
    if (!opt_addTime && !is_data_valid(data_in)) {
      verbose2("Returned block is empty, trying to sleep and re-issue the command...");
      usleep(1000000);

      cmd56_arg = 0x00000021;
      ret = CMD56_data_in(fd, cmd56_arg, data_in, opt_input_file, opt_output_file);
      if (ret) {
        json_object_push(j, "error2", json_sprintf_new("2nd CMD56 CALL FAILED: %s", strerror(errno)));
        json_print_and_free(j);
        exit(1);
      }
    }

    json_object_push(j, "flashId", json_array_build("0x%02x", data_in, 0, 9));
    json_object_push(j, "icVersion", json_array_build("0x%02x", data_in, 9, 2));
    json_object_push(j, "fwVersion", json_array_build("%02d", data_in, 11, 2)); // show in decimal
    json_object_push(j, "ceNumber", json_sprintf_new("0x%02x", data_in[14]));

    // printf("\"badBlockReplaceMaximum\": [\"0x%02x\",\"0x%02x\"],\n", data_in[16], data_in[17]);
    // badBlockReplaceMaximum is spareBlockCount
    json_object_push(j, "spareBlockCount", json_integer_new(nwordbe_to_int(data_in, 16, 2)));

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
    json_object_push(j, "initialBadBlockCount", json_integer_new(sum));

    // printf("\"goodBlockRatePercentBytes\": [\"0x%02x\",\"0x%02x\"],\n", data_in[64], data_in[65]);
    // printf("\"goodBlockRatePercent\": %d,\n", (int)((data_in[64]<<8)+data_in[65]));
    json_object_push(j, "goodBlockRatePercent", json_double_new((double)nwordbe_to_int(data_in, 64, 2) / 100.0));

    json_object_push(j, "totalEraseCount", json_integer_new(nwordbe_to_int(data_in, 80, 4)));

    // printf("\"enduranceRemainLifePercentBytes\": [\"0x%02x\",\"0x%02x\"],\n", data_in[96], data_in[97]);
    // printf("\"enduranceRemainLifePercent\": %d,\n", (int)((data_in[96]<<8)+data_in[97]));
    json_object_push(j, "enduranceRemainLifePercent", json_double_new((double)nwordbe_to_int(data_in, 96, 2) / 100.0));
    json_object_push(j, "avgEraseCount", json_integer_new((nwordbe_to_int(data_in, 104, 2) << 16) + nwordbe_to_int(data_in, 98, 2)));
    json_object_push(j, "minEraseCount", json_integer_new((nwordbe_to_int(data_in, 106, 2) << 16) + nwordbe_to_int(data_in, 100, 2)));
    json_object_push(j, "maxEraseCount", json_integer_new((nwordbe_to_int(data_in, 108, 2) << 16) + nwordbe_to_int(data_in, 102, 2)));
    json_object_push(j, "powerUpCount", json_integer_new(nwordbe_to_int(data_in, 112, 4)));
    json_object_push(j, "abnormalPowerOffCount", json_integer_new(nwordbe_to_int(data_in, 128, 2)));
    json_object_push(j, "totalRefreshCount", json_integer_new(nwordbe_to_int(data_in, 160, 2)));
    json_object_push(j, "productMarker", json_array_build("0x%02x", data_in, 176, 8));
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
    json_object_push(j, "laterBadBlockCount", json_integer_new(sum));

    // This seems to be the "signature" of Kingston SDCE cards, and there are undocumented fields
    // that look a lot like 4-bytes long words, and they're incrementing.
    // So far only the word at offset 328 has been found/guessed.
    const unsigned char flashIdKingstonSDCE[] = {0x98,0x3e,0xa8,0x03,0x7a,0xe4,0x08,0x16,0x00};
    if (memcmp(data_in, flashIdKingstonSDCE, sizeof(flashIdKingstonSDCE)) == 0) {
      json_object_push(j, "undocumented300", json_integer_new(nwordbe_to_int(data_in, 300, 4)));
      json_object_push(j, "undocumented304", json_integer_new(nwordbe_to_int(data_in, 304, 4)));
      json_object_push(j, "undocumented308", json_integer_new(nwordbe_to_int(data_in, 308, 4)));
      json_object_push(j, "undocumented312", json_integer_new(nwordbe_to_int(data_in, 312, 4)));
      json_object_push(j, "undocumented316", json_integer_new(nwordbe_to_int(data_in, 316, 4)));
      json_object_push(j, "undocumented320", json_integer_new(nwordbe_to_int(data_in, 320, 4)));
      json_object_push(j, "undocumented324", json_integer_new(nwordbe_to_int(data_in, 324, 4)));

      json_object_push(j, "sectorsWrittenCount", json_integer_new(nwordbe_to_int(data_in, 328, 4))); // found scrutating real-life data
      json_object_push(j, "bytesWrittenCount", json_integer_new(nwordbe_to_int(data_in, 328, 4) << 9)); // found scrutating real-life data

      json_object_push(j, "undocumented332", json_integer_new(nwordbe_to_int(data_in, 332, 4)));
    }

    close(fd);
    json_object_push(j, "success", json_boolean_new(1));
    json_print_and_free(j);
    exit(0);
  }

}
