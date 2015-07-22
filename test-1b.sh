#! /bin/sh

# UCLA CS 111 Lab 1b - Test that valid commands are executed correctly.

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
#output 'abc'
echo abc

echo Life is about the journey not the destination > a.txt

#output Life is about the journey not the destination
(cat < a.txt)

(echo where is my mind > b.txt)

#output Life is about the journey not the destination
#output where is my mind
cat a.txt && cat b.txt

(cat asdf.txt && cat b.txt)

#output Life is about the journey not the destination
cat a.txt || cat b.txt

#output where is my mind
(cat asdf.txt || cat b.txt)

#output apple
echo apple; echo pear > p.txt

#output pear
#output where is my mind
(cat asdf.txt || cat p.txt); cat b.txt

#output a.txt
ls | grep a

(echo its a doggy dog world out there) > c.txt

#output intercepted
echo intercepted

#output its a doggy dog world out there
cat c.txt
EOF

cat >test.exp <<'EOF'
abc
Life is about the journey not the destination
Life is about the journey not the destination
where is my mind
Life is about the journey not the destination
where is my mind
apple
pear
where is my mind
a.txt
intercepted
its a doggy dog world out there
EOF

../timetrash test.sh >test.out 2>test.err || exit

diff -u test.exp test.out || exit

) || exit

rm -fr "$tmp"
