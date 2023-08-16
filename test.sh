#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./9cc "main(){$input}" > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert_file() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  cc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert_stdout() {
  expected_code="$1"
  expected_stdout="$2"
  input="$3"

  ./9cc "main(){$input}" > tmp.s
  cc -o tmp tmp.s calltest.c
  stdout_text=$(./tmp)
  actual="$?"

  if [ "$actual" = "$expected_code" ]; then
    if [ "$stdout_text" = "$expected_stdout" ]; then
      echo "$input => $actual, $stdout_text"
    else
      echo "code: $input => $actual"
      echo "stdout: $expected_stdout expected, but got $stdout_text"
    fi
  else
    echo "$input => $expected_code expected, but got $actual"
    exit 1
  fi
}

assert 0 "0;"
assert 42 "42;"
assert 21 "5+20-4;"
assert 41 " 12 + 34 - 5 ;"
assert 7 "1+2*3;"
assert 9 "(1+2)*3;"
assert 47 '5+6*7;'
assert 15 '5*(9-6);'
assert 4 '(3+5)/2;'
assert 4 '(3+5)/+2;'
assert 6 '-5*+2+16;'
assert 1 '10==10;'
assert 0 '10!=10;'
assert 1 '9<10;'
assert 0 '9<9;'
assert 0 '10<9;'
assert 1 '9<=10;'
assert 1 '9<=9;'
assert 0 '10<=9;'
assert 1 '10<=9==20<=19;'
assert 0 '1*1==20<=19;'
assert 1 '1*0==20<=19;'
assert 14 'a=7;a*2;'
assert 20 'a=10;a*2;'
assert 30 'abc=10;abc*3;'
assert 2 'abc=10;def=2*1;ghi=4+1;abc/ghi;'
assert 5 'abc=10;def=2*1;return ghi=4+1;abc/ghi;'
assert 20 'if(1){a=10;b=2;}else{a=1;b=9;}return a*b;'
assert 9 'if(0){a=10;b=2;}else{a=1;b=9;}return a*b;'
assert 20 'b=2;if(1)a=10;else{a=1;b=9;}return a*b;'
assert 27 'b=2;if(b==1)a=10;else{a=1;b=9;if(a=9){b=3;}}return a*b;'
assert 45 'b=0;for(a=0;a<10;a=a+1){b=b+a;}b;'
assert 0 'b=0;for(a=0;a==1;a=a+1){b=1;}b;'
assert 31 "a=0;b=32;while(b>0){b=b/2;a=a+b;}a;"
assert 10 "a=0;do a=a+2; while(a<9);a;"
assert_stdout 3 "testfunc1 called!" "testfunc1();"
assert_stdout 16 "testfunc2,7,9 called!" "testfunc2(1+6,9);"
assert_file 34 "tra(a){if(a==1){return 1;}if(a==2){return 2;}return tra(a-1)+tra(a-2);}main(){return tra(8);}"

echo OK
