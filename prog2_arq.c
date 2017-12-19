#include <stdio.h>
#include <stdlib.h>

#include <prog2.h>

int compute_checksum(struct pkt packet)
{
	int i;
	int checksum;

	checksum = 0;
	checksum += packet.seqnum;
	checksum += packet.acknum;

	for (i = 0; i < 20; i++) {
		checksum += (int) packet.payload[i];
	}

	return checksum;
}

int compare_checksum(struct pkt packet)
{
	if (packet.checksum == compute_checksum(packet)) return CHKSUM_CORRECT;
	return CHKSUM_WRONG;
}

struct pkt *make_pkt(struct msg *message, struct conn_ncb *ncb)
{
	struct pkt *pkt;

	pkt = malloc(sizeof(struct pkt));

	pkt->seqnum = -1;
	pkt->acknum = -1;
	pkt->checksum = -1;

	int i;
	for (i = 0; i < 20; i++) pkt->payload[i] = message->data[i];

	return pkt;
}

int put_pkt_in_buffer(struct pkt *pkt, struct conn_ncb *ncb)
{
	int head = ncb->tx_ncb->head;
	int tail = ncb->tx_ncb->tail;

	if ((head + 1) % SIZE_TX_BUFFER_ARQ == tail) {
		printf("**** Buffer full, so DROP messages coming from Layer 5.\n");

		return PKT_DROP;
	}

	ncb->tx_ncb->buffers[head].pkt = pkt;
	ncb->tx_ncb->head += 1;
	ncb->tx_ncb->head %= SIZE_TX_BUFFER_ARQ;

	printf("--> Put packet received from Layer 5 in buffer.\n");
	return PKT_SAVE;
}

void send_pkt(struct conn_ncb *ncb, int mode)
{
	int head = ncb->tx_ncb->head;
	int tail = ncb->tx_ncb->tail;
	int base = ncb->tx_ncb->base;
	int nextseg = ncb->tx_ncb->nextseg;

	if (head == tail) {
		printf("**** Buffer is empty. NO messages to send to Layer 3.\n");
		return;
	}

	if (nextseg - base >= ncb->tx_ncb->cwnd) {
		printf("**** cwnd = %d is full. NO messages sending to Layer 3.\n", ncb->tx_ncb->cwnd);
		return;
	}

	struct pkt *pkt;
	pkt = ncb->tx_ncb->buffers[nextseg].pkt;

	//if (ncb->seqnum == -1 && ncb->acknum == -1) {
	if (ncb->conn == -1) {
		ncb->conn = END_SENDER;

		ncb->seqnum = 0;
		ncb->acknum = 0;
	}

	if (mode != MODE_RESEND) {
		pkt->seqnum = ncb->seqnum;
		pkt->acknum = ncb->acknum; //-1;//ncb->acknum;
		pkt->checksum = compute_checksum(*pkt);
		
		ncb->seqnum += 1;
		ncb->seqnum %= MAX_SEQ_ACK;

	}

	if (base == nextseg) 
		starttimer(0, ncb->increment);
	
	ncb->tx_ncb->nextseg += 1;
	ncb->tx_ncb->nextseg %= SIZE_TX_BUFFER_ARQ;

	printf("--> Send packet with: seqnum: %d; acknum: %d; checksum: %d\n", pkt->seqnum, pkt->acknum, pkt->checksum);

	tolayer3(0, *pkt);
}

void A_output(message)
    struct msg message;
{
	if (A_conn_ncb->host == A) printf("[A] ");
	else printf("[B] ");
	printf("--------------------------------------------------\n");
	struct pkt *pkt;
	
	pkt = make_pkt(&message, A_conn_ncb);
	if (put_pkt_in_buffer(pkt, A_conn_ncb) != PKT_DROP) send_pkt(A_conn_ncb, MODE_SEND);
	printf("==================================================\n");
}

void recv_pkt(struct pkt *pkt, struct conn_ncb *ncb) {
	printf("--> Receive packet with: seqnum: %d; acknum: %d; checksum: %d\n", pkt->seqnum, pkt->acknum, pkt->checksum);
	printf("--> Compute checksum: %d\n", compute_checksum(*pkt));

	//if (pkt->seqnum == -1 && pkt->acknum != -1) {
	if (ncb->conn == END_SENDER) {
		if (compare_checksum(*pkt) ==  CHKSUM_WRONG || pkt->acknum == -2) {
			printf("--> Receive CORRUPTED packet\n");
		} else if (pkt->acknum == -2) {
			printf("--> Receive NAK\n");
			printf("--> Resend packet:\n");
			ncb->tx_ncb->nextseg = ncb->tx_ncb->base;	
			if (ncb->host == A) stoptimer(0);
			else stoptimer(1);
			send_pkt(ncb, MODE_RESEND);
		} else {
			printf("--> Receive correct packet\n");
			ncb->tx_ncb->tail += 1;
			ncb->tx_ncb->tail %= SIZE_TX_BUFFER_ARQ;
			ncb->tx_ncb->base += 1;
			ncb->tx_ncb->base %= SIZE_TX_BUFFER_ARQ;

			if (ncb->host == A) stoptimer(0);
			else stoptimer(1);
			if (ncb->tx_ncb->base != ncb->tx_ncb->nextseg) {
				if (ncb->host == A) starttimer(0, ncb->increment);
				else starttimer(1, ncb->increment);
			}
		}
	//} else if (pkt->seqnum != -1 && pkt->acknum == -1) {
	} else if (ncb->conn == END_RECEIVER) {
		if (compare_checksum(*pkt) ==  CHKSUM_WRONG) {
			printf("--> Receive CORRUPTED packet\n");
			printf("--> Send NAK\n");
			struct msg *nak_msg;
			struct pkt *nak_pkt;
			nak_msg = malloc(sizeof(struct msg));
			int i;
			for (i = 0; i < 20; i++) nak_msg->data[i] = 0;
		
			nak_pkt = make_pkt(nak_msg, ncb);
			nak_pkt->seqnum = ncb->seqnum;
			nak_pkt->acknum = -2;
			nak_pkt->checksum = compute_checksum(*nak_pkt);

			printf("--> Send packet with: seqnum: %d; acknum: %d; checksum: %d\n", nak_pkt->seqnum, nak_pkt->acknum, nak_pkt->checksum);
			
			if (ncb->host == A) tolayer3(0, *nak_pkt);
			else tolayer3(1, *nak_pkt);
		} else {
			printf("--> Receive correct packet\n");
			printf("--> Send ACK\n");
			if (ncb->acknum != pkt->seqnum) {
				ncb->rx_ncb->success++;
			}
			struct msg *ack_msg;
			struct pkt *ack_pkt;
			ack_msg = malloc(sizeof(struct msg));
			int i;
			for (i = 0; i < 20; i++) ack_msg->data[i] = 0;
		
			ack_pkt = make_pkt(ack_msg, ncb);
			ack_pkt->seqnum = ncb->seqnum;
			ack_pkt->acknum = pkt->seqnum;
			ncb->acknum = pkt->seqnum;
			ack_pkt->checksum = compute_checksum(*ack_pkt);
			
			printf("--> Send packet with: seqnum: %d; acknum: %d; checksum: %d\n", ack_pkt->seqnum, ack_pkt->acknum, ack_pkt->checksum);

			if (ncb->host == A) tolayer3(0, *ack_pkt);
			else tolayer3(1, *ack_pkt);
		}
	} else {
		printf("!!!Error in recv_pkt.\n\n");
	}
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
    struct pkt packet;
{
	printf("[A] ");
	printf("--------------------------------------------------\n");
	if (A_conn_ncb->conn == -1) A_conn_ncb->conn = END_RECEIVER;
	recv_pkt(&packet, A_conn_ncb);
	/*if (A_conn_ncb->acknum != packet->seqnum + 20) {	
	}*/
	printf("==================================================\n");
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
	printf("[A] ");
	printf("--------------------------------------------------\n");
	printf("--> Timeout\n");
	//stoptimer(0);
	if (A_conn_ncb->tx_ncb->base != A_conn_ncb->tx_ncb->nextseg) {
		A_conn_ncb->tx_ncb->nextseg = A_conn_ncb->tx_ncb->base;
		send_pkt(A_conn_ncb, MODE_RESEND);
	}
	printf("==================================================\n");
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
struct conn_ncb *init_ncb()
{
	struct conn_ncb *ncb = malloc(sizeof(struct conn_ncb));
	ncb->conn = -1;
	ncb->seqnum = 0; //-1;
	ncb->acknum = -1;

	ncb->increment = 15.0;

	ncb->tx_ncb = malloc(sizeof(struct tx));
	ncb->tx_ncb->head = 0;
	ncb->tx_ncb->tail = 0;
	ncb->tx_ncb->base = 0;
	ncb->tx_ncb->nextseg = 0;

	ncb->rx_ncb = malloc(sizeof(struct rx));
	ncb->rx_ncb->head = 0;
	ncb->rx_ncb->tail = 0;
	ncb->rx_ncb->success = 0;

	return ncb;
}

void A_init()
{
	A_conn_ncb = init_ncb();
	A_conn_ncb->host = A;

	// For Alternating Bit Protocol
	A_conn_ncb->tx_ncb->cwnd = 1;
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(message)  /* need be completed only for extra credit */
    struct msg message;
{
	printf("[B] ");
	printf("--------------------------------------------------\n");
	struct pkt *pkt;
	
	pkt = make_pkt(&message, B_conn_ncb);
	if (put_pkt_in_buffer(pkt, B_conn_ncb) != PKT_DROP) send_pkt(B_conn_ncb, MODE_SEND);
	printf("==================================================\n");
}


/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
    struct pkt packet;
{
	printf("[B] ");
	printf("--------------------------------------------------\n");
	if (B_conn_ncb->conn == -1) B_conn_ncb->conn = END_RECEIVER;
	recv_pkt(&packet, B_conn_ncb);
	/*if (A_conn_ncb->acknum != packet->seqnum + 20) {	
	}*/
	printf("==================================================\n");
}

/* called when B's timer goes off */
void B_timerinterrupt()
{
	printf("[B] ");
	printf("--------------------------------------------------\n");
	printf("--> Timeout\n");
	//stoptimer(1);
	if (B_conn_ncb->tx_ncb->base != B_conn_ncb->tx_ncb->nextseg) {
		B_conn_ncb->tx_ncb->nextseg = B_conn_ncb->tx_ncb->base;
		send_pkt(B_conn_ncb, MODE_RESEND);
	}
	printf("==================================================\n");
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
	B_conn_ncb = init_ncb();
	B_conn_ncb->host = B;

	// For Alternating Bit Protocol
	B_conn_ncb->tx_ncb->cwnd = 1;
}
