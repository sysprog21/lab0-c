/*
 * Generate precalculated values of log2 with assumption that arg will be left
 * shifted by 16 bit and return value of log2_lshift16() will be left shifted
 * by 3 bit All that shifts used for avoid of using floating point in
 * calculation.
 */

#include <stdint.h>

#define LOG2_ARG_SHIFT (1 << 16)
#define LOG2_RET_SHIFT (1 << 3)

/* store precalculated function (log2(arg << 24)) << 3 */
static inline int log2_lshift16(uint64_t lshift16)
{
    if (lshift16 < 558) {
        if (lshift16 < 54) {
            if (lshift16 < 13) {
                if (lshift16 < 7) {
                    if (lshift16 < 1)
                        return -136;
                    if (lshift16 < 2)
                        return -123;
                    if (lshift16 < 3)
                        return -117;
                    if (lshift16 < 4)
                        return -113;
                    if (lshift16 < 5)
                        return -110;
                    if (lshift16 < 6)
                        return -108;
                    if (lshift16 < 7)
                        return -106;
                } else {
                    if (lshift16 < 8)
                        return -104;
                    if (lshift16 < 9)
                        return -103;
                    if (lshift16 < 10)
                        return -102;
                    if (lshift16 < 11)
                        return -100;
                    if (lshift16 < 12)
                        return -99;
                    if (lshift16 < 13)
                        return -98;
                }
            } else {
                if (lshift16 < 29) {
                    if (lshift16 < 15)
                        return -97;
                    if (lshift16 < 16)
                        return -96;
                    if (lshift16 < 17)
                        return -95;
                    if (lshift16 < 19)
                        return -94;
                    if (lshift16 < 21)
                        return -93;
                    if (lshift16 < 23)
                        return -92;
                    if (lshift16 < 25)
                        return -91;
                    if (lshift16 < 27)
                        return -90;
                    if (lshift16 < 29)
                        return -89;
                } else {
                    if (lshift16 < 32)
                        return -88;
                    if (lshift16 < 35)
                        return -87;
                    if (lshift16 < 38)
                        return -86;
                    if (lshift16 < 41)
                        return -85;
                    if (lshift16 < 45)
                        return -84;
                    if (lshift16 < 49)
                        return -83;
                    if (lshift16 < 54)
                        return -82;
                }
            }
        } else {
            if (lshift16 < 181) {
                if (lshift16 < 99) {
                    if (lshift16 < 59)
                        return -81;
                    if (lshift16 < 64)
                        return -80;
                    if (lshift16 < 70)
                        return -79;
                    if (lshift16 < 76)
                        return -78;
                    if (lshift16 < 83)
                        return -77;
                    if (lshift16 < 91)
                        return -76;
                    if (lshift16 < 99)
                        return -75;
                } else {
                    if (lshift16 < 108)
                        return -74;
                    if (lshift16 < 117)
                        return -73;
                    if (lshift16 < 128)
                        return -72;
                    if (lshift16 < 140)
                        return -71;
                    if (lshift16 < 152)
                        return -70;
                    if (lshift16 < 166)
                        return -69;
                    if (lshift16 < 181)
                        return -68;
                }
            } else {
                if (lshift16 < 304) {
                    if (lshift16 < 197)
                        return -67;
                    if (lshift16 < 215)
                        return -66;
                    if (lshift16 < 235)
                        return -65;
                    if (lshift16 < 256)
                        return -64;
                    if (lshift16 < 279)
                        return -63;
                    if (lshift16 < 304)
                        return -62;
                } else {
                    if (lshift16 < 332)
                        return -61;
                    if (lshift16 < 362)
                        return -60;
                    if (lshift16 < 395)
                        return -59;
                    if (lshift16 < 431)
                        return -58;
                    if (lshift16 < 470)
                        return -57;
                    if (lshift16 < 512)
                        return -56;
                    if (lshift16 < 558)
                        return -55;
                }
            }
        }
    } else {
        if (lshift16 < 6317) {
            if (lshift16 < 2048) {
                if (lshift16 < 1117) {
                    if (lshift16 < 609)
                        return -54;
                    if (lshift16 < 664)
                        return -53;
                    if (lshift16 < 724)
                        return -52;
                    if (lshift16 < 790)
                        return -51;
                    if (lshift16 < 861)
                        return -50;
                    if (lshift16 < 939)
                        return -49;
                    if (lshift16 < 1024)
                        return -48;
                    if (lshift16 < 1117)
                        return -47;
                } else {
                    if (lshift16 < 1218)
                        return -46;
                    if (lshift16 < 1328)
                        return -45;
                    if (lshift16 < 1448)
                        return -44;
                    if (lshift16 < 1579)
                        return -43;
                    if (lshift16 < 1722)
                        return -42;
                    if (lshift16 < 1878)
                        return -41;
                    if (lshift16 < 2048)
                        return -40;
                }
            } else {
                if (lshift16 < 3756) {
                    if (lshift16 < 2233)
                        return -39;
                    if (lshift16 < 2435)
                        return -38;
                    if (lshift16 < 2656)
                        return -37;
                    if (lshift16 < 2896)
                        return -36;
                    if (lshift16 < 3158)
                        return -35;
                    if (lshift16 < 3444)
                        return -34;
                    if (lshift16 < 3756)
                        return -33;
                } else {
                    if (lshift16 < 4096)
                        return -32;
                    if (lshift16 < 4467)
                        return -31;
                    if (lshift16 < 4871)
                        return -30;
                    if (lshift16 < 5312)
                        return -29;
                    if (lshift16 < 5793)
                        return -28;
                    if (lshift16 < 6317)
                        return -27;
                }
            }
        } else {
            if (lshift16 < 21247) {
                if (lshift16 < 11585) {
                    if (lshift16 < 6889)
                        return -26;
                    if (lshift16 < 7512)
                        return -25;
                    if (lshift16 < 8192)
                        return -24;
                    if (lshift16 < 8933)
                        return -23;
                    if (lshift16 < 9742)
                        return -22;
                    if (lshift16 < 10624)
                        return -21;
                    if (lshift16 < 11585)
                        return -20;
                } else {
                    if (lshift16 < 12634)
                        return -19;
                    if (lshift16 < 13777)
                        return -18;
                    if (lshift16 < 15024)
                        return -17;
                    if (lshift16 < 16384)
                        return -16;
                    if (lshift16 < 17867)
                        return -15;
                    if (lshift16 < 19484)
                        return -14;
                    if (lshift16 < 21247)
                        return -13;
                }
            } else {
                if (lshift16 < 35734) {
                    if (lshift16 < 23170)
                        return -12;
                    if (lshift16 < 25268)
                        return -11;
                    if (lshift16 < 27554)
                        return -10;
                    if (lshift16 < 30048)
                        return -9;
                    if (lshift16 < 32768)
                        return -8;
                    if (lshift16 < 35734)
                        return -7;
                } else {
                    if (lshift16 < 38968)
                        return -6;
                    if (lshift16 < 42495)
                        return -5;
                    if (lshift16 < 46341)
                        return -4;
                    if (lshift16 < 50535)
                        return -3;
                    if (lshift16 < 55109)
                        return -2;
                    if (lshift16 < 60097)
                        return -1;
                }
            }
        }
    }
    return 0;
}
