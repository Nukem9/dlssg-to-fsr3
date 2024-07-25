/*
 * Local defines for the md5 functions.
 *
 * $Id: md5_loc.h,v 1.5 2010-05-07 13:58:18 gray Exp $
 */

#ifndef __MD5_LOC_H__
#define __MD5_LOC_H__

/*
 * We don't include "conf.h" here because it gets included before this file in md5.cpp so the defines
 * are correctly determing before they are checked.
 */
 #if MD5_DEBUG
    #include <iostream>
#endif // MD5_DEBUG

/// For now we are assuming everything is in little endian byte-order

namespace md5 {
    /*
     * T denotes the integer part of the i-th element of the function:
     * T[i] = 4294967296 * abs(sin(i)), where i is in radians.
     */
    const unsigned int T[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
    };

    /*
     * Constants for the MD5 Transform routine as defined in RFC 1321
     */
    const unsigned int S1[4] = {7, 12, 17, 22};
    const unsigned int S2[4] = {5, 9,  14, 20};
    const unsigned int S3[4] = {4, 11, 16, 23};
    const unsigned int S4[4] = {6, 10, 15, 21};

    /*
     * Function to perform the cyclic left rotation of blocks of data
     */
    inline unsigned int cyclic_left_rotate(unsigned int data, unsigned int shift_bits) {
        return (data << shift_bits) | (data >> (32 - shift_bits));
    }

    inline unsigned int F(unsigned int x, unsigned int y, unsigned int z) {return (x & y) | (~x & z);};
    inline unsigned int G(unsigned int x, unsigned int y, unsigned int z) {return (x & z) | (y & ~z);};
    inline unsigned int H(unsigned int x, unsigned int y, unsigned int z) {return x ^ y ^ z;};
    inline unsigned int I(unsigned int x, unsigned int y, unsigned int z) {return y ^ (x | ~z);};

    inline void FF(unsigned int &a, unsigned int b, unsigned int c, unsigned int d, unsigned int Xk, unsigned int s, unsigned int i) {
        #if MD5_DEBUG
            std::cout << "\nA: " << a << "\nB: " << b << "\nC: " << c << "\nD: " << d << "\nX[" << i << "]: " << Xk << "\ns: " << S1[s] << "\nT: " << T[i] << "\n";
        #endif

        a += F(b,c,d) + Xk + T[i];
        a = cyclic_left_rotate(a, S1[s]);
        a += b;

        #if MD5_DEBUG
            std::cout << "A = " << a << "\n";
        #endif
    };

    inline void GG(unsigned int &a, unsigned int b, unsigned int c, unsigned int d, unsigned int Xk, unsigned int s, unsigned int i) {
        #if MD5_DEBUG
            std::cout << "\nA: " << a << "\nB: " << b << "\nC: " << c << "\nD: " << d << "\nX[" << i - 16 << "]: " << Xk << "\ns: " << S2[s] << "\nT: " << T[i] << "\n";
        #endif // MD5_DEBUG

        a += G(b,c,d) + Xk + T[i];
        a = cyclic_left_rotate(a, S2[s]);
        a += b;

        #if MD5_DEBUG
            std::cout << "A = " << a << "\n";
        #endif // MD5_DEBUG
    };

    inline void HH(unsigned int &a, unsigned int b, unsigned int c, unsigned int d, unsigned int Xk, unsigned int s, unsigned int i) {
        #if MD5_DEBUG
            std::cout << "\nA: " << a << "\nB: " << b << "\nC: " << c << "\nD: " << d << "\nX[" << i - 32 << "]: " << Xk << "\ns: " << S3[s] << "\nT: " << T[i] << "\n";
        #endif // MD5_DEBUG

        a += H(b,c,d) + Xk + T[i];
        a = cyclic_left_rotate(a, S3[s]);
        a += b;

        #if MD5_DEBUG
            std::cout << "A = " << a << "\n";
        #endif // MD5_DEBUG
    };
    inline void II(unsigned int &a, unsigned int b, unsigned int c, unsigned int d, unsigned int Xk, unsigned int s, unsigned int i) {
        #if MD5_DEBUG
            std::cout << "\nA: " << a << "\nB: " << b << "\nC: " << c << "\nD: " << d << "\nX[" << i - 48 << "]: " << Xk << "\ns: " << S4[s] << "\nT: " << T[i] << "\n";
        #endif // MD5_DEBUG

        a += I(b,c,d) + Xk + T[i];
        a = cyclic_left_rotate(a, S4[s]);
        a += b;

        #if MD5_DEBUG
            std::cout << "A = " << a << "\n";
        #endif // MD5_DEBUG
    };

    /*
     * Define my endian-ness.  Could not do in a portable manner using the
     * include files -- grumble.
     */
    #if MD5_BIG_ENDIAN

    /*
     * big endian - big is better
     */
    #define MD5_SWAP(n) (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))

    #else

    /*
     * little endian
     */
    #define MD5_SWAP(n) (n)

    #endif // MD5_BIG_ENDIAN

    const char* HEX_STRING = "0123456789abcdef";    /* to convert to hex */
}

#endif /* ! __MD5_LOC_H__ */
