// copied (and modified) from https://github.com/ublk-org/ublksrv/blob/master/demo_null.c
// SPDX-License-Identifier: MIT or GPL-2.0-only

#include <argp.h>
//#include <ublksrv/config.h>
//#include <stdio.h>
#include <stdlib.h>
//#include <sched.h>
//#include <pthread.h>
//#include <getopt.h>
//#include <stdarg.h>
//#include <errno.h>
//#include <error.h>
#include <string.h>
//#include <sys/types.h>
//#include <unistd.h>

#include <ublksrv.h>
//#include <ublksrv_utils.h>

#include "ublk.h"

// what is that for?
#define UBLKSRV_TGT_TYPE_DEMO  0

typedef struct Arguments {
  bool verbose;
  unsigned long size;
} Arguments;

Arguments args;

const char *argp_program_version = "demo_ramdisk 0.1.0";
const char *argp_program_bug_address = "andre.gebers@gmail.com";
static char doc[] = "";
static char args_doc[] = "";
static struct argp_option options[] = {
    { "verbose", 'v', 0, 0, "enable verbose output"},
    { "size", 's', "<size>", 0, "size of ramdisk in bytes (k/m/g/t) e.g. 1g"},
    { 0 }
};

// Function to convert string to unsigned long long (to handle larger numbers)
static unsigned long long to_bytes(const char *input) {
    //printf("to_bytes input %s\n", input);
    unsigned long long result = 0;
    char unit[10];
    double value = 0;
    int i;
    // Parse the number part of the string
    for (i = 0; input[i] != '\0' && isdigit(input[i]); i++) {
        value = value * 10 + (input[i] - '0');
    }
    // Skip any spaces or decimal points (for simplicity, we'll ignore fractional parts)
    while (isspace(input[i]) || input[i] == '.') i++;
    // Parse the unit
    int u = 0;
    while (input[i] && u < 9) {
        unit[u++] = tolower(input[i++]);
    }
    unit[u] = '\0';
    // Convert based on the unit
    if (strcmp(unit, "b") == 0 || strcmp(unit, "byte") == 0) {
        result = (unsigned long long)value;
    } else if (strcmp(unit, "kb") == 0 || strcmp(unit, "k") == 0 || strcmp(unit, "kilobyte") == 0) {
        result = (unsigned long long)(value * 1024);
    } else if (strcmp(unit, "mb") == 0 || strcmp(unit, "m") == 0 || strcmp(unit, "megabyte") == 0) {
        result = (unsigned long long)(value * 1024 * 1024);
    } else if (strcmp(unit, "gb") == 0 || strcmp(unit, "g") == 0 || strcmp(unit, "gigabyte") == 0) {
        result = (unsigned long long)(value * 1024 * 1024 * 1024);
    } else if (strcmp(unit, "tb") == 0 || strcmp(unit, "t") == 0 || strcmp(unit, "terabyte") == 0) {
        result = (unsigned long long)(value * 1024 * 1024 * 1024 * 1024);
    } else {
        fprintf(stderr, "Unknown unit: %s\n", unit);
        return 0; // or some error value
    }
    return result;
}

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  Arguments *arguments = state->input;
  switch (key) {
    case 'v': arguments->verbose = true; break;
    case 's': arguments->size = to_bytes(arg); break;
    case ARGP_KEY_ARG: return 0;
    default: return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

static __u8* demoDisk;
static __u64 demoDiskSize;

static int demo_init_tgt(struct ublksrv_dev *dev, int type, int argc,
		char *argv[])
{
	const struct ublksrv_ctrl_dev_info *info =
		ublksrv_ctrl_get_dev_info(ublksrv_get_ctrl_dev(dev));
	struct ublksrv_tgt_info *tgt = &dev->tgt;
	struct ublksrv_tgt_base_json tgt_json = {
		.type = type,
	};
        strcpy(tgt_json.name, "ramdisk");


	if (type != UBLKSRV_TGT_TYPE_DEMO)
		return -1;

	//tgt_json.dev_size = tgt->dev_size = 250UL * 1024 * 1024 * 1024;
	//tgt_json.dev_size = tgt->dev_size = 10UL * 1024 * 1024;
  //tgt_json.dev_size = tgt->dev_size = args.size * 1024 * 1024;
  tgt_json.dev_size = tgt->dev_size = args.size;
	tgt->tgt_ring_depth = info->queue_depth;
	tgt->nr_fds = 0;

  demoDiskSize = tgt->dev_size;
  demoDisk = malloc(tgt->dev_size);
  
	ublksrv_json_write_dev_info(ublksrv_get_ctrl_dev(dev), jbuf, sizeof jbuf);
	ublksrv_json_write_target_base_info(jbuf, sizeof jbuf, &tgt_json);

	return 0;
}

static int demo_handle_io_async(const struct ublksrv_queue *q, const struct ublk_io_data *data) {
  const struct ublksrv_io_desc *iod = data->iod;
  __u8 op = ublksrv_get_op(iod);
  if(op == UBLK_IO_OP_READ) {
    if(args.verbose) {
    fprintf(stdout, "handle UBLK_IO_OP_READ, nr_sectors '%d', start_sector '%llu'\n",
      iod->nr_sectors,
      iod->start_sector);
    }
    __u64 endOffset = (iod->start_sector << 9) + (iod->nr_sectors << 9);
    if(endOffset <= demoDiskSize) {
      memcpy((void*)iod->addr, demoDisk+(iod->start_sector << 9), iod->nr_sectors << 9);
    } else {
      fprintf(stdout, "end offset out of disk bounds\n");
    }
  } else if(op == UBLK_IO_OP_WRITE) {
    if(args.verbose) {
    fprintf(stdout, "handle UBLK_IO_OP_WRITE, nr_sectors '%d', start_sector '%llu'\n",
      iod->nr_sectors,
      iod->start_sector);
    }
    __u64 endOffset = (iod->start_sector << 9) + (iod->nr_sectors << 9);
    if(endOffset <= demoDiskSize) {
      memcpy(demoDisk+(iod->start_sector << 9), (void*)iod->addr, iod->nr_sectors << 9);
    } else {
      fprintf(stdout, "end offset out of disk bounds\n");
    }
  } else {
    fprintf(stdout, "unsupported op '%d'\n", op);
  }
  ublksrv_complete_io(q, data->tag, iod->nr_sectors << 9);
// grok:
// Returning 0 might imply that the operation was either completed immediately (synchronous behavior within an async context)
// or was not processed - for example, if the operation was invalid or not applicable.
// However, 0 is less commonly used as a success indicator here,
// as async operations typically involve some form of future completion notification.
// Successfully submitted, return a positive value or iod->tag for correlation
//  return iod->tag;
  return 0;
}

static struct ublksrv_tgt_type demo_tgt_type = {
	.type	= UBLKSRV_TGT_TYPE_DEMO,
	.name	=  "demo_ramdisk",
	.init_tgt = demo_init_tgt,
	.handle_io_async = demo_handle_io_async,
	//.alloc_io_buf = null_alloc_io_buf,
	//.free_io_buf = null_free_io_buf,
};

int main(int argc, char *argv[]) {
  args.verbose = false;
  args.size = 10UL * 1024 * 1024;
  argp_parse(&argp, argc, argv, 0, 0, &args);
  if(args.verbose) {
    printf("ramdisk size bytes '%lu'\n", args.size);
  }
  return startUblk(&demo_tgt_type, false);
}
