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

#ifndef CONFIG_H
#define CONFIG_H

// Bot settings structure.
typedef struct {
    char uuid[16];          // UUID of the bot instance. Used internally.
    char hostname[64];      // Hostname or IP address to connect to.
    int port;               // Port to connect to. Defaults to 5555.

#ifndef TICK_FEATURES_NO_CRYPTO
    uint8_t aes_key[TICK_AES_SIZE / 8];     // AES encryption key.
    uint8_t aes_iv[TICK_AES_SIZE / 8];      // AES initial IV (optional).
#endif

} Settings;

#endif /* CONFIG_H */
