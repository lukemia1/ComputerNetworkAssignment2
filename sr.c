#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT  15.0       /* round trip time.  MUST BE SET TO 15.0 when submitting assignment */
#define WINDOWSIZE 6    /* Maximum number of buffered unacked packet */
#define SEQSPACE 12      /* min sequence space for SR must be at least windowsize * 2  */
#define NOTINUSE (-1)   /* used to fill header fields that are not being used */

/* generic procedure to compute the checksum of a packet.  Used by both sender and receiver  
   the simulator will overwrite part of your packet with 'z's.  It will not overwrite your 
   original checksum.  This procedure must generate a different checksum to the original if
   the packet is corrupted.
*/
int ComputeChecksum(struct pkt packet)
{
  int checksum = 0;
  int i;

  checksum = packet.seqnum;
  checksum += packet.acknum;
  for ( i=0; i<20; i++ ) 
    checksum += (int)(packet.payload[i]);

  return checksum;
}

bool IsCorrupted(struct pkt packet)
{
  if (packet.checksum == ComputeChecksum(packet))
    return (false);
  else
    return (true);
}


/********* Sender (A) variables and functions ************/

static struct pkt buffer[WINDOWSIZE];  /* array for storing packets waiting for ACK */
static int windowfirst, windowlast;    /* array indexes of the first/last packet awaiting ACK */
static int windowcount;                /* the number of packets currently awaiting an ACK */
static int A_nextseqnum;               /* the next sequence number to be used by the sender */

static int reserve_ackcount;           /* remember how many pkt after the first awaiting pkt have received ACK */
static int last_ACK;                   /* remember the last pkt received ACK */

/* called from layer 5 (application layer), passed the message to be sent to other side */
void A_output(struct msg message)
{
  struct pkt sendpkt;
  int i;

  /* if not blocked waiting on ACK */
  if ( windowcount < WINDOWSIZE) {
    if (TRACE > 1)
      printf("----A: New message arrives, send window is not full, send new messge to layer3!\n");

    /* create packet */
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for ( i=0; i<20 ; i++ ) 
      sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt); 

    /* put packet in window buffer */
    /* windowlast will always be 0 for alternating bit; but not for GoBackN */
    windowlast = (windowlast + 1) % WINDOWSIZE; 
    buffer[windowlast] = sendpkt;
    windowcount++;

    /* send out packet */
    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3 (A, sendpkt);

    /* start timer if first packet in window */
    if (windowcount == 1) 
      starttimer(A,RTT);

    /* get next sequence number, wrap back to 0 */
    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;  
  }
  /* if blocked,  window is full */
  else {
    if (TRACE > 0)
      printf("----A: New message arrives, send window is full\n");
    window_full++;
  }
}

/* called from layer 3, when a packet arrives for layer 4 
   In this practical this will always be an ACK as B never sends data.
*/
void A_input(struct pkt packet)
{
  int ackcount = 0;
  int i;

  /* if received ACK is not corrupted */ 
  if (!IsCorrupted(packet)) {
    if (TRACE > 0)
      printf("----A: uncorrupted ACK %d is received\n",packet.acknum);
    total_ACKs_received++;
  
    /* check if new ACK or duplicate */
    if (windowcount != 0 && packet.acknum != last_ACK) {
          int seqfirst = buffer[windowfirst].seqnum;
          int seqlast = buffer[windowlast].seqnum;
          /* check case when seqnum has and hasn't wrapped */
          if (((seqfirst <= seqlast) && (packet.acknum >= seqfirst && packet.acknum <= seqlast)) ||
              ((seqfirst > seqlast) && (packet.acknum >= seqfirst || packet.acknum <= seqlast))) {

            /* packet is a new ACK */
            if (TRACE > 0)
              printf("----A: ACK %d is not a duplicate\n",packet.acknum);
           
            new_ACKs++;

            last_ACK = packet.acknum;
            
            /* if the received ACK is not for the first awaiting pkt in window*/
            if (packet.acknum != seqfirst)
            {           
              /* else, change pkt seqnum to -1 to marked as received */
              for (i = 1; i < windowcount; i++)
              { 
                if (buffer[(windowfirst+i) % WINDOWSIZE].seqnum == packet.acknum)
                {
                  buffer[(windowfirst+i) % WINDOWSIZE].seqnum = -1;
                }
              }     
            }

            /* if packet ack equal to the seq num of the first packet of sender window */
            /* windowfirst only increment by 1 when first waiting pkt receives ACK */
            if (packet.acknum == seqfirst)
            { 
              ackcount = packet.acknum + 1 - seqfirst;
              
              /* check the following pkt */         
              for (i = 1; i < windowcount; i++)
              {
                if ((buffer[(windowfirst+i) % WINDOWSIZE]).seqnum == -1)
                {
                  reserve_ackcount += 1;
                } else {
                  break;
                }
              }
              
              /* check if any pkt after the 1st pkt (pkt no. must be in order) received ACK already */
              if (reserve_ackcount != 0)
              {
                /* if any slide the window in number of 1 (windowfirst) + pkt after windowfirstreceived ACK */
                ackcount = ackcount + reserve_ackcount;
                reserve_ackcount = 0;
              }
              
            }

	          /* slide window by the number of packets ACKed */
            windowfirst = (windowfirst + ackcount) % WINDOWSIZE;

            /* delete the acked packets from window buffer */
            for (i=0; i<ackcount; i++)
              windowcount--;

            
	          /* stop and start timer if the first unacked packets in window receive ACK */
            if (packet.acknum == seqfirst)
            {
              stoptimer(A);
              if (windowcount > 0)
              {
                starttimer(A, RTT+1);
              }           
            }        
            
          }
        }
        else 
        {
            if (TRACE > 0)
          printf ("----A: duplicate ACK received, do nothing!\n");
        }
  }
  else 
    if (TRACE > 0)
      printf ("----A: corrupted ACK is received, do nothing!\n");
}

/* called when A's timer goes off */
void A_timerinterrupt(void)
{
  if (TRACE > 0)
    printf("----A: time out,resend packets!\n");

  if ((buffer[windowfirst]).seqnum != -1)
  {
    if (TRACE > 0)
      printf ("---A: resending packet %d\n", (buffer[windowfirst]).seqnum);

    /* only rsend the windowfirst pkt */
    tolayer3(A,buffer[windowfirst]);
    packets_resent++;
    starttimer(A,RTT+1);
  }
    
}
     



/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init(void)
{
  /* initialise A's window, buffer and sequence number */
  A_nextseqnum = 0;  /* A starts with seq num 0, do not change this */
  windowfirst = 0;
  windowlast = -1;   /* windowlast is where the last packet sent is stored.  
		     new packets are placed in winlast + 1 
		     so initially this is set to -1
		   */
  windowcount = 0;
  reserve_ackcount = 0;
  last_ACK = -1;
}



/********* Receiver (B)  variables and procedures ************/

static int first_round;
static int rev_buffer[SEQSPACE];  /* array for storing packets sent to application */
static int rev_first;   /*  array indexes of the first received data */
static int B_nextseqnum;    /* the sequence number for the next packets sent by B */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(struct pkt packet)
{
  struct pkt sendpkt;
  int i;

  /* initialize rev_buffer in the start */
  if (first_round == 0)
  {
    for (i = 0; i < SEQSPACE; i++)
    {
      rev_buffer[i] = -1;
    }
    first_round = 1;
  }
  
  /* if not corrupted and received packet is in order */
  if  ((!IsCorrupted(packet))) {
    /* is 0 when it hasn't sent to application */
    int data_existed = 0;

    if (TRACE > 0)
      printf("----B: packet %d is correctly received, send ACK!\n",packet.seqnum);

    packets_received++;

    for (i = 0; i < SEQSPACE; i++)
    {
      if (rev_buffer[i] == packet.seqnum)
      { 
        /* change to 1 if sent already */
        data_existed = 1;
        break;
      }
    }
    
    if (data_existed == 0)
    { 
      /* remember the state of received data */
      rev_buffer[rev_first] = packet.seqnum;
      if (rev_first < SEQSPACE)
      {
        rev_first+=1;
      }

      /* deliver to receiving application */
      tolayer5(B, packet.payload);
    }
    
    /* send an ACK for the received packet */
    sendpkt.acknum = packet.seqnum;

    /* create packet */
    sendpkt.seqnum = B_nextseqnum;
    B_nextseqnum = (B_nextseqnum + 1) % 2;
      
    /* we don't have any data to send.  fill payload with 0's */
    for ( i=0; i<20 ; i++ ) 
      sendpkt.payload[i] = '0';  

    /* computer checksum */
    sendpkt.checksum = ComputeChecksum(sendpkt); 

    /* send out packet */
    tolayer3 (B, sendpkt);
  }
}
  /* else { */
  /* packet is corrupted do nothing */
  /* if (TRACE > 0) */
  /* printf("----B: packet corrupted or not expected sequence number, resend ACK!\n"); */
  


/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init(void)
{
  rev_first = 0;
  B_nextseqnum = 1;
  first_round = 0;
}

/******************************************************************************
 * The following functions need be completed only for bi-directional messages *
 *****************************************************************************/

/* Note that with simplex transfer from a-to-B, there is no B_output() */
void B_output(struct msg message)  
{
}

/* called when B's timer goes off */
void B_timerinterrupt(void)
{
}


