#! /bin/sh

# UCLA CS 111 Lab 1 - Test that valid syntax is processed correctly.

tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test.sh <<'EOF'
a
b

a

b

a ||
b

a && b || c | d

ls -l | grep s

cat < 1.txt > 2.txt

sed -r s/a+b/c/g < 1.txt > 2.txt

(
a ||
b
)

((cat a.txt)
(cat b.txt))
EOF

cat >test.exp <<'EOF'
# 1
    a \
  ;
    b
# 2
  a
# 3
  b
# 4
    a \
  ||
    b
# 5
      a \
    &&
      b \
  ||
      c \
    |
      d
# 6
    ls -l \
  |
    grep s
# 7
  cat<1.txt>2.txt
# 8
  sed -r s/a+b/c/g<1.txt>2.txt
# 9
  (
     a \
   ||
     b
  )
# 10
  (
     (
      cat a.txt
     ) \
   ;
     (
      cat b.txt
     )
  )
EOF

../timetrash -p test.sh >test.out 2>test.err || exit

diff -u test.exp test.out || exit
test ! -s test.err || {
  cat test.err
  exit 1
}

) || exit

rm -fr "$tmp"
