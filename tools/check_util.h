#ifndef CHECK_UTIL_H
#define CHECK_UTIL_H

void check_expect(int ok, const char *msg);
int check_near(float a, float b, float tol);
int check_failed(void);

#endif
