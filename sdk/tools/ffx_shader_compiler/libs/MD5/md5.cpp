#include <cassert>
#include <cstring>
#include <iostream>

#include "md5.h"
#include "md5_loc.h"

namespace md5 {
    /****************************** Public Functions ******************************/

    /*
     * md5_t
     *
     * DESCRIPTION:
     *
     * Initialize structure containing state of MD5 computation. (RFC 1321,
     * 3.3: Step 3).  This is for progressive MD5 calculations only.  If
     * you have the complete string available, call it as below.
     * process should be called for each bunch of bytes and after the
     * last process call, finish should be called to get the signature.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * None.
     */
    md5_t::md5_t() {
        initialise();
    }

    /*
     * md5_t
     *
     * DESCRIPTION:
     *
     * This function is used to calculate a MD5 signature for a buffer of
     * bytes.  If you only have part of a buffer that you want to process
     * then md5_t, process, and finish should be used.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * buffer - A buffer of bytes whose MD5 signature we are calculating.
     *
     * input_length - The length of the buffer.
     *
     * signature - A 16 byte buffer that will contain the MD5 signature.
     */
    md5_t::md5_t(const void* input, const unsigned int input_length, void* signature) {
        /* initialize the computation context */
        initialise();

        /* process whole buffer but last input_length % MD5_BLOCK bytes */
        process(input, input_length);

        /* put result in desired memory area */
        finish(signature);
    }

    /*
     * process
     *
     * DESCRIPTION:
     *
     * This function is used to progressively calculate a MD5 signature some
     * number of bytes at a time.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * buffer - A buffer of bytes whose MD5 signature we are calculating.
     *
     * input_length - The length of the buffer.
     */
    void md5_t::process(const void* input, const unsigned int input_length) {
        if (!finished) {
            unsigned int processed = 0;

            /*
             * If we have any data stored from a previous call to process then we use these
             * bytes first, and the new data is large enough to create a complete block then
             * we process these bytes first.
             */
            if (stored_size && input_length + stored_size >= md5::BLOCK_SIZE) {
                unsigned char block[md5::BLOCK_SIZE];
                memcpy(block, stored, stored_size);
                memcpy(block + stored_size, input, md5::BLOCK_SIZE - stored_size);
                processed = md5::BLOCK_SIZE - stored_size;
                stored_size = 0;
                process_block(block);
            }

            /*
             * While there is enough data to create a complete block, process it.
             */
            while (processed + md5::BLOCK_SIZE <= input_length) {
                process_block((unsigned char*)input + processed);
                processed += md5::BLOCK_SIZE;
            }

            /*
             * If there are any unprocessed bytes left over that do not create a complete block
             * then we store these bytes for processing next time.
             */
            if (processed != input_length) {
                memcpy(stored + stored_size, (char*)input + processed, input_length - processed);
                stored_size += input_length - processed;
            } else {
                stored_size = 0;
            }
        } else {
            // throw error when trying to process after completion?
        }
    }

    /*
     * finish
     *
     * DESCRIPTION:
     *
     * Finish a progressing MD5 calculation and copy the resulting MD5
     * signature into the result buffer which should be 16 bytes
     * (MD5_SIZE).  After this call, the MD5 structure cannot process
	 * additional bytes.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * signature - A 16 byte buffer that will contain the MD5 signature.
     */
    void md5_t::finish(void* signature_) {
        if (!finished) {
            if (message_length[0] + stored_size < message_length[0])
                message_length[1]++;
            message_length[0] += stored_size;

            int pad = md5::BLOCK_SIZE - (sizeof(unsigned int) * 2) - stored_size;
            if (pad <= 0)
                pad += md5::BLOCK_SIZE;

            /*
             * Modified from a fixed array to this assignment and memset to be
             * more flexible with block-sizes -- Gray 10/97.
             */
            if (pad > 0) {
                stored[stored_size] = 0x80;
                if (pad > 1)
                    memset(stored + stored_size + 1, 0, pad - 1);
                stored_size += pad;
            }

            /*
             * Put the 64-bit file length in _bits_ (i.e. *8) at the end of the
             * buffer. appears to be in beg-endian format in the buffer?
             */
            unsigned int size_low = ((message_length[0] & 0x1FFFFFFF) << 3);
            memcpy(stored + stored_size, &size_low, sizeof(unsigned int));
            stored_size += sizeof(unsigned int);

            /* shift the high word over by 3 and add in the top 3 bits from the low */
            unsigned int size_high = (message_length[1] << 3) | ((message_length[0] & 0xE0000000) >> 29);
            memcpy(stored + stored_size, &size_high, sizeof(unsigned int));
            stored_size += sizeof(unsigned int);

            /*
             * process the last block of data.
             * if the length of the message was already exactly sized, then we have
             * 2 messages to process
             */
            process_block(stored);
            if (stored_size > md5::BLOCK_SIZE)
                process_block(stored + md5::BLOCK_SIZE);

            /* Arrange the results into a signature */
            get_result(static_cast<void*>(signature));

            /* store the signature into a readable sring */
            sig_to_string(signature, str, MD5_STRING_SIZE);

            if (signature_ != NULL) {
                memcpy(signature_, static_cast<void*>(signature), MD5_SIZE);
            }

            finished = true;
        } else {
            // add error?
        }
    }

    /*
     * get_sig
     *
     * DESCRIPTION:
     *
     * Retrieves the previously calculated signature from the MD5 object.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * signature_ - A 16 byte buffer that will contain the MD5 signature.
     */
    void md5_t::get_sig(void* signature_) {
        if (finished) {
            memcpy(signature_, signature, MD5_SIZE);
        } else {
            //error?
        }
    }

    /*
     * get_string
     *
     * DESCRIPTION:
     *
     * Retrieves the previously calculated signature from the MD5 object in
     * printable format.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * str_ - a string of characters which should be at least 33 bytes long
     * (2 characters per MD5 byte and 1 for the \0).
     */
    void md5_t::get_string(void* str_) {
        if (finished) {
            memcpy(str_, str, MD5_STRING_SIZE);
        } else {
            // error?
        }
    }

    /****************************** Private Functions ******************************/

    /*
     * initialise
     *
     * DESCRIPTION:
     *
     * Initialize structure containing state of MD5 computation. (RFC 1321,
     * 3.3: Step 3).
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * None.
     */
    void md5_t::initialise() {
        /*
         * ensures that unsigned int is 4 bytes on this platform, will need modifying
         * if we are to use on a different sized platform.
         */
        assert(MD5_SIZE == 16);

        A = 0x67452301;
        B = 0xefcdab89;
        C = 0x98badcfe;
        D = 0x10325476;

        message_length[0] = 0;
        message_length[1] = 0;
        stored_size = 0;

        finished = false;
    }

    /*
     * process_block
     *
     * DESCRIPTION:
     *
     * Process a block of bytes into a MD5 state structure.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * buffer - A buffer of bytes whose MD5 signature we are calculating.
     *
     * input_length - The length of the buffer.
     */
    void md5_t::process_block(const unsigned char* block) {
    /* Process each 16-word block. */

        /*
         * we check for when the lower word rolls over, and increment the
         * higher word. we do not need to worry if the higher word rolls over
         * as only the two words we maintain are needed in the function later
         */
        if (message_length[0] + md5::BLOCK_SIZE < message_length[0])
            message_length[1]++;
        message_length[0] += BLOCK_SIZE;

        // Copy the block into X. */
        unsigned int X[16];
        for (unsigned int i = 0; i < 16; i++) {
            memcpy(X + i, block + 4 * i, 4);
        }

        /* Save A as AA, B as BB, C as CC, and D as DD. */
        unsigned int AA = A, BB = B, CC = C, DD = D;

        /* Round 1
         * Let [abcd k s i] denote the operation
         * a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s)
         * Do the following 16 operations
         * [ABCD  0  7  1]  [DABC  1 12  2]  [CDAB  2 17  3]  [BCDA  3 22  4]
         * [ABCD  4  7  5]  [DABC  5 12  6]  [CDAB  6 17  7]  [BCDA  7 22  8]
         * [ABCD  8  7  9]  [DABC  9 12 10]  [CDAB 10 17 11]  [BCDA 11 22 12]
         * [ABCD 12  7 13]  [DABC 13 12 14]  [CDAB 14 17 15]  [BCDA 15 22 16]
         */
        md5::FF(A, B, C, D, X[0 ], 0, 0 );
        md5::FF(D, A, B, C, X[1 ], 1, 1 );
        md5::FF(C, D, A, B, X[2 ], 2, 2 );
        md5::FF(B, C, D, A, X[3 ], 3, 3 );
        md5::FF(A, B, C, D, X[4 ], 0, 4 );
        md5::FF(D, A, B, C, X[5 ], 1, 5 );
        md5::FF(C, D, A, B, X[6 ], 2, 6 );
        md5::FF(B, C, D, A, X[7 ], 3, 7 );
        md5::FF(A, B, C, D, X[8 ], 0, 8 );
        md5::FF(D, A, B, C, X[9 ], 1, 9 );
        md5::FF(C, D, A, B, X[10], 2, 10);
        md5::FF(B, C, D, A, X[11], 3, 11);
        md5::FF(A, B, C, D, X[12], 0, 12);
        md5::FF(D, A, B, C, X[13], 1, 13);
        md5::FF(C, D, A, B, X[14], 2, 14);
        md5::FF(B, C, D, A, X[15], 3, 15);

        /* Round 2
         * Let [abcd k s i] denote the operation
         * a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s)
         * Do the following 16 operations
         * [ABCD  1  5 17]  [DABC  6  9 18]  [CDAB 11 14 19]  [BCDA  0 20 20]
         * [ABCD  5  5 21]  [DABC 10  9 22]  [CDAB 15 14 23]  [BCDA  4 20 24]
         * [ABCD  9  5 25]  [DABC 14  9 26]  [CDAB  3 14 27]  [BCDA  8 20 28]
         * [ABCD 13  5 29]  [DABC  2  9 30]  [CDAB  7 14 31]  [BCDA 12 20 32]
         */
        md5::GG(A, B, C, D, X[1 ], 0, 16);
        md5::GG(D, A, B, C, X[6 ], 1, 17);
        md5::GG(C, D, A, B, X[11], 2, 18);
        md5::GG(B, C, D, A, X[0 ], 3, 19);
        md5::GG(A, B, C, D, X[5 ], 0, 20);
        md5::GG(D, A, B, C, X[10], 1, 21);
        md5::GG(C, D, A, B, X[15], 2, 22);
        md5::GG(B, C, D, A, X[4 ], 3, 23);
        md5::GG(A, B, C, D, X[9 ], 0, 24);
        md5::GG(D, A, B, C, X[14], 1, 25);
        md5::GG(C, D, A, B, X[3 ], 2, 26);
        md5::GG(B, C, D, A, X[8 ], 3, 27);
        md5::GG(A, B, C, D, X[13], 0, 28);
        md5::GG(D, A, B, C, X[2 ], 1, 29);
        md5::GG(C, D, A, B, X[7 ], 2, 30);
        md5::GG(B, C, D, A, X[12], 3, 31);

        /* Round 3
         * Let [abcd k s i] denote the operation
         * a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s)
         * Do the following 16 operations
         * [ABCD  5  4 33]  [DABC  8 11 34]  [CDAB 11 16 35]  [BCDA 14 23 36]
         * [ABCD  1  4 37]  [DABC  4 11 38]  [CDAB  7 16 39]  [BCDA 10 23 40]
         * [ABCD 13  4 41]  [DABC  0 11 42]  [CDAB  3 16 43]  [BCDA  6 23 44]
         * [ABCD  9  4 45]  [DABC 12 11 46]  [CDAB 15 16 47]  [BCDA  2 23 48]
         */
        md5::HH(A, B, C, D, X[5 ], 0, 32);
        md5::HH(D, A, B, C, X[8 ], 1, 33);
        md5::HH(C, D, A, B, X[11], 2, 34);
        md5::HH(B, C, D, A, X[14], 3, 35);
        md5::HH(A, B, C, D, X[1 ], 0, 36);
        md5::HH(D, A, B, C, X[4 ], 1, 37);
        md5::HH(C, D, A, B, X[7 ], 2, 38);
        md5::HH(B, C, D, A, X[10], 3, 39);
        md5::HH(A, B, C, D, X[13], 0, 40);
        md5::HH(D, A, B, C, X[0 ], 1, 41);
        md5::HH(C, D, A, B, X[3 ], 2, 42);
        md5::HH(B, C, D, A, X[6 ], 3, 43);
        md5::HH(A, B, C, D, X[9 ], 0, 44);
        md5::HH(D, A, B, C, X[12], 1, 45);
        md5::HH(C, D, A, B, X[15], 2, 46);
        md5::HH(B, C, D, A, X[2 ], 3, 47);

        /* Round 4
         * Let [abcd k s i] denote the operation
         * a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s)
         * Do the following 16 operations
         * [ABCD  0  6 49]  [DABC  7 10 50]  [CDAB 14 15 51]  [BCDA  5 21 52]
         * [ABCD 12  6 53]  [DABC  3 10 54]  [CDAB 10 15 55]  [BCDA  1 21 56]
         * [ABCD  8  6 57]  [DABC 15 10 58]  [CDAB  6 15 59]  [BCDA 13 21 60]
         * [ABCD  4  6 61]  [DABC 11 10 62]  [CDAB  2 15 63]  [BCDA  9 21 64]
         */
        md5::II(A, B, C, D, X[0 ], 0, 48);
        md5::II(D, A, B, C, X[7 ], 1, 49);
        md5::II(C, D, A, B, X[14], 2, 50);
        md5::II(B, C, D, A, X[5 ], 3, 51);
        md5::II(A, B, C, D, X[12], 0, 52);
        md5::II(D, A, B, C, X[3 ], 1, 53);
        md5::II(C, D, A, B, X[10], 2, 54);
        md5::II(B, C, D, A, X[1 ], 3, 55);
        md5::II(A, B, C, D, X[8 ], 0, 56);
        md5::II(D, A, B, C, X[15], 1, 57);
        md5::II(C, D, A, B, X[6 ], 2, 58);
        md5::II(B, C, D, A, X[13], 3, 59);
        md5::II(A, B, C, D, X[4 ], 0, 60);
        md5::II(D, A, B, C, X[11], 1, 61);
        md5::II(C, D, A, B, X[2 ], 2, 62);
        md5::II(B, C, D, A, X[9 ], 3, 63);

        /* Then perform the following additions. (That is increment each
        of the four registers by the value it had before this block
        was started.) */
        A += AA;
        B += BB;
        C += CC;
        D += DD;
    }

    /*
     * get_result
     *
     * DESCRIPTION:
     *
     * Copy the resulting MD5 signature into the first 16 bytes (MD5_SIZE)
     * of the result buffer.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * result - A 16 byte buffer that will contain the MD5 signature.
     */
    void md5_t::get_result(void *result) {
        memcpy((char*)result, &A, sizeof(unsigned int));
        memcpy((char*)result + sizeof(unsigned int), &B, sizeof(unsigned int));
        memcpy((char*)result + 2 * sizeof(unsigned int), &C, sizeof(unsigned int));
        memcpy((char*)result + 3 * sizeof(unsigned int), &D, sizeof(unsigned int));
    }

    /****************************** Exported Functions ******************************/

    /*
     * sig_to_string
     *
     * DESCRIPTION:
     *
     * Convert a MD5 signature in a 16 byte buffer into a hexadecimal string
     * representation.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * signature_ - a 16 byte buffer that contains the MD5 signature.
     *
     * str_ - a string of charactes which should be at least 33 bytes long (2
     * characters per MD5 byte and 1 for the \0).
     *
     * str_len - the length of the string.
     */
    void sig_to_string(const void* signature_, char* str_, const int str_len) {
        unsigned char* sig_p;
        char* str_p;
        char* max_p;
        unsigned int high, low;

        str_p = str_;
        max_p = str_ + str_len;

        for (sig_p = (unsigned char*)signature_; sig_p < (unsigned char*)signature_ + MD5_SIZE; sig_p++) {
            high = *sig_p / 16;
            low = *sig_p % 16;
            /* account for 2 chars */
            if (str_p + 1 >= max_p) {
                break;
            }
            *str_p++ = md5::HEX_STRING[high];
            *str_p++ = md5::HEX_STRING[low];
        }
        /* account for 2 chars */
        if (str_p < max_p) {
            *str_p++ = '\0';
        }
    }

    /*
     * sig_from_string
     *
     * DESCRIPTION:
     *
     * Convert a MD5 signature from a hexadecimal string representation into
     * a 16 byte buffer.
     *
     * RETURNS:
     *
     * None.
     *
     * ARGUMENTS:
     *
     * signature_ - A 16 byte buffer that will contain the MD5 signature.
     *
     * str_ - A string of charactes which _must_ be at least 32 bytes long (2
     * characters per MD5 byte).
     */
    void sig_from_string(void* signature_, const char* str_) {
        unsigned char *sig_p;
        const char *str_p;
        char* hex;
        unsigned int high, low, val;

        hex = (char*)md5::HEX_STRING;
        sig_p = static_cast<unsigned char*>(signature_);

        for (str_p = str_; str_p < str_ + MD5_SIZE * 2; str_p += 2) {
            high = strchr(hex, *str_p) - hex;
            low = strchr(hex, *(str_p + 1)) - hex;
            val = high * 16 + low;
            *sig_p++ = val;
        }
    }
} // namespace md5

