#pragma once

#include <winsock.h>

#define HOST_ENDIAN_LONG(x) x = ntohl(x);
#define HOST_ENDIAN_SHORT(x) x = ntohs(x);

#define NETWORK_ENDIAN_LONG(x) x = htonl(x);
#define NETWORK_ENDIAN_SHORT(x) x = htons(x);
