extern int run_fconv2d(void);
extern int run_fmatmul(void);

int main() {
  run_fconv2d();
  run_fmatmul();
  for (;;)
    ;
}
