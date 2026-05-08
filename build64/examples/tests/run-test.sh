echo 'testing stack'
./test-runner stack eyrie-rt --retval 12345
echo 'testing loop'
./test-runner loop eyrie-rt --retval 54321
echo 'testing malloc'
./test-runner malloc eyrie-rt --retval 11411
echo 'testing long-nop'
./test-runner long-nop eyrie-rt --retval 12345
echo 'testing fibonacci'
./test-runner fibonacci eyrie-rt --retval 14930352
echo 'testing fib-bench'
./test-runner fib-bench eyrie-rt
echo 'testing attestation'
./test-runner attestation eyrie-rt --retval 0
echo 'testing untrusted'
./test-runner untrusted eyrie-rt --retval 13
echo 'testing data-sealing'
./test-runner data-sealing eyrie-rt --retval 0
