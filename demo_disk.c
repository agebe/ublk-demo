// copied (and modified) from https://github.com/ublk-org/ublksrv/blob/master/demo_null.c
// SPDX-License-Identifier: MIT or GPL-2.0-only

#include <argp.h>
#include <stdlib.h>
#include <string.h>

#include <ublksrv.h>

#include "ublk.h"
#include "utils.h"

// what is that for?
#define UBLKSRV_TGT_TYPE_DEMO  0

typedef struct Arguments {
  bool verbose;
  __u64 size;
  char* file;
} Arguments;

Arguments args;

const char *argp_program_version = "demo_ramdisk 0.1.0";
const char *argp_program_bug_address = "andre.gebers@gmail.com";
static char doc[] = "";
static char args_doc[] = "";
static struct argp_option options[] = {
    { "verbose", 'v', 0, 0, "enable verbose output"},
    { "size", 's', "<size>", 0, "size of ramdisk in bytes (k/m/g/t) e.g. 1g"},
    { "file", 'f', "<file>", 0, "file that backs the disk"},
    { 0 }
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  Arguments *arguments = state->input;
  switch (key) {
    case 'v': arguments->verbose = true; break;
    case 's': arguments->size = stringToByteSize(arg); break;
    case 'f': arguments->file = strdup(arg); break;
    case ARGP_KEY_ARG: return 0;
    default: return ARGP_ERR_UNKNOWN;
  }
  return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };

//static __u8* demoDisk;
//static __u64 demoDiskSize;
FILE *file;

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

  //demoDiskSize = tgt->dev_size;
  //demoDisk = malloc(tgt->dev_size);

  if(args.file == NULL) {
    fprintf(stderr, "no file given\n");
    exit(1);
  }
  file = fopen(args.file, "rb+");
  if (file == NULL) {
    fprintf(stderr, "error opening file '%s'\n", args.file);
    exit(1);
    }
	ublksrv_json_write_dev_info(ublksrv_get_ctrl_dev(dev), jbuf, sizeof jbuf);
	ublksrv_json_write_target_base_info(jbuf, sizeof jbuf, &tgt_json);

	return 0;
}

static int demo_handle_io_async(const struct ublksrv_queue *q, const struct ublk_io_data *data) {
  const struct ublksrv_io_desc *iod = data->iod;
  __u8 op = ublksrv_get_op(iod);
  __u64 position = iod->start_sector << 9;
  __u64 bytes = iod->nr_sectors << 9;
  __u64 endOffset = position + bytes;
  if(op == UBLK_IO_OP_READ) {
    if(args.verbose) {
    fprintf(stdout, "handle UBLK_IO_OP_READ, nr_sectors '%d', start_sector '%llu'\n",
      iod->nr_sectors,
      iod->start_sector);
    }
    if(endOffset <= args.size) {
      if(fseek(file, position, SEEK_SET) == 0) {
        fread((void*)iod->addr, 1, bytes, file);
        // TODO check for errors
      } else {
        perror("Error seeking in file");
      }
    } else {
      fprintf(stderr, "end offset out of disk bounds\n");
    }
  } else if(op == UBLK_IO_OP_WRITE) {
    if(args.verbose) {
    fprintf(stdout, "handle UBLK_IO_OP_WRITE, nr_sectors '%d', start_sector '%llu'\n",
      iod->nr_sectors,
      iod->start_sector);
    }
    if(endOffset <= args.size) {
      if(fseek(file, position, SEEK_SET) == 0) {
        fwrite((void*)iod->addr, 1, bytes, file);
        // TODO check for errors
      } else {
        perror("Error seeking in file");
      }
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
