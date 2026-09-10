//******************************************************************************
// FXUTIL.H -- FlasherX utility functions
//******************************************************************************
#ifndef FXUTIL_H_
#define FXUTIL_H_

// Returns the character count, -1 at EOF before a line, or -2 when the input
// line exceeded the supplied buffer. The destination is always terminated.
int read_ascii_line( Stream *serial, char *line, int maxbytes );
void update_firmware( Stream *in, Stream *out,
			uint32_t buffer_addr, uint32_t buffer_size,
			uint32_t expected_crc = 0, bool verify_crc = false );

#endif
