/*
 *	Update the firmware in an Ecowitt weather gateway/console using
 *	the binary "telnet" interface.
 *
 *	This code is based upon the "telnet" interface document by
 *	Fine Offset Electronics - document serial number "FOS-ENG-022-A" -
 *		  initial version 1.6.0, dated 2021-01-05
 *		revised - version 1.6.1, dated 2021-08-17
 *	        revised - version 1.6.2, dated 2021-11-04
 *		revised - version 1.6.3, dated 2021-11-04
 *		revised - version 1.6.4, dated 2022-03-05
 *		revised - version 1.6.5, dated 2022-04-19
 *		revised - version 1.6.6, dated 2022-04-20
 *		revised - version 1.6.7, dated 2022-06-17
 *		revised - version 1.6.8, dated 2023-02-27
 *		revised - version 1.6.9, dated 2024-01-15
 *
 *	Jonathan Broome
 *	jbroome@wao.com
 *	June 2024
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>	// for ether_ntoa() and struct ether_addr

// We need this for Linux to get the function prototypes:
#ifdef __linux__
# include <netinet/ether.h>	// Linux
#endif

#include <sys/time.h>
#include <poll.h>
#include <netdb.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>

// Commands that we need to know - we only use a very few:
typedef enum {
	CMD_READ_SATION_MAC = 0x26,	// read MAC address (sic - missing the first 'T')
	CMD_WRITE_UPDATE = 0x43,	// firmware upgrade
	CMD_READ_FIRMWARE_VERSION = 0x50 // read current firmware version number
} CMD_LT;

typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned long	ulong;


/* global variables */
char	*progname;
int	debug = 0;
int	verbose = 0;
int	update = 0;


/* prototypes */
int	build_command_packet (uchar command, uchar *data, int datalen, uchar *packet);
int	receive_reply_packet (int fd, uchar *packet, int maxlen);

int	interpret_read_station_mac (uchar command, uchar *ptr, int length);
int	interpret_read_firmware_version (uchar command, uchar *ptr, int length);
int	interpret_reply_packet (uchar expectedcommand, uchar *packet, int length);

int	read_station_mac (int sock);
int	read_firmware_version (int sock);
int	write_update (int sock, struct in_addr *addr, int port);
int	do_firmware_service (int sock, int fd_user1, int fd_user2);

int	open_socket (char *host, char *service);
int	do_getsockname (int s, struct sockaddr_in *addrptr);
void	hexdump (uchar *data, int length);

int	safe_write (int fd, uchar *bufp, int len);
int	timed_read (int fd, char *buf, int len, int timeout);
int	read_until_null (int fd, char *buf, int bufsiz);

void	usage (void);
int	main (int argc, char **argv);


/*
 *	Build up a command packet, with the header, command byte, length, any
 *	parameters or data, and the checksum.
 *
 *	Returns the total packet length, including the header.
 */
int
build_command_packet (uchar command, uchar *data, int datalen, uchar *packet)
{
	uchar	*pptr;
	uchar	c;
	uchar	checksum;
	uchar	size;

	pptr = packet;

	/* the first two bytes are always FF - header -
	 * and don't count in checksum or length.
	 */
	*pptr++ = 0xff;
	*pptr++ = 0xff;

	/* next byte is always the command byte, and is included in
	 * both the checksum and the length.
	 */
	*pptr++ = command;
	checksum = command;

	/* NOTE: some commands use TWO bytes of size. We're not using those
	 * commands here though.
	 */
	/* next byte should be the size - we know that it's 2 + datalen + 1
	 * (command + size + data + checksum)
	 */
	size = 1 + 1 + datalen + 1;		// total packet size with one byte of size.
	*pptr++ = size;	/* store the size */
	checksum = (checksum & 0xff) + size;	/* and add it to checksum */

	/* now include the data, if any, updating the checksum on the way */
	while (datalen-- > 0) {
		c = *data++;
		*pptr++ = c;
		checksum = (checksum & 0xff) + c;	/* and add it to checksum */
	}

	/* finally, append the checksum, which is counted in the length */
	*pptr++ = checksum;

	return (int)(pptr - packet);		// total number of bytes in the packet
}

//XXX this function could use some reworking -- need to verify that
//XXX the data received is what we expect, and timeouts are obeyed.
int
receive_reply_packet (int fd, uchar *packet, int maxlen)
{
	uchar	*pptr = packet;
	uchar	c = '\0';
	int	total_len = 0;
	int	r;
	int	size;
	int	remain;

	/* first we expect to read the two header bytes - should be FF FF */

	/* okay to block for the first header byte */
	while ((r = read (fd, &c, 1)) >= 0 && maxlen > 0) {
		*pptr++ = c;
		maxlen--;
		total_len++;
		if (c == 0xff)	// this is what we want
			break;
	}
	if (r < 0) {		/* an error occurred */
		fprintf (stderr, "%s: socket read error: %s\n",
			__FUNCTION__, strerror (errno));
		return -1;
	}
	/* make sure it's the FF */
	if (c != 0xff) {
		fprintf (stderr, "%s: first byte isn't 0xff - 0x%02x\n",
			__FUNCTION__, ((unsigned int)c) & 0x00ff);
		return -1;
	}

	/* Now that we have the first FF byte, read the next three bytes - they
	 * should be the second FF, the command byte, and the size.
	 */
	while (total_len < 4) {
		if ((r = timed_read (fd, (char *)pptr, (4 - total_len), 1)) < 0) {
			// the read timed out
			fprintf (stderr, "%s: timeout reading header bytes (%d).\n",
				__FUNCTION__, (4 - total_len));
			return -1;
		}
		pptr += r;
		total_len += r;
	}

	//command = packet[2];		/* third byte is the command */

	// NOTE that certain commands use two bytes for the size field, but
	// we're not using those commands here.
	/* the expected size is in packet[3] -- this size includes the
	 * command byte, size byte, data byte(s), and checksum byte.
	 */
	size = packet[3];
	size &= 0x00ff;
	if (size > (maxlen-2)) {	// sanity check
		fprintf (stderr, "%s: size in reply packet is too large for buffer (%d vs %d)\n",
			__FUNCTION__, size, maxlen);
		return -1;
	}

	/* we need to get size-2 bytes (we already have command and size) */
	remain = size - 2;

	/* now read the remaining bytes, with a timeout */
	while (remain > 0) {
		r = timed_read (fd, (char *)pptr, maxlen, 1);
		if (r < 0) {
			fprintf (stderr, "%s: timeout reading the rest.\n",
				__FUNCTION__);
			return -1;
		} else if (r == 0) {
			fprintf (stderr, "%s: connection closed by remote.\n",
				__FUNCTION__);
			return -1;
		}
		pptr += r;
		remain -= r;
	}

	return (int)(pptr - packet);		// total number of bytes in the packet
}


/*
 *	Macros for pulling various-sized entities from the data buffer.
 *
 *	These are simple and brute-force, but don't have any alignment
 *	or byte-order issues...
 */
#define	GET_BYTE(VAR,PTR,LEN)		{	\
				VAR = (*(PTR++));	\
				(LEN) --;		\
					}

#define	GET_BUFFER(VAR,N,PTR,LEN)	{	\
				uchar *_p = (uchar *)(VAR);	\
				for (int _remain = (N); _remain > 0; _remain--) {	\
					*_p++ = (*(PTR++));	\
					(LEN) --;		\
				}				\
					}


//------------------------------------------------------------------------------
/* Interpret the reply to CMD_READ_SATION_MAC (sic)..
 *
 * We expect to see this field here:
 *	Sta_mac[6]		6	sta_mac[0];sta_mac[1];sta_mac[2]; sta_mac[3];sta_mac[4];sta_mac[5];
 *
 * Returns the number of bytes consumed, so the caller can be sure that
 * everything was handled.
 */
int
interpret_read_station_mac (uchar command, uchar *ptr, int length)
{
	int	original_length = length;
	uchar	macaddress[6];

	/* the MAC address is six bytes */
	GET_BUFFER (macaddress, 6, ptr, length);

	printf ("MAC Address [%02x:%02x:%02x:%02x:%02x:%02x]\n",
			macaddress[0], macaddress[1], macaddress[2],
			macaddress[3], macaddress[4], macaddress[5]);

	return (original_length - length);
}

//------------------------------------------------------------------------------
/* Interpret the reply to CMD_READ_FIRMWARE_VERSION.
 *
 * We expect to see these fields here:
 *	Version length			1	Max value 23Bytes
 *	Version buffer				For example: "EasyWeatherV1.2.0"
 *
 * Returns the number of bytes consumed, so the caller can be sure that
 * everything was handled.
 */
int
interpret_read_firmware_version (uchar command, uchar *ptr, int length)
{
	int	original_length = length;
	int	version_length;
	uchar	version_buffer[256];

	/* get the length of the version string */
	GET_BYTE (version_length, ptr, length);

	/* now collect that many bytes into version_buffer[] */
	GET_BUFFER (version_buffer, version_length, ptr, length);

	version_buffer[version_length] = '\0';	// null-terminate

	printf ("Firmware Version [%s]\n", version_buffer);

	return (original_length - length);
}

/*------------------------------------------------------------------------------
 *	This is the first code to handle a reply packet.
 *	- It knows that byte 0 and 1 are both 0xff.
 *	- Byte 2 is the command that this is a reply to.
 *	- Byte 3 [and possibly byte 4] is/are the size, including the command
 *	  and size bytes.  (It knows which commands use two-byte sizes.)
 *	- bytes 4 or 5 to N-1 are the payload
 *	- Byte N is the checksum.
 */
int
interpret_reply_packet (uchar expectedcommand, uchar *packet, int length)
{
	int	orig_length = length;
	int	r;
	int	i;
	uchar	*ptr;
	uchar	command;
	int	size;
	uchar	cksum;
	int	status;

	ptr = packet;

	if (debug) {	// dump out the raw data
		printf ("%s: reply is %d bytes:\n", __FUNCTION__, length);
		hexdump (packet, length);
	}

	// verify the first two bytes are ff ff:
	if (packet[0] != 0xff || packet[1] != 0xff) {
		fprintf (stderr,
			"%s: first two bytes are not ff ff: %02x %02x\n",
			__FUNCTION__, packet[0], packet[1]);
		return -1;
	}

	// verify the checksum - this is the 8-bit sum of bytes [2 to len-2],
	// aka from "command" to "last of data", not including the checksum
	cksum = packet[length-1];	// transmitted checksum is the last byte
	int computed = 0;
	int c;
	for (i = 2; i < length-1; i++) {
		c = packet[i];
		computed = (computed + c) & 0x00ff;
	}
	if (computed != cksum) {
		printf ("checksum error: specified %x, computed %x.\n", cksum, computed);
	} else {
		if (debug)
			printf ("checksum OKAY: specified %x, computed %x.\n", cksum, computed);
	}

	/* first two bytes should always be ff - we can skip over them */
	ptr += 2;	/// skip ff ff
	length -= 2;

	/* third byte is the command */
	command = *ptr++;
	length--;

	// see if the indicated command is the what we expected -- if not, complain:
	if (expectedcommand != command) {
		printf ("Received command 0x%02x (data length=%d) in response to 0x%02x\n",
			command, length, expectedcommand);
	}

	// NOTE that several commands use two bytes to specify the "size" value
	// in their replies.  We are not using any of those commands here.
	/* fourth byte is size */
	size = (unsigned int) (*ptr++);
	length--;

	// take off one from the length for the checksum byte at the end.
	length--;

	/* now handle the data bytes - which depends on the command */

	// read the gateway's MAC address
	if (command == CMD_READ_SATION_MAC) {
		r = interpret_read_station_mac (command, ptr, length);
		if (r > 0) {	// adjust length and data pointer
			length -= r;
			ptr += r;
		}
	} else

	// read the gateway's firmware version
	if (command == CMD_READ_FIRMWARE_VERSION) {
		r = interpret_read_firmware_version (command, ptr, length);
		if (r > 0) {	// adjust length and data pointer
			length -= r;
			ptr += r;
		}
	} else

	// write (firmware) update
	if (command == CMD_WRITE_UPDATE) {
		/* get the status - first data byte */
		GET_BYTE (status, ptr, length);
		if (debug) {
			printf ("command CMD_WRITE_UPDATE: status = 0x%x\n", status);
		}
		if (status == 0)	// 0 = success
			return 0;
		return -1;		// else return failure
	} else

	{
		// unknown command - perhaps we sent something the firmware
		// doesn't recognise, or at least we don't recognise it here.

		/* assume it's okay to get the status - first data byte */
		int status;
		GET_BYTE (status, ptr, length);

		printf ("%s: UNHANDLED CASE: command=0x%x - length is %d\n",
			__FUNCTION__, command, orig_length);
		printf ("status = 0x%x, size = %d\n", status, size);

		// dump out the raw data
		printf ("%s: Dump of raw data - length %d bytes:\n", __FUNCTION__, orig_length);
		hexdump (packet, orig_length);
		printf ("\n");

		abort();
	}

	// Make sure we consumed all bytes of the response - if not, we parsed it wrong!
	if (length != 0) {
		printf ("\n*** LENGTH IS %d - should be 0 (command 0x%02x). ***\n", length, command);
		abort();
	}

	return 0;
}

//------------------------------------------------
// CMD_READ_SATION_MAC(sic) takes no data:
//		Read MAC
// Send:
//	Fixed header		2	0xffff
//	CMD_READ_SATION_MAC	1	0x26
//	Size			1
//	Checksum		1	checksum
// Receive:
//	Fixed header		2	0xffff
//	CMD_READ_SATION_MAC	1	0x26
//	Size			1	Packet size
//	Sta_mac[6]		6	sta_mac[0];sta_mac[1];sta_mac[2];sta_mac[3];sta_mac[4];sta_mac[5];
//	Checksum		1	checksum
int
read_station_mac (int sock)
{
	int	packetlen;
	uchar	commandpacket[1024];
	int	r;
	uchar	response[1024];

	packetlen = build_command_packet (CMD_READ_SATION_MAC, NULL, 0, commandpacket);
	if ((r = safe_write (sock, commandpacket, packetlen)) != packetlen)
		return r;
	if ((r = receive_reply_packet (sock, response, sizeof response)) < 0)
		return r;
	r = interpret_reply_packet (CMD_READ_SATION_MAC, response, r);
	return r;
}

//------------------------------------------------
// CMD_READ_FIRMWARE_VERSION takes no data:
//		
// Send:
//	Fixed header			2	0xffff
//	CMD_READ_FIRMWARE_VERSION	1	0x50
//	Size				1
//	Checksum			1
// Receive:
//	Fixed header			2	0xffff
//	CMD_READ_FIRMWARE_VERSION	1	0x50
//	Size				1
//	Version length			1	Max value 23Bytes
//	Version buffer				For example: "EasyWeatherV1.2.0"
//	Checksum			1	checksum
int
read_firmware_version (int sock)
{
	int	packetlen;
	uchar	commandpacket[1024];
	int	r;
	uchar	response[1024];

	packetlen = build_command_packet (CMD_READ_FIRMWARE_VERSION, NULL, 0, commandpacket);
	if ((r = safe_write (sock, commandpacket, packetlen)) != packetlen)
		return r;
	if ((r = receive_reply_packet (sock, response, sizeof response)) < 0)
		return r;
	r = interpret_reply_packet (CMD_READ_FIRMWARE_VERSION, response, r);
	return r;
}

//------------------------------------------------
// CMD_WRITE_UPDATE ....
// Start the process to update the firmware in the Ecowitt gateway.
// Send:
//	Fixed header		2	0xffff
//	CMD_WRITE_UPDATE	1	0x43
//	Size			1
//	ServerIP		4	0xc0a80063 //"192.168.0.99"
//	ServerPort		2	1~65535
//	Checksum		1
// Receive:
//	Fixed header		2	0xffff
//	CMD_WRITE_UPDATE	1	0x43
//	Size			1
//	Result			1	0x00: success, 0x01: fail
//	Checksum		1	checksum
//
// NOTE: "port" should be in *network* byte order.
int
write_update (int sock, struct in_addr *addr, int port)
{
	int	packetlen;
	uchar	commandpacket[1024];
	uchar	databuf[1024];
	uchar	*dataptr = databuf;
	int	r;
	uchar	response[1024];
	ulong	haddr;		// address in host byteorder
	ushort	hport;		// port in host byteorder

	// fill in "databuf" with the data for this command.

	// First is the four bytes of Server IP address
	//XXX UGH! This knows about the insides of "struct in_addr".
	haddr = ntohl (addr->s_addr);		// get it into host byte order
	*dataptr++ = (haddr >> 24) & 0xff;	// highest byte
	*dataptr++ = (haddr >> 16) & 0xff;	// 
	*dataptr++ = (haddr >>  8) & 0xff;	//
	*dataptr++ = (haddr)       & 0xff;	// lowest byte

	// Next is two bytes of Server Port, high byte first
	hport = ntohs (port);			// get it into host byte order
	*dataptr++ = (hport >> 8) & 0xff;	// high byte
	*dataptr++ = (hport) & 0xff;		// low byte

	packetlen = build_command_packet (CMD_WRITE_UPDATE, databuf, (int)(dataptr - databuf), commandpacket);
	if ((r = safe_write (sock, commandpacket, packetlen)) != packetlen)
		return r;
	if ((r = receive_reply_packet (sock, response, sizeof response)) < 0)
		return r;
	r = interpret_reply_packet (CMD_WRITE_UPDATE, response, r);
	return r;
}

//==============================================================================

// various states during the firmware transfer process:
#define	STATE_BASE	0	// waiting to get "user1.bin" or "user2.bin"
#define	GOT_USER1	1	// we have gotten "user1.bin"
#define	GOT_USER2	2	// we have gotten "user2.bin"
#define	GOT_START	3	// we have gotten "start"
#define	GOT_CONTINUE	4	// we have gotten "continue"
#define	GOT_END		5	// we have gotten "end"

// helper function to decode "state" to a string:
char *
decode_state (int state)
{
	char *s;

	switch (state) {
	case STATE_BASE:	s = "state_base"; break;
	case GOT_USER1:		s = "got_user1"; break;
	case GOT_USER2:		s = "got_user2"; break;
	case GOT_START:		s = "got_start"; break;
	case GOT_CONTINUE:	s = "got_continue"; break;
	case GOT_END:		s = "got_end"; break;
	default:		s = "unknown"; break;
	}

	return s;
}

/*
 *	Firmware update - this is the part that actually handles the inbound
 *	conversation with the client device.  The process has already been
 *	initiated by sending the CMD_WRITE_UPDATE (0x43) to the device,
 *	specifying the IP address and TCP port number that the device should
 *	connect to in order to receive the firmware here, and the device has
 *	connected to our service port.  So now we talk the talk.
 *
 *	The protocol is very simple - it is all done over TCP, and all
 *	messages from the client are null-terminated.
 *
 *  ->	The client starts by asking for the firmware image that it wants
 *	with a simple "user1.bin\0" or "user2.bin\0" request. (Note: Older
 *	devices such as the GW1000 have two firmware images, referred to as
 *	"user1" and "user2". Newer device have a single image, and those
 *	devices always request "user1.bin".)
 *  <-	The server locates the correct file in the filesystem and opens it.
 *	It determines the file size (in bytes), and responds to the client
 *	with the size as a four-byte binary value, in network byte order.
 *	(NOT a string, which could be a variable length.)
 *
 *  The client usually takes a few seconds here -- I'm guessing that it's
 *  preparing the flash to store the inbound image.
 *
 *  ->	The client asks for the first data chunk by sending "start\0"
 *  <-	The server sends a buffer of data (1024 bytes, per observation of
 *	the WS View app - though the GW1000 specification says 1460.)
 * /->	The client replies with "continue\0"
 * \<-	The server loops - reading and sending the next buffer then waiting
 *	for "continue" again, until the entire image has been transferred.
 *	(The final buffer will be shorter, unless the file size is an exact
 *	multiple of the buffer size.)
 *  ->	When the client receives the full count of firmware image data, it
 *	will send "end\0", then close the TCP connection.
 */
int
do_firmware_service (int sock, int fd_user1, int fd_user2)
{
	char	line[BUFSIZ];	// client request buffer
	int	linelen;	// number of bytes read from client
	int	what;		// decoded value of what client just sent
	int	fwfd = -1;	// file descriptor for firmware file
	uchar	fwbuf[1024];	// size determined by observation of WS View app.
	int	fwlen;		// number of bytes read from firmware file
	struct stat stb;
	int	packets_sent = 0;
	int	bytes_sent = 0;
	int	currstate = STATE_BASE;
	int	nextstate = STATE_BASE;

	for ( ;; ) {
		// read the input from the client, which should end in \0
		linelen = read_until_null (sock, line, sizeof line);
		if (linelen < 0) {
			perror ("reading command from client failed");
			exit (6);
		} else if (linelen == 0) {
			printf ("\007Client closed the connection%s.\n",
				nextstate == GOT_END ? "" : " before END");
			break;
		}

		nextstate = currstate;

		/* show the input buffer to the user */
		printf (">>> %s\n", line);

		// figure out what the client said to us:
		if (linelen == 10 && memcmp (line, "user1.bin\0", 10) == 0) {
			what = GOT_USER1;
		} else if (linelen == 10 && memcmp (line, "user2.bin\0", 10) == 0) {
			what = GOT_USER2;
		} else if (linelen == 6 && memcmp (line, "start\0", 6) == 0) {
			what = GOT_START;
		} else if (linelen == 9 && memcmp (line, "continue\0", 9) == 0) {
			what = GOT_CONTINUE;
		} else if (linelen == 4 && memcmp (line, "end\0", 4) == 0) {
			what = GOT_END;
		} else {
			// we got something unexpected - say so, and bail
			printf ("%s: received unexpected \"%s\" from the client - quitting.\n",
					__FUNCTION__, line);
			exit (7);
		}

		// If we are at the base state, we expect the client
		// to specify which image they want - the request
		// should be literally "user1.bin" or "user2.bin".
		// We use this to select which open file descriptor to use.
		if (currstate == STATE_BASE) {
			// we are waiting for the client to specify which image
			// it wants - it should say "user1.bin" or "user2.bin"
			if (what == GOT_USER1) {
				fwfd = fd_user1;		// file descriptor for image 1
				nextstate = GOT_USER1;		// update the state
			} else if (what == GOT_USER2) {
				fwfd = fd_user2;		// file descriptor for image 2
				if (fwfd < 0) {			// but we don't have an image2 ?
					fprintf (stderr,
		"\n*** %s: device requested user2, but second firmware image was not specified ***\n\n",
						progname);
					return -1;
				}
				nextstate = GOT_USER2;		// update the state
			} else {
				// we got something unexpected while in the base state
				printf ("%s: received unexpected \"%s\" while in the base state - quitting.\n",
					__FUNCTION__, line);
				exit (7);
			}

			// use the open firmware file descriptor to determine
			// the size, then it to the client as a four-byte
			// binary value:
			if (fstat (fwfd, &stb) < 0) {
				fprintf (stderr, "%s: fstat failed: %s\n",
					__FUNCTION__, strerror (errno));
				exit (8);
			}

			// We need to send four bytes - the binary
			// representation of the file size, in network
			// byte order.  (NOTE: NOT a string.)
			// Do this using the st_size field, then convert
			// to network byte order.
			unsigned long size = htonl (stb.st_size);

			// send the 4-byte size value to the client:
			safe_write (sock, (uchar *)&size, 4);

			printf ("file size is %ld bytes.\n", stb.st_size);
			// After this, we expect the client to send "start".
		} else if (currstate == GOT_USER1 || currstate == GOT_USER2) {
			// the client should ask for the first block of data
			// ("start").  Then we will start sending data.
			if (what == GOT_START) {
				// got "start" - send the first data packet
				nextstate = GOT_START;	// update the state
				packets_sent = 0;
				// After this, we expect the client to send
				// a series of "continue"s until all data is sent.
			} else {
				// we got something unexpected while in the GOT_USER state
				printf ("%s: received unexpected \"%s\" while in GOT_USER state - quitting.\n",
					__FUNCTION__, line);
				exit (9);
			}
		} else if (currstate == GOT_START || currstate == GOT_CONTINUE) {
			// the client should ask for the the next block of data
			// ("continue") or say it is done ("end").
			if (what == GOT_CONTINUE) {
				// got "continue" - send the next data packet
				nextstate = GOT_CONTINUE; // update the state
			} else if (what == GOT_END) {
				// got "end" - we're all done now.
				nextstate = GOT_END;	// update the state
			} else {
				// we got something unexpected while in GOT_START/GOT_CONTINUE state
				printf ("%s: received unexpected \"%s\" while in GOT_START/GOT_CONTINUE state - quitting.\n",
					__FUNCTION__, line);
				exit (10);
			}
		} else if (currstate == GOT_END) {
			// we shouldn't be here - we should have already hopped out of the loop.
			printf ("%s: Not sure why we're here in GOT_END state!\n", __FUNCTION__);
			exit (11);
		} else {
			// we're in a totally unexpected state!
			printf ("%s: We are in unexpected state %d !!!\n", __FUNCTION__, currstate);
			exit (12);
		}

		// if we have just received either "start" or "continue",
		// read the next block from the file and send to the client:
		if (what == GOT_START || what == GOT_CONTINUE) {
			/* read data from the file */
			if ((fwlen = read (fwfd, fwbuf, sizeof fwbuf)) < 0) {
				perror ("error reading firmware data");
				break;
			}
			// if we're at EOF, drop out:
			if (fwlen == 0) {
				printf ("At EOF on firmware file after %d packet%s, %d bytes.\n",
					packets_sent, packets_sent == 1 ? "" : "s", bytes_sent);
				// *** this case actually should never happen - the client
				// *** knows that there are zero bytes remaining, so it
				// *** should send "end" instead of "continue".
			} else {
				/* send the buffer of data to the client */
				packets_sent++;
				printf ("sending packet %4d - %4d byte%s  ",
					packets_sent, fwlen, fwlen == 1 ? "" : "s");
				fflush (stdout);
				safe_write (sock, fwbuf, fwlen);
				bytes_sent += fwlen;
				printf (" - sent=%d\n", bytes_sent);
			}
			// NOTE that if fwlen shows a *partial* read, then this
			// was the last piece of the file. We expect the client
			// to send "end".
			// Otherwise, the client should send "continue" to keep
			// going.
		}

		if (nextstate != currstate) {	// time to change state:
			currstate = nextstate;	// update the state
			if (debug || verbose)
				printf ("newstate=%d [%s]\n",
					currstate, decode_state (currstate));
		}
	}

	/*
	 *	All done.
	 */
	printf ("%s: sent total of %d packet%s, %d bytes.\n",
		__FUNCTION__,
		packets_sent, packets_sent == 1 ? "" : "s", bytes_sent);

	return 0;
}


/*
 *	JCB NOTES - 9/29/2021
 *	After we send the "write_update" command, the device connects to the specified address and port.
 *	It sends user1.bin\0
 *	      OR user2.bin\0
 *	We respond with 4 bytes of size (in network byte order)
 *	It sends "start\0"
 *	We send the first packet of data (1460 bytes per spec, 1024 per observation of WSV+)
 *
 *	[ It sends "continue\0"
 *	[ We send the next packet of data (or whatever is left)
 *	[ -- repeat until finished
 *
 *	It sends "end\0" and closes the connection.
 */

/*
 *	A simple wrapper to open a firmware file for reading - prints the
 *	errno string upon failure.
 */
int
open_firmware_file (char *fname)
{
	int	fd;

	if ((fd = open (fname, O_RDONLY)) < 0) {
		fprintf (stderr,
			"%s: cannot open cannot open firmware file \"%s\": %s\n",
				progname, fname, strerror (errno));
		return -1;
	}
	return fd;
}

/*------------------------------------------------------------------------------
 *	Update the firmware on the device, using the specified binary image
 *	file(s).
 *
 *	This is the wrapper that calls the various stages involved -
 *		setup
 *		socket creation
 *		send command to the device
 *		actual data download
 */
int
update_firmware (int sock, char *fname_user1, char *fname_user2)
{
	int	r = -1;
	int	fd_user1 = -1,
		fd_user2 = -1;
	int	listen_sock;
	int	clientfd;
	struct	sockaddr_in addr, command_addr, listen_addr, claddr;
	socklen_t claddrlen = sizeof claddr;

	// Open the correct firmware file(s) based on the request.
	// Open user1 image:
	if ((fd_user1 = open_firmware_file (fname_user1)) < 0)
		return -1;

	// Open user2 image, if specified:
	if (fname_user2 != NULL &&
	    (fd_user2 = open_firmware_file (fname_user2)) < 0) {
		return -2;
	}

	/* Figure out the "our end" IP address for the connected command socket.
	 * We will specify this same address in the CMD_WRITE_UPDATE command,
	 * as that is clearly the address that the client can use to initiate
	 * the connection back to us for the actual firmware download.
	 */
	if (do_getsockname (sock, &command_addr) < 0) {
		fprintf (stderr, "%s: %s: getsockname failed: %s\n",
			progname, __FUNCTION__, strerror (errno));
		return -3;
	}
	printf ("command socket address is %s, port %hu.\n",
		inet_ntoa (command_addr.sin_addr), ntohs (command_addr.sin_port));

	/* Create the server socket.  The device will initiate a new connection
	 * to this socket for the actual firmware data download.
	 */
	if ((listen_sock = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf (stderr, "%s: %s: can't create server socket: %s\n",
			progname, __FUNCTION__, strerror (errno));
		return -4;
	}

	/* Ask to bind to the address of our command socket, with any available port,
	 * but explicitly specifying the IP address of our connected command socket.
	 * This ensures the socket has an IP address that the client can connect to.
	 */
	memset (&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr = command_addr.sin_addr;	// use the IP address from the command socket
	addr.sin_port = 0;	// assign any avilable port
	if (bind (listen_sock, (struct sockaddr *)&addr, sizeof addr) < 0) {
		fprintf (stderr, "%s: cannot bind local address %s:%d to socket: %s\n",
			__FUNCTION__, inet_ntoa (addr.sin_addr), ntohs (addr.sin_port),
			strerror (errno));
		return -5;
	}

	/* find out where our socket was bound */
	if (do_getsockname (listen_sock, &listen_addr) < 0) {
		fprintf (stderr, "%s: cannot getsockname on socket: %s\n",
			__FUNCTION__, strerror (errno));
		return -6;
	}
	printf ("firmware server socket is bound to host address %s, port %hu\n",
		inet_ntoa (listen_addr.sin_addr), ntohs (listen_addr.sin_port));

	// mark the socket as listening for inbound connections:
	if (listen (listen_sock, -1) < 0) {
		fprintf (stderr, "%s: cannot listen() on socket: %s\n",
			__FUNCTION__, strerror (errno));
		return -7;
	}

	// The socket is now ready.
	// Tell the device to contact this new listening socket for the
	// firmware update:
	r = write_update (sock, &listen_addr.sin_addr, listen_addr.sin_port);
	if (r < 0) {
		printf ("%s: write_update failed.\n", __FUNCTION__);
		return r;
	}

	// Now wait for the device to connect to our listening socket to
	// request and download the actual firmware data...
	printf ("Waiting for inbound connection... ");
	fflush (stdout);
	for ( ;; ) {
		/* Wait for and accept the inbound connection.
		 * Note that "accept()" will block until a client arrives.
		 */
		claddrlen = sizeof claddr;
		clientfd = accept (listen_sock, (struct sockaddr *)&claddr,
					&claddrlen);
		if (clientfd < 0) {
			if (errno == EINTR)
				continue;
			fprintf (stderr, "%s: cannot accept incoming connection on socket: %s\n",
				__FUNCTION__, strerror (errno));
			break;
		}

		printf ("\n%s: received inbound connection from address %s, port %hu\n",
			__FUNCTION__,
			inet_ntoa (claddr.sin_addr), ntohs (claddr.sin_port));

		// Talk the protocol with the client:
		r = do_firmware_service (clientfd, fd_user1, fd_user2);

		// close the connection to the client now that we're done.
		close (clientfd);

		// we only handle one client per invocation, so we're done now.
		break;
	}

	// Clean up and close our listening socket now - we don't expect another client.
	close (listen_sock);

	return r;
}

/*
 *	Write a buffer to the given file descriptor, being persistent,
 *	as we may only get a partial write if the fd is in non-blocking mode
 *	or if output is interrupted due to a signal.
 */
int
safe_write (int fd, uchar *bufp, int len)
{
	register int r;
	int	remain;
	int	written = 0;
	int	save_errno;

	remain = len;
	while (remain > 0) {
		if ((r = write (fd, bufp, remain)) > 0) {
			bufp += r;
			remain -= r;
			written += r;
		}
		if (r <= 0) {		// couldn't write any
			save_errno = errno;
			perror ("safe_write");
			if (save_errno == ENXIO || save_errno == EPIPE)
				break;
		}
	}
	if (written != len)
		fprintf (stderr,
			"%s(fd=%d): didn't write %d - wrote %d instead.\n",
			__FUNCTION__, fd,
			len, written);

	return written;
}

/*
 *	Open a socket to the specified host and service/port.
 *	Returns the connected stream socket descriptor.
 */
int
open_socket (char *host, char *service)
{
	struct addrinfo hints = {0};
	struct addrinfo *p, *addresses;
	int	sock = -1;
	int	r;
	char	hostbuf[NI_MAXHOST];
	char	servbuf[16];

	// give it some hints - the devices only support IPv4 and TCP
	hints.ai_family = AF_INET;		/* IPv4 only */
	hints.ai_socktype = SOCK_STREAM;	/* TCP */

	// ask for the address information based on 'host' and 'service':
	r = getaddrinfo (host, service, &hints, &addresses);
	if (r != 0) {
		fprintf (stderr, "%s: could not resolve host \"%s\", port/service \"%s\": %s\n",
				__FUNCTION__, host, service, gai_strerror (r));
		return -1;
	}

	// try each possibility that we got from getaddrinfo():
	for (p = addresses; p != NULL; p = p->ai_next) {
		sock = socket (p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sock == -1)		// might be unsupported type or protocol - don't complain
			continue;

		// map that address back to IP address and port so we can display it:
		(void) getnameinfo (p->ai_addr, p->ai_addrlen,
					hostbuf, sizeof hostbuf, servbuf, sizeof servbuf,
					NI_NUMERICHOST | NI_NUMERICSERV);
		if (debug || verbose) {
			printf ("Attempting to connect to host address %s, port %s\n", hostbuf, servbuf);
		}

		// try to connect to the address:
		r = connect (sock, p->ai_addr, p->ai_addrlen);
		if (r == -1) {		// connection failed
			//XXX should we display an error?
			fprintf (stderr, "Cannot connect to host address %s, port %s: %s\n",
				hostbuf, servbuf, strerror (errno));

			close (sock);
			sock = -1;
			continue;
		}
		// connect succeeded - break out of the loop
		if (debug || verbose) {
			printf ("connected to server, socket file descriptor is %d\n", sock);
		}
		break;
	}

	// free the list of addresses - we don't need it any longer
	freeaddrinfo (addresses);

	/* return the new connected socket fd, or -1 on failure */
	return sock;
}

/*
 *	A wrapper function to do "getsockname" easily.
 */
int
do_getsockname (int s, struct sockaddr_in *addrptr)
{
	socklen_t addrlen;
	addrlen = sizeof (*addrptr);
	memset (addrptr, 0, addrlen);
	return getsockname (s, (struct sockaddr *)addrptr, &addrlen);
}

// dump out the raw data, with some tiny amount of formatting.
void
hexdump (uchar *data, int length)
{
	int	i;
	uchar	c;
	int	numdigits;

	// figure out how many digits we'll need for the length prefix:
	char tmpbuf[32+1];
	snprintf (tmpbuf, sizeof tmpbuf, "%d", length);
	numdigits = strlen (tmpbuf);

	for (i = 0; i < length; i++) {
		c = *data++;

		// print the counter at the start of each line:
		if (i % 16 == 0) {
			printf ("  %0*d:  ", numdigits, i);
		}

		// print separator if not at the start of the line
		if (i % 16 != 0)
			printf (" ");

		printf ("%02x", ((unsigned int) c) & 0x00ff);

		if (i % 16 == 7)	// two spaces between groups of 8
			printf (" ");
		if (i % 16 == 15)	// newline after 16
			printf ("\n");
	}
	printf ("\n");
}

/*
 * Read() with a timeout.
 * Returns the number of bytes actually read, -2 on timeout, -1 on other errors.
 */
int
timed_read (int fd, char *buf, int len, int timeout)
{
	int	r;
	struct pollfd pfds[1];

	for ( ;; ) {
		/* set up the pollfd structure */
		pfds[0].fd = fd;	/* file descriptor to read */
		pfds[0].events = POLLIN | POLLRDNORM;
		/* timeout needs to be converted to milliseconds */
		if (poll (pfds, 1, timeout * 1000) < 0) {
			perror ("poll");
			break;
		}
		if ((pfds[0].revents & (POLLIN | POLLRDNORM)) != 0) {
			// input is available - read it and return
			r = read (fd, buf, len);
			return r;
		}
		/* if we got here, nothing was ready to read */
		buf[0] = 0xff;
		return -2;
	}
	return -1;
}


/*
 *	Read input until a terminating null is seen.
 *	Returns the number of bytes read (including the null),
 *	or 0 if the connection is closed before receiving the null,
 *	or -1 if a read error has occurred.
 */
int
read_until_null (int fd, char *buf, int bufsiz)
{
	int	remain = bufsiz;
	char	*bufp = buf;
	int	count = 0;
	char	cbuf[1];
	int	r;

	while (remain > 0) {
		// try to read one byte of input:
		r = read (fd, cbuf, 1);
		if (r < 0) {	// read error occurred
			fprintf (stderr, "%s: error reading from socket: %s\n",
				__FUNCTION__, strerror (errno));
			*bufp = '\0';
			return -1;
		}
		if (r == 0) {	// connection was closed
			*bufp = '\0';
			return 0;
		}
		// add the character to the buffer:
		*bufp++ = cbuf[0];
		count++;

		// if the character was the trailing null, we're done
		if (cbuf[0] == '\0')
			break;
		remain--;
	}
	return count;
}


void
usage (void)
{
	fprintf (stderr,
		"Usage: %s [-d][-v] [-h host] [-p port] [-u firmware_image [firmware_image2]]\n",
		progname);
	exit (1);
	/*NOTREACHED*/
}


int
main (int argc, char **argv)
{
	int	c;
	char	*cp;
	int	sock = -1;
	int	r;
	char	*host = NULL,
		*service = "45000";	// default port for Ecowitt API
	char	*firmware1 = NULL,
		*firmware2 = NULL;

	/* Always ensure that stdout and stderr are line-buffered,
	 * even if output is redirected to a file, so that tracing
	 * becomes easier.
	 */
	setvbuf (stdout, NULL, _IOLBF, BUFSIZ);
	setvbuf (stderr, NULL, _IOLBF, BUFSIZ);

	if ((cp = strrchr (argv[0], '/')) != NULL) {
		*cp++ = '\0';	/* zap the slash */
		progname = cp;		/* progname becomes basename */
		argv[0] = progname;	/* for the benefit of getopt() */
	} else {
		progname = argv[0];	/* progname is simply argv[0] */
	}

	// process command-line options right away, particularly
	// so we can have "debug" and "verbose" set correctly!
	while ((c = getopt (argc, argv, "h:p:udv")) != EOF) {
		switch (c) {
		case 'h':	// specify the host name or IP address
			host = optarg;
			break;
		case 'p':	// specify the port/service
			service = optarg;
			break;
		case 'd':	// enable debugging
			debug++;
			break;
		case 'u':	// actually do the update
			update++;
			break;
		case 'v':	// enable verbose mode
			verbose++;
			break;
		case '?':	/* bad option */
			usage ();
			break;
		}
	}

	// Make sure the host was specified:
	if (host == NULL) {
		fprintf (stderr,
			"%s: missing host name or address - use \"-h host\".\n",
				progname);
		exit (1);
	}

	// If they want to update, we need one or two arguments to specify
	// the firmware file(s):
	if (update) {
		if (optind < argc) {	// additional arg(s) present
			firmware1 = argv[optind++];
			if (optind != argc) {	// at least one more arg
				firmware2 = argv[optind++];
			}
		} else {
			fprintf (stderr, "%s: missing firmware file(s)\n",
				progname);
			usage();
		}
	}
	if (optind != argc) {	/* more args were given */
		fprintf (stderr,
			"%s: too many arguments specified -- \"%s\" ...\n",
			progname, argv[optind]);
		usage ();
	}

	/* attempt to open a connection to the device */
	if ((sock = open_socket (host, service)) < 0) {
		fprintf (stderr, "%s: can't connect to %s/%s\n",
			progname, host, service);
		exit (2);
	}

	// Read the hardware MAC address:
	r = read_station_mac (sock);

	// Read the firmware version, which also tells us the model:
	r = read_firmware_version (sock);

	// If we want to actually do the update, do that now:
	if (update) {
		printf ("Updating firmware (fname1=%s, fname2=%s):\n",
			firmware1, firmware2 ? firmware2 : "<null>");

		r = update_firmware (sock, firmware1, firmware2);

		if (r != 0)
			printf ("Firmware update failed.\n");
	}

	/*
	 *	All done, shut down the socket and quit.
	 */
	shutdown (sock, 2);
	if (close (sock) < 0) {
		perror ("socket close");
		exit (8);
	}

	exit (r);
}

