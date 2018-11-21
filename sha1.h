/*
 * SHA1 routine optimized to do word accesses rather than byte accesses,
 * and to avoid unnecessary copies into the context array.
 *
 * This was initially based on the Mozilla SHA1 implementation, although
 * none of the original Mozilla code remains.
 */

typedef struct {
	unsigned long long size;
	unsigned int H[5];
	unsigned int W[16];
} SHA1_CTX;

void SHA1_Init(SHA1_CTX *ctx);
void SHA1_Update(SHA1_CTX *ctx, const void *dataIn, unsigned long len);
void SHA1_Final(unsigned char hashout[20], SHA1_CTX *ctx);

