/* Minimal, non-vector memset/memcpy/memcmp — no library dependencies */
__attribute__((used))
void *memset(void *s, int c, unsigned long n) {
  unsigned char *p = (unsigned char*)s;
  while (n--) *p++ = (unsigned char)c;
  return s;
}

__attribute__((used))
void *memcpy(void *d, const void *s, unsigned long n) {
  unsigned char *dp = (unsigned char*)d;
  const unsigned char *sp = (const unsigned char*)s;
  while (n--) *dp++ = *sp++;
  return d;
}

__attribute__((used))
int memcmp(const void *s1, const void *s2, unsigned long n) {
  const unsigned char *p1 = (const unsigned char*)s1;
  const unsigned char *p2 = (const unsigned char*)s2;
  while (n--) { if (*p1 != *p2) return *p1 - *p2; p1++; p2++; }
  return 0;
}
