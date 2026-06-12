#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void handler(int sig)
{
    asm volatile(
        "li t0, 222\n"
        "vmv.v.x v1, t0\n"
    );
}

int main()
{
    signal(SIGALRM, handler);

    alarm(1);

    asm volatile(
        "li t0, 111\n"
        "vmv.v.x v1, t0\n"
    );

    while (1) {
        long x;

        asm volatile(
            "vmv.x.s %0, v1"
            : "=r"(x)
        );

        printf("%ld\n", x);
    }
}
