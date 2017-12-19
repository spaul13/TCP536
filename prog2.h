#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
                           /* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */

#define CHKSUM_WRONG 0
#define CHKSUM_CORRECT 1

#define SIZE_TX_BUFFER_ARQ 2 // 51
#define SIZE_TX_BUFFER_GBN 51
#define SIZE_RX_BUFFER 51

#define PKT_DROP 0
#define PKT_SAVE 1

#define MAX_INIT_SEQ_ACK 256
#define MAX_SEQ_ACK 2

#define MODE_SEND 0
#define MODE_RESEND 1

#define MODE_ACK 0
#define MODE_NAK 1

#define END_SENDER 0
#define END_RECEIVER 1

/* possible events: */
#define TIMER_INTERRUPT 0
#define FROM_LAYER5 1
#define FROM_LAYER3 2

#define OFF 0
#define ON 1
#define A 0
#define B 1

struct msg {
    char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
    int seqnum;
    int acknum;
    int checksum;
    char payload[20];
};

struct buffer {
    struct pkt *pkt;
};

// network control block
struct tx {
	struct buffer buffers[SIZE_TX_BUFFER_GBN];
	int head;
	int tail;
	int base;
	int nextseg;
	int cwnd;
};

struct rx {
	struct buffer buffers[SIZE_RX_BUFFER];
	int head;
	int tail;
	int success;
};

struct conn_ncb {
	int host;
	int conn;
	int seqnum;
	int acknum;
	float increment;
	struct tx *tx_ncb;
	struct rx *rx_ncb;
};

struct event *evlist;   /* the event list */

struct conn_ncb *A_conn_ncb;
struct conn_ncb *B_conn_ncb;
