#!/bin/bash 
HOSTDIR="/afs/nd.edu/user21/jwesthof/Public" 
uut="./simplefs" 
cat <<EOF 
RUNNING LAME TEST SCRIPT VERSION 7 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
Please note that this is not a comprehensive test, it just tests 
a couple of corner cases. If you have any questions, comments, concerns, 
suggestions, or whatever, bug @jeffrey on slack, or John Westhoff on facebook. 
 
EOF 
tmp=`mktemp -d` 
chmod 777 $tmp 
cp $HOSTDIR/test/image.5 $tmp/img.5 

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
