#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

//#define DEBUG
#define TERM			(0)
#define V(n)			buf[(n)]
#define NEW(t, n)		malloc((n) * sizeof(t))
#define ABS_TO_S12(x)		((x) % 3 == 1 ? ((x) / 3) : (x) / 3 + n0)
#define S12_TO_LABS(x)		((x) * 3+1)
#define S12_TO_RABS(x)		(((x)-n0)*3+2)
#define S12_IS_LEFT(x)		((x) < n0)
#define S12_TO_ABS(x)		(S12_IS_LEFT(x)?S12_TO_LABS(x):S12_TO_RABS(x))

static inline int
leq2(int a1, int b1, int a2, int b2)
{
	return (a1 < a2 || (a1 == a2 && b1 <= b2));
}

static inline int
leq3(int a1, int b1, int c1, int a2, int b2, int c2)
{
	return (a1 < a2 || (a1 == a2 && leq2(b1, c1, b2, c2)));
}

static void
radix_pass(int *src, int *dst, int size, const int *buf, int space)
{
	int i, n, *c;
	space += 1;
	c = NEW(int, space);
	for (i = 0; i < space; i++)
		c[i] = 0;
	for (i = 0; i < size; i++) {
		int n = V(src[i]);
		++c[n];
	}
	n = 0;
	for (i = 0; i < space; i++) {
		int tmp = c[i];
		c[i] = n;
		n += tmp;
	}
	for (i = 0; i < size; i++) {
		int pos = c[V(src[i])]++;
		dst[pos] = src[i];
	}
	free(c);
	return ;
}

/*
--------->	s0		s1		s2              n0  n1  n2
index
 \|/
123		123		230		300             1   1   1
1234		123,400		234,000		340             2   1   1
12345		123,450		234,500		345		2   2   1
*/

void
dc3_r(const int *buf, int *SA, int size, int space)
{
	int i, lexname, v[3];
	int n0, n1, n2, n12;
	int *p, *SA0, *s0, *SA12, *s12;
	n0 = (2+size) / 3;
	n1 = (2+size - 1) / 3;
	n2 = (2+size - 2) / 3;
	n12 = n0 + n2;
	//sort S1 S2
	s0 = NEW(int, n0);
	SA0 = NEW(int, n0);
	s12 = NEW(int, n12 + 3);
	SA12 = NEW(int, n12 + 3);
	s12[n12] = s12[n12+1] = s12[n12+2] = TERM;
	SA12[n12] = SA12[n12+1] = SA12[n12+2] = TERM;
	p = s12;
	for (i = 0; i < size + (n0 - n1); i++) {
		if (i % 3 != 0)
			*p++ = i;
	}
	assert(p - s12 == n12);
	radix_pass(s12, SA12, n12, buf+2, space); //tri[i+2]
	radix_pass(SA12, s12, n12, buf+1, space); //tri[i+1]
	radix_pass(s12, SA12, n12, buf+0, space); //tri[i+0]
	//build lexname for s12
	lexname = 0;
	s12[n12] = s12[n12 + 1] = s12[n12+2] = TERM;
	v[0] = v[1] = v[2] = -1;
	for (i = 0; i < n12; i++) {
		int idx12;
		int abs= SA12[i];
		if (V(abs) != v[0] || V(abs+1) != v[1] || V(abs+2) != v[2]) {
			++lexname;
			v[0] = V(abs);
			v[1] = V(abs+1);
			v[2] = V(abs+2);
		}
		assert(lexname > 0);
		assert(abs % 3 != 0);
		idx12 = ABS_TO_S12(abs);
		s12[idx12] = lexname;
	}
	assert(s12[n12] == s12[n12 + 1]);
	assert(s12[n12+1] == s12[n12+2] && s12[n12 + 2] == TERM);
	//now s12[idx] = rank which idx % 3 != 0
	if (lexname < n12) { //lexname conflict
		dc3_r(s12, SA12, n12, lexname);
		for (i = 0; i < n12; i++)
			s12[SA12[i]] = i + 1;
	} else {
		for (i = 0; i < n12; i++)
			SA12[s12[i] - 1] = i;
	}
	//sort S0
	p = s0;
	//radix pass 4
	for (i = 0; i < n12; i++) {
		int n = SA12[i];
		if (S12_IS_LEFT(n))
			*p++ = S12_TO_LABS(n) - 1;
	}
	assert(p - s0 == n0);
	radix_pass(s0, SA0, n0, buf, space);
	int k, r0 = 0, r12 = n0 - n1;
	for (k = 0; k < size; k++) {
		int i, j, i12, j12;
		i = SA12[r12];
		j = SA0[r0];
		assert(r12 < n12);
		assert(r0 < n0);
		int leq = 0;
		if (S12_IS_LEFT(i)) {	//k mod 3 == 1, i+n1 ==> k mod 3 == 2
			i = S12_TO_LABS(i);
			i12 = ABS_TO_S12(i+1);
			j12 = ABS_TO_S12(j+1);
			leq = leq2(buf[i], s12[i12], buf[j], s12[j12]);
		} else {	//k mod 3 == 2, k mode 3 + 2 => (k mod 3 == 1)+1
			i = S12_TO_RABS(i);
			i12 = ABS_TO_S12(i+2);
			j12 = ABS_TO_S12(j+2);
			leq = leq3(buf[i], buf[i+1], s12[i12], buf[j], buf[j+1], s12[j12]);
		}
		if (leq) {
			SA[k] = i;
			r12++;
			if (r12 == n12)
				break;
		} else {
			SA[k] = j;
			++r0;
			if (r0 == n0)
				break;
		}
	}
	while (r0 < n0)
		SA[++k] = SA0[r0++];
	while (r12 < n12) {
		int ii = SA12[r12++];
		SA[++k] = S12_TO_ABS(ii);
	}
	//free buffer
	free(s0);
	free(s12);
	free(SA0);
	free(SA12);
	return ;
}


int *
dc3(const uint8_t *dat, int size)
{
	int i, *p;
	int *SA = NEW(int, size);
	int *buf = NEW(int, size+3);
	p = buf;
	for (i = 0; i < size; i++)
		*p++ = dat[i]+1;
	for (i = 0; i < 3; i++)
		*p++ = TERM;
	dc3_r(buf, SA, size, 0xff+1);
	free(buf);
	return SA;
}



