/*
 * dz.h: Serial port driver for DECStations equiped 
 *       with the DZ chipset.
 *
 * Copyright (C) 1998 Olivier A. D. Lebaillif 
 *             
 * Email: olivier.lebaillif@ifrsys.com
 *
 */
#ifndef DZ_SERIAL_H
#define DZ_SERIAL_H

/*
 * Definitions for the Control and Status Received.
 */
#define DZ_TRDY        0x8000                 /* Transmitter empty */
#define DZ_TIE         0x4000                 /* Transmitter Interrupt Enable */
#define DZ_RDONE       0x0080                 /* Receiver data ready */
#define DZ_RIE         0x0040                 /* Receive Interrupt Enable */
#define DZ_MSE         0x0020                 /* Master Scan Enable */
#define DZ_CLR         0x0010                 /* Master reset */
#define DZ_MAINT       0x0008                 /* Loop Back Mode */

/*
 * Definitions for the Received buffer. 
 */
#define DZ_RBUF_MASK   0x00FF                 /* Data Mask in the Receive Buffer */
#define DZ_LINE_MASK   0x0300                 /* Line Mask in the Receive Buffer */
#define DZ_DVAL        0x8000                 /* Valid Data indicator */
#define DZ_OERR        0x4000                 /* Overrun error indicator */
#define DZ_FERR        0x2000                 /* Frame error indicator */
#define DZ_PERR        0x1000                 /* Parity error indicator */

#define LINE(x) (x & DZ_LINE_MASK) >> 8       /* Get the line number from the input buffer */
#define UCHAR(x) (unsigned char)(x & DZ_RBUF_MASK)

/*
 * Definitions for the Transmit Register.
 */
#define DZ_LINE_KEYBOARD 0x0001
#define DZ_LINE_MOUSE    0x0002
#define DZ_LINE_MODEM    0x0004
#define DZ_LINE_PRINTER  0x0008

#define DZ_MODEM_DTR     0x0400               /* DTR for the modem line (2) */

/*
 * Definitions for the Modem Status Register.
 */
#define DZ_MODEM_DSR     0x0200               /* DSR for the modem line (2) */

/*
 * Definitions for the Transmit Data Register.
 */
#define DZ_BRK0          0x0100               /* Break assertion for line 0 */
#define DZ_BRK1          0x0200               /* Break assertion for line 1 */
#define DZ_BRK2          0x0400               /* Break assertion for line 2 */
#define DZ_BRK3          0x0800               /* Break assertion for line 3 */

/*
 * Definitions for the Line Parameter Register.
 */
#define DZ_KEYBOARD      0x0000               /* line 0 = keyboard */
#define DZ_MOUSE         0x0001               /* line 1 = mouse */
#define DZ_MODEM         0x0002               /* line 2 = modem */
#define DZ_PRINTER       0x0003               /* line 3 = printer */

#define DZ_CSIZE         0x0018               /* Number of bits per byte (mask) */
#define DZ_CS5           0x0000               /* 5 bits per byte */
#define DZ_CS6           0x0008               /* 6 bits per byte */
#define DZ_CS7           0x0010               /* 7 bits per byte */
#define DZ_CS8           0x0018               /* 8 bits per byte */

#define DZ_CSTOPB        0x0020               /* 2 stop bits instead of one */ 

#define DZ_PARENB        0x0040               /* Parity enable */
#define DZ_PARODD        0x0080               /* Odd parity instead of even */

#define DZ_CBAUD         0x0E00               /* Baud Rate (mask) */
#define DZ_B50           0x0000
#define DZ_B75           0x0100
#define DZ_B110          0x0200
#define DZ_B134          0x0300
#define DZ_B150          0x0400
#define DZ_B300          0x0500
#define DZ_B600          0x0600
#define DZ_B1200         0x0700 
#define DZ_B1800         0x0800
#define DZ_B2000         0x0900
#define DZ_B2400         0x0A00
#define DZ_B3600         0x0B00
#define DZ_B4800         0x0C00
#define DZ_B7200         0x0D00
#define DZ_B9600         0x0E00

#define DZ_CREAD         0x1000               /* Enable receiver */
#define DZ_RXENAB        0x1000               /* enable receive char */
/*
 * Addresses for the DZ registers
 */
#define DZ_CSR       0x00            /* Control and Status Register */
#define DZ_RBUF      0x08            /* Receive Buffer */
#define DZ_LPR       0x08            /* Line Parameters Register */
#define DZ_TCR       0x10            /* Transmitter Control Register */
#define DZ_MSR       0x18            /* Modem Status Register */
#define DZ_TDR       0x18            /* Transmit Data Register */

#define DZ_NB_PORT 4

#define DZ_XMIT_SIZE   4096                 /* buffer size */
#define DZ_WAKEUP_CHARS   DZ_XMIT_SIZE/4

#ifdef MODULE
int init_module (void)
void cleanup_module (void)
#endif

#endif /* DZ_SERIAL_H */
