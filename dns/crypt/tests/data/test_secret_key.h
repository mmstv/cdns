/****
	/usr/bin/hexdump -e '14/1 "%#4x, " "\n"' crypt_secret.key > test_secret_key.h
****/
const uint8_t test_secret_key[] = {
0x53, 0xa0, 0x83, 0x37, 0x62, 0xbd, 0x1c, 0x80, 0x2c, 0x99, 0x93, 0xed, 0x67, 0x66,
0x74, 0x6d, 0x2d, 0x56, 0x7f, 0x27, 0xad,  0x1, 0xb4, 0x45, 0xc1, 0xaa, 0x1a, 0xf5,
0xc0, 0x5e, 0x6c, 0x9f
};
