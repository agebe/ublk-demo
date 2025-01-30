// copied (and modified) from https://github.com/ublk-org/ublksrv/blob/master/demo_null.c
// SPDX-License-Identifier: MIT or GPL-2.0-only

#include <argp.h>
#include <ublksrv/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <getopt.h>
#include <stdarg.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <ublksrv.h>
#include <ublksrv_utils.h>

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
unsigned long long to_bytes(const char *input) {
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

struct demo_queue_info {
	const struct ublksrv_dev *dev;
	int qid;
	pthread_t thread;
};

static struct ublksrv_ctrl_dev *this_dev;

static pthread_mutex_t jbuf_lock;
static char jbuf[4096];

static __u8* demoDisk;
static __u64 demoDiskSize;

static void sig_handler(int sig)
{
	fprintf(stderr, "got signal %d\n", sig);
	ublksrv_ctrl_stop_dev(this_dev);
}

/*
 * io handler for each ublkdev's queue
 *
 * Just for showing how to build ublksrv target's io handling, so callers
 * can apply these APIs in their own thread context for making one ublk
 * block device.
 */
static void *demo_null_io_handler_fn(void *data)
{
	struct demo_queue_info *info = (struct demo_queue_info *)data;
	const struct ublksrv_dev *dev = info->dev;
	const struct ublksrv_ctrl_dev_info *dinfo =
		ublksrv_ctrl_get_dev_info(ublksrv_get_ctrl_dev(dev));
	unsigned dev_id = dinfo->dev_id;
	unsigned short q_id = info->qid;
	const struct ublksrv_queue *q;

	sched_setscheduler(getpid(), SCHED_RR, NULL);

	pthread_mutex_lock(&jbuf_lock);
	ublksrv_json_write_queue_info(ublksrv_get_ctrl_dev(dev), jbuf, sizeof jbuf,
			q_id, ublksrv_gettid());
	pthread_mutex_unlock(&jbuf_lock);
	q = ublksrv_queue_init(dev, q_id, NULL);
	if (!q) {
		fprintf(stderr, "ublk dev %d queue %d init queue failed\n",
				dinfo->dev_id, q_id);
		return NULL;
	}

	fprintf(stdout, "tid %d: ublk dev %d queue %d started\n",
			ublksrv_gettid(),
			dev_id, q->q_id);
	do {
		if (ublksrv_process_io(q) < 0)
			break;
	} while (1);

	fprintf(stdout, "ublk dev %d queue %d exited\n", dev_id, q->q_id);
	ublksrv_queue_deinit(q);
	return NULL;
}

static void demo_null_set_parameters(struct ublksrv_ctrl_dev *cdev,
		const struct ublksrv_dev *dev)
 {
	const struct ublksrv_ctrl_dev_info *info =
		ublksrv_ctrl_get_dev_info(cdev);
	struct ublk_params p = {
		.types = UBLK_PARAM_TYPE_BASIC,
		.basic = {
			.logical_bs_shift	= 9,
			.physical_bs_shift	= 12,
			.io_opt_shift		= 12,
			.io_min_shift		= 9,
			.max_sectors		= info->max_io_buf_bytes >> 9,
			.dev_sectors		= dev->tgt.dev_size >> 9,
		},
	};
	int ret;

	pthread_mutex_lock(&jbuf_lock);
	ublksrv_json_write_params(&p, jbuf, sizeof jbuf);
	pthread_mutex_unlock(&jbuf_lock);

	ret = ublksrv_ctrl_set_params(cdev, &p);
	if (ret)
		fprintf(stderr, "dev %d set basic parameter failed %d\n",
				info->dev_id, ret);
}

static int demo_null_io_handler(struct ublksrv_ctrl_dev *ctrl_dev)
{
	int ret, i;
	const struct ublksrv_dev *dev;
	struct demo_queue_info *info_array;
	void *thread_ret;
	const struct ublksrv_ctrl_dev_info *dinfo =
		ublksrv_ctrl_get_dev_info(ctrl_dev);

	info_array = (struct demo_queue_info *)
		calloc(sizeof(struct demo_queue_info), dinfo->nr_hw_queues);
	if (!info_array)
		return -ENOMEM;

	dev = ublksrv_dev_init(ctrl_dev);
	if (!dev) {
		free(info_array);
		return -ENOMEM;
	}

	for (i = 0; i < dinfo->nr_hw_queues; i++) {
		info_array[i].dev = dev;
		info_array[i].qid = i;
		pthread_create(&info_array[i].thread, NULL,
				demo_null_io_handler_fn,
				&info_array[i]);
	}

	demo_null_set_parameters(ctrl_dev, dev);

	/* everything is fine now, start us */
	ret = ublksrv_ctrl_start_dev(ctrl_dev, getpid());
	if (ret < 0)
		goto fail;

	ublksrv_ctrl_get_info(ctrl_dev);
	ublksrv_ctrl_dump(ctrl_dev, jbuf);

	/* wait until we are terminated */
	for (i = 0; i < dinfo->nr_hw_queues; i++)
		pthread_join(info_array[i].thread, &thread_ret);
 fail:
	ublksrv_dev_deinit(dev);

	free(info_array);

	return ret;
}

static int ublksrv_start_daemon(struct ublksrv_ctrl_dev *ctrl_dev)
{
	int ret;

	if (ublksrv_ctrl_get_affinity(ctrl_dev) < 0)
		return -1;

	ret = demo_null_io_handler(ctrl_dev);

	return ret;
}



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
  __u8 j=0;
  for(unsigned long long i=0;i<tgt->dev_size;i++) {
    demoDisk[i] = j++;
  }

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

void *null_alloc_io_buf(const struct ublksrv_queue *q, int tag, int size)
{
	return malloc(size);
}

void null_free_io_buf(const struct ublksrv_queue *q, void *buf, int tag)
{
	free(buf);
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
	struct ublksrv_dev_data data = {
		.dev_id = -1,
		.max_io_buf_bytes = DEF_BUF_SIZE,
		.nr_hw_queues = DEF_NR_HW_QUEUES,
		.queue_depth = DEF_QD,
		.tgt_type = "demo_ramdisk",
		.tgt_ops = &demo_tgt_type,
		.flags = 0,
	};
	struct ublksrv_ctrl_dev *dev;
	int ret;
	static const struct option longopts[] = {
		{ "buf",		1,	NULL, 'b' },
		{ "need_get_data",	1,	NULL, 'g' },
		{ NULL }
	};
	int opt;
	bool use_buf = false;

	while ((opt = getopt_long(argc, argv, ":bg",
				  longopts, NULL)) != -1) {
		switch (opt) {
		case 'b':
			use_buf = true;
			break;
		case 'g':
			data.flags |= UBLK_F_NEED_GET_DATA;
			break;
		}
	}

	if (signal(SIGTERM, sig_handler) == SIG_ERR)
		error(EXIT_FAILURE, errno, "signal");
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		error(EXIT_FAILURE, errno, "signal");

	if (use_buf) {
		demo_tgt_type.alloc_io_buf = null_alloc_io_buf;
		demo_tgt_type.free_io_buf = null_free_io_buf;
	}

	pthread_mutex_init(&jbuf_lock, NULL);
	dev = ublksrv_ctrl_init(&data);
	if (!dev)
		error(EXIT_FAILURE, ENODEV, "ublksrv_ctrl_init");
	/* ugly, but signal handler needs this_dev */
	this_dev = dev;

	ret = ublksrv_ctrl_add_dev(dev);
	if (ret < 0) {
		error(0, -ret, "can't add dev %d", data.dev_id);
		goto fail;
	}

	ret = ublksrv_start_daemon(dev);
	if (ret < 0) {
		error(0, -ret, "can't start daemon");
		goto fail_del_dev;
	}

	ublksrv_ctrl_del_dev(dev);
	ublksrv_ctrl_deinit(dev);
	exit(EXIT_SUCCESS);

 fail_del_dev:
	ublksrv_ctrl_del_dev(dev);
 fail:
	ublksrv_ctrl_deinit(dev);

	exit(EXIT_FAILURE);
}
