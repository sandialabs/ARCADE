#pragma once

/// Largest IPv4 UDP payload.
///
/// Publications must fit in one datagram.
#define UDP_MAX_PAYLOAD 65507

/// Broadcast a complete message.
///
/// Returns 0 on success, -1 on error.
int UDP_Server(const char *msg);

/// Send a stop message over UDP.
void UDP_Stop(void);

// Port to broadcast UDP messages on.
#define UDP_PORT 8000
