#!/bin/bash
HOSTDIR="/afs/nd.edu/user21/jwesthof/Public"
uut="./simplefs image.20 20"
output="out.txt"
txt1="medium.txt"
txt2="less_big2.txt"
cat <<EOF
RUNNING LAME TEST SCRIPT VERSION 7
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Please note that this is not a comprehensive test, it just tests
a couple of corner cases. If you have any questions, comments, concerns,
suggestions, or whatever, bug @jeffrey on slack, or John Westhoff on facebook.

EOF
tmp=`mktemp -d`
chmod 777 $tmp

### TEST ONE ###
$uut >/dev/null <<EOF
mount
copyout 2 $out.txt
EOF
## Verify test succeeded ##
if diff $tmp/1.txt $HOSTDIR/test/1.txt ; then
    echo "TEST ONE GOOD - Simple copyout works"
else
    echo "TEST ONE FAIL - Copying out a file failed"
fi

### TEST TWO ###
$uut $tmp/test.5 5 > /dev/null <<EOF
format
mount
create
create
create
copyin $HOSTDIR/test/hi 1
copyin $HOSTDIR/test/hi 2
copyin $HOSTDIR/test/hi 3
copyout 1 $tmp/1.txt
copyout 2 $tmp/2.txt
copyout 3 $tmp/3.txt
cat 3
EOF
if diff $tmp/1.txt $HOSTDIR/test/hi > /dev/null && \
   diff $tmp/2.txt $HOSTDIR/test/hi > /dev/null && \
   diff $tmp/3.txt $HOSTDIR/test/hi > /dev/null ; then
    echo "TEST TWO GOOD - Create and Copyout work"
else
    echo "TEST TWO FAIL - Copying in and out three files failed"
fi

### TEST THREE ###
$uut $tmp/test.5 5 > /dev/null <<EOF
format
mount
create
create
create
create
copyin $HOSTDIR/test/hi 1
copyin $HOSTDIR/test/hi 2
copyin $HOSTDIR/test/hi 3
copyin $HOSTDIR/test/hi 4
copyout 1 $tmp/1.txt
copyout 2 $tmp/2.txt
copyout 3 $tmp/3.txt
copyout 4 $tmp/4.txt
EOF
if diff $tmp/1.txt $HOSTDIR/test/hi > /dev/null && \
   diff $tmp/2.txt $HOSTDIR/test/hi > /dev/null && \
   diff $tmp/3.txt $HOSTDIR/test/hi > /dev/null && \
   [ ! -s $tmp/4.txt ] ; then
    echo "TEST THREE GOOD - Filling the disk and then copying out worked"
else
    echo "TEST THREE FAIL - Filling the disk failed and copyin/out failed"
fi

### TEST FOUR ###
$uut $tmp/img.1222 1222 > /dev/null <<EOF
format
mount
create
copyin $HOSTDIR/test/big 1
copyout 1 $tmp/big
debug
EOF
## Verify test succeeded ##
if diff $tmp/big $HOSTDIR/test/big > /dev/null ; then
    echo "TEST FOUR GOOD - Copying in and out a really big file worked"
else
    echo "TEST FOUR FAIL - Filling a really big file failed"
fi

### TEST FIVE ###
$uut $tmp/img.1222 1222 > /dev/null <<EOF
format
mount
create
copyin $HOSTDIR/test/big 1
delete 1
create
copyin $HOSTDIR/test/big 1
copyout 1 $tmp/big
debug
EOF
## Verify test succeeded ##
if diff $tmp/big $HOSTDIR/test/big > /dev/null ; then
    echo "TEST FIVE GOOD - Copying in, deleting, and recreating a big file"
else
    echo "TEST FIVE FAIL - Filling and deleting a really big file failed"
fi

### TEST SIX ##
$uut $tmp/img.11 11 > /dev/null <<EOF
format
mount
`for i in {1..128} ; do echo "create" ; done`
copyin $HOSTDIR/test/hi 128
copyout 128 $tmp/hi2
debug
EOF
## Verify test succeeded ##
if diff $tmp/hi2 $HOSTDIR/test/hi > /dev/null ; then
    echo "TEST SIX GOOD - Creating many inodes and copying in/out works"
else
    echo "TEST SIX FAIL - Creating many inodes and copying out failed"
fi

### TEST SEVEN ##
$uut $tmp/img.10 10 > /dev/null <<EOF
format
mount
`for i in {1..128} ; do echo "create" ; done`
copyin $HOSTDIR/test/hi 128
copyout 128 $tmp/how
debug
EOF
## Verify test succeeded ##
if [ ! -s $tmp/how ] ; then
    echo "TEST SEVEN GOOD - Creating too many inodes seems to be handled well"
else
    echo "TEST SEVEN FAIL - Creating too many inodes failed"
fi

### TEST EIGHT ##
T8A=`$uut $tmp/img.create 10 <<EOF
format
mount
debug
EOF`
T8B=`$uut $tmp/img.create 10 <<EOF
format
mount
create
debug
EOF`
## Verify test succeeded ##
if [ `echo $T8A | wc -w` -lt `echo $T8B | wc -w` ] ; then
    echo "TEST EIGHT GOOD - Create does something"
else
    echo "TEST EIGHT FAIL - Create Failed"
fi

### TEST NINE ##
T9A=`$uut $tmp/img.create 10 <<EOF
format
mount
debug
EOF`
T9B=`$uut $tmp/img.create 10 <<EOF
format
mount
create
create
debug
EOF`
T9C=`$uut $tmp/img.create 10 <<EOF
format
mount
create
create
delete 2
debug
EOF`
## Verify test succeeded ##
if [ `echo $T9A | wc -w` -lt `echo $T9C | wc -w` ] && 
   [ `echo $T9C | wc -w` -lt `echo $T9B | wc -w` ] ; then
    echo "TEST NINE GOOD - Create create delete seems to work"
else
    echo "TEST NINE FAIL - Create create delete failed"
fi

### TEST TEN ##
$uut $tmp/img.create 10 > /dev/null <<EOF
format
EOF
T10A=`$uut $tmp/img.create 10 <<EOF
mount
debug
EOF`
$uut $tmp/img.create 10 > /dev/null <<EOF
mount
create
create
delete 1
delete 2
debug
EOF
T10B=`$uut $tmp/img.create 10 <<EOF
mount
debug
EOF`
## Verify test succeeded ##
echo $T10A
echo $T10B
if [ "$T10A" = "$T10B" ] ; then
    echo "TEST TEN GOOD - Create create delete delete seems to work"
else
    echo "TEST TEN FAIL - Create create delete delete failed"
fi

### TEST 11 ##
$uut $tmp/img.defrag 2000 > /dev/null <<EOF
format
mount
`for i in {1..384} ; do echo "create" ; done`
`for i in {2..384..2} ; do echo "copyin $HOSTDIR/test/hi $i" ; done`
copyin $HOSTDIR/test/big 1
defrag
copyout 1 $tmp/biggest
EOF
## Verify test succeeded ##
if diff $tmp/biggest $HOSTDIR/test/big > /dev/null ; then
    echo "TEST 11 GOOD - defrag doesn't break anything"
else
    echo "TEST 11 FAIL - defrag breaks something"
fi
