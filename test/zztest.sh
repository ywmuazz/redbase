rm in.txt out.txt tb*
./mkd $1
mv tb* in.txt
rm -r mdb
cd ..
make all
cp tester ./test
cp dbcreate ./test/
cd ./test
#rm -r mdb
./dbcreate mdb
# ./tester mdb
time ./tester mdb
#/usr/bin/time -v
