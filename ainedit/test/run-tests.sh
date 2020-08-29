#!/bin/sh

cd $(dirname "$0")

./test-runner.sh main.jaf

# integer ops
./test-runner.sh add.jaf
./test-runner.sh sub.jaf
./test-runner.sh mul.jaf
./test-runner.sh div.jaf
./test-runner.sh mod.jaf
./test-runner.sh seq.jaf
./test-runner.sh lshift.jaf
./test-runner.sh rshift.jaf
./test-runner.sh bitand.jaf
./test-runner.sh bitior.jaf
./test-runner.sh bitxor.jaf
./test-runner.sh lt.jaf
./test-runner.sh gt.jaf
./test-runner.sh lte.jaf
./test-runner.sh gte.jaf
./test-runner.sh eq.jaf
./test-runner.sh neq.jaf
./test-runner.sh and.jaf
./test-runner.sh or.jaf
./test-runner.sh plusa.jaf
./test-runner.sh minusa.jaf
./test-runner.sh mula.jaf
./test-runner.sh diva.jaf
./test-runner.sh moda.jaf
./test-runner.sh lshifta.jaf
./test-runner.sh rshifta.jaf
./test-runner.sh anda.jaf
./test-runner.sh iora.jaf
./test-runner.sh xora.jaf

# floating point ops
./test-runner.sh f-add.jaf
./test-runner.sh f-sub.jaf
./test-runner.sh f-mul.jaf
./test-runner.sh f-div.jaf
./test-runner.sh f-lt.jaf
./test-runner.sh f-gt.jaf
./test-runner.sh f-lte.jaf
./test-runner.sh f-gte.jaf
./test-runner.sh f-eq.jaf
./test-runner.sh f-neq.jaf
./test-runner.sh f-plusa.jaf
./test-runner.sh f-minusa.jaf
./test-runner.sh f-mula.jaf
./test-runner.sh f-diva.jaf

# string ops
./test-runner.sh s-add.jaf
./test-runner.sh s-lt.jaf
./test-runner.sh s-gt.jaf
./test-runner.sh s-lte.jaf
./test-runner.sh s-gte.jaf
./test-runner.sh s-eq.jaf
./test-runner.sh s-neq.jaf
./test-runner.sh string-constant.jaf
#./test-runner.sh s-length.jaf
#./test-runner.sh s-lengthbyte.jaf
#./test-runner.sh s-empty.jaf
#./test-runner.sh s-find.jaf
#./test-runner.sh s-getpart.jaf
#./test-runner.sh s-pushback.jaf
#./test-runner.sh s-popback.jaf
#./test-runner.sh s-erase.jaf
#./test-runner.sh s-mod.jaf
#./test-runner.sh s-from-int.jaf
#./test-runner.sh s-from-float.jaf
#./test-runner.sh s-to-int.jaf

# arrays
./test-runner.sh array-int.jaf
./test-runner.sh array-string.jaf
./test-runner.sh array-arg.jaf
./test-runner.sh array-multi.jaf
./test-runner.sh ref-array-item.jaf
./test-runner.sh ref-array-int.jaf

# structs
./test-runner.sh struct-arg.jaf
./test-runner.sh ref-struct.jaf
./test-runner.sh local-ref-struct.jaf

# control flow
./test-runner.sh if.jaf
./test-runner.sh if-else.jaf
./test-runner.sh while.jaf
./test-runner.sh do-while.jaf
./test-runner.sh for.jaf

# reference types
./test-runner.sh int-ref.jaf
./test-runner.sh local-ref-int.jaf
./test-runner.sh float-ref.jaf
./test-runner.sh string-ref.jaf
./test-runner.sh local-ref-string.jaf

./test-runner.sh functype.jaf
./test-runner.sh functype-void.jaf
./test-runner.sh functype-ref-int.jaf

./test-runner.sh message.jaf
./test-runner.sh message-call.jaf

./test-runner.sh char-constant.jaf
