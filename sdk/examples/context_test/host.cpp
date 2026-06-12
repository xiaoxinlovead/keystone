#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int v_available = 1;

void sigill_handler(int sig) {
    write(2, "SIGILL: V extension not available\n", 35);
    v_available = 0;
}

void handler(int sig)
{
    write(2, "H\n", 2);
    if (v_available) {
        asm volatile(
            "li t0, 222\n"
            "vmv.v.x v1, t0\n"
        );
    }
}

int main()
{
    /* Catch SIGILL to detect if V is not available */
    signal(SIGILL, sigill_handler);

    /* Test if V works */
    signal(SIGALRM, handler);
    alarm(1);

    if (v_available) {
        asm volatile(
            "li t0, 111\n"
            "vmv.v.x v1, t0\n"
        );
        /* Read back to verify V works */
        long check;
        asm volatile("vmv.x.s %0, v1" : "=r"(check));
        write(2, "V_OK v1=", 8);
        char c[8];
        int n = 0;
        if (check >= 100) { c[n++] = '0' + check/100; check %= 100; }
        if (check >= 10) { c[n++] = '0' + check/10; check %= 10; }
        c[n++] = '0' + check;
        c[n++] = '\n';
        write(2, c, n);
    } else {
        write(2, "V not available, exiting\n", 25);
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        long x;
        asm volatile("vmv.x.s %0, v1" : "=r"(x));
        printf("%ld\n", x);
        sleep(1);
    }
    return 0;
}
