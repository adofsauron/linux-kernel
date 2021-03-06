/* Copyright (c) 2004 Coraid, Inc.  See COPYING for GPL terms. */
#define VERSION "12"
#define AOE_MAJOR 152
#define DEVICE_NAME "aoe"

/* set AOE_PARTITIONS to 1 to use whole-disks only
 * default is 16, which is 15 partitions plus the whole disk
 */
#ifndef AOE_PARTITIONS
#define AOE_PARTITIONS (16)
#endif

#define SYSMINOR(aoemajor, aoeminor) ((aoemajor) * NPERSHELF + (aoeminor))
#define AOEMAJOR(sysminor) ((sysminor) / NPERSHELF)
#define AOEMINOR(sysminor) ((sysminor) % NPERSHELF)
#define WHITESPACE " \t\v\f\n"

enum {
	AOECMD_ATA,
	AOECMD_CFG,

	AOEFL_RSP = (1<<3),
	AOEFL_ERR = (1<<2),

	AOEAFL_EXT = (1<<6),
	AOEAFL_DEV = (1<<4),
	AOEAFL_ASYNC = (1<<1),
	AOEAFL_WRITE = (1<<0),

	AOECCMD_READ = 0,
	AOECCMD_TEST,
	AOECCMD_PTEST,
	AOECCMD_SET,
	AOECCMD_FSET,

	AOE_HVER = 0x10,
};

struct aoe_hdr {
	unsigned char dst[6];
	unsigned char src[6];
	__be16 type;
	unsigned char verfl;
	unsigned char err;
	__be16 major;
	unsigned char minor;
	unsigned char cmd;
	__be32 tag;
};

struct aoe_atahdr {
	unsigned char aflags;
	unsigned char errfeat;
	unsigned char scnt;
	unsigned char cmdstat;
	unsigned char lba0;
	unsigned char lba1;
	unsigned char lba2;
	unsigned char lba3;
	unsigned char lba4;
	unsigned char lba5;
	unsigned char res[2];
};

struct aoe_cfghdr {
	__be16 bufcnt;
	__be16 fwver;
	unsigned char res;
	unsigned char aoeccmd;
	unsigned char cslen[2];
};

enum {
	DEVFL_UP = 1,	/* device is installed in system and ready for AoE->ATA commands */
	DEVFL_TKILL = (1<<1),	/* flag for timer to know when to kill self */
	DEVFL_EXT = (1<<2),	/* device accepts lba48 commands */
	DEVFL_CLOSEWAIT = (1<<3), /* device is waiting for all closes to revalidate */
	DEVFL_WC_UPDATE = (1<<4), /* this device needs to update write cache status */
	DEVFL_WORKON = (1<<4),

	BUFFL_FAIL = 1,
};

enum {
	MAXATADATA = 1024,
	NPERSHELF = 16,		/* number of slots per shelf address */
	FREETAG = -1,
	MIN_BUFS = 8,
};

struct buf {
	struct list_head bufs;
	ulong start_time;	/* for disk stats */
	ulong flags;
	ulong nframesout;
	char *bufaddr;
	ulong resid;
	ulong bv_resid;
	sector_t sector;
	struct bio *bio;
	struct bio_vec *bv;
};

struct frame {
	int tag;
	ulong waited;
	struct buf *buf;
	char *bufaddr;
	int writedatalen;
	int ndata;

	/* largest possible */
	unsigned char data[sizeof(struct aoe_hdr) + sizeof(struct aoe_atahdr)];
};

struct aoedev {
	struct aoedev *next;
	unsigned char addr[6];	/* remote mac addr */
	ushort flags;
	ulong sysminor;
	ulong aoemajor;
	ulong aoeminor;
	ulong nopen;		/* (bd_openers isn't available without sleeping) */
	ulong rttavg;		/* round trip average of requests/responses */
	u16 fw_ver;		/* version of blade's firmware */
	struct work_struct work;/* disk create work struct */
	struct gendisk *gd;
	request_queue_t blkq;
	struct hd_geometry geo; 
	sector_t ssize;
	struct timer_list timer;
	spinlock_t lock;
	struct net_device *ifp;	/* interface ed is attached to */
	struct sk_buff *sendq_hd; /* packets needing to be sent, list head */
	struct sk_buff *sendq_tl;
	mempool_t *bufpool;	/* for deadlock-free Buf allocation */
	struct list_head bufq;	/* queue of bios to work on */
	struct buf *inprocess;	/* the one we're currently working on */
	ulong lasttag;		/* last tag sent */
	ulong nframes;		/* number of frames below */
	struct frame *frames;
};


int aoeblk_init(void);
void aoeblk_exit(void);
void aoeblk_gdalloc(void *);
void aoedisk_rm_sysfs(struct aoedev *d);

int aoechr_init(void);
void aoechr_exit(void);
void aoechr_error(char *);

void aoecmd_work(struct aoedev *d);
void aoecmd_cfg(ushort, unsigned char);
void aoecmd_ata_rsp(struct sk_buff *);
void aoecmd_cfg_rsp(struct sk_buff *);

int aoedev_init(void);
void aoedev_exit(void);
struct aoedev *aoedev_by_aoeaddr(int maj, int min);
void aoedev_downdev(struct aoedev *d);
struct aoedev *aoedev_set(ulong, unsigned char *, struct net_device *, ulong);
int aoedev_busy(void);

int aoenet_init(void);
void aoenet_exit(void);
void aoenet_xmit(struct sk_buff *);
int is_aoe_netif(struct net_device *ifp);
int set_aoe_iflist(const char __user *str, size_t size);

u64 mac_addr(char addr[6]);
