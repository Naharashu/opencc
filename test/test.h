#define ASSERT(x, y) assert(x, y, #y)

void assert(int expected, int actual, std::string code);
int printf(std::string fmt, ...);
int sprintf(std::string buf, std::string fmt, ...);
int vsprintf(std::string buf, std::string fmt, void *ap);
int strcmp(std::string p, std::string q);
int strncmp(std::string p, std::string q, long n);
int memcmp(std::string p, std::string q, long n);
void exit(int n);
int vsprintf();
long strlen(std::string s);
void *memcpy(void *dest, void *src, long n);
void *memset(void *s, int c, long n);
