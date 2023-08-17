#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./9cc "int main(){$input}" > tmp.s
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

  ./9cc "int main(){$input}" > tmp.s
  cc -o tmp tmp.s calltest.c
  stdout_text=$(./tmp)
  actual="$?"

  if [ "$actual" = "$expected_code" ]; then
    if [ "$stdout_text" = "$expected_stdout" ]; then
      echo "$input => $actual, $stdout_text"
    else
      echo "code: $input => $actual"
      echo "stdout: $expected_stdout expected, but got $stdout_text"
      exit 1
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
assert 14 'int a;a=7;a*2;'
assert 20 'int a;a=10;a*2;'
assert 30 'int abc;abc=10;abc*3;'
assert 2 'int abc;int def;int ghi;abc=10;def=2*1;ghi=4+1;abc/ghi;'
assert 5 'int abc;int def;int ghi;abc=10;def=2*1;return ghi=4+1;abc/ghi;'
assert 20 'int a;int b;if(1){a=10;b=2;}else{a=1;b=9;}return a*b;'
assert 9 'int a;int b;if(0){a=10;b=2;}else{a=1;b=9;}return a*b;'
assert 20 'int a;int b;b=2;if(1)a=10;else{a=1;b=9;}return a*b;'
assert 27 'int b;int a;b=2;if(b==1)a=10;else{a=1;b=9;if(a=9){b=3;}}return a*b;'
assert 45 'int a;int b;b=0;for(a=0;a<10;a=a+1){b=b+a;}b;'
assert 0 'int a;int b;b=0;for(a=0;a==1;a=a+1){b=1;}b;'
assert 31 "int a;int b;a=0;b=32;while(b>0){b=b/2;a=a+b;}a;"
assert 10 "int a;a=0;do a=a+2; while(a<9);a;"
assert_stdout 3 "testfunc1 called!" "testfunc1();"
assert_stdout 16 "testfunc2,7,9 called!" "testfunc2(1+6,9);"
assert_file 34 "int tra(int a){if(a==1){return 1;}if(a==2){return 2;}return tra(a-1)+tra(a-2);}int main(){return tra(8);}"
assert_file 3 "int main(){int a;int *b;a=2;b=&a;a=3;return *b;}"
assert_stdout 14 "testfunc3,10,4 called!" "int *c;c=malloc(30);*c=10;*(c+1)=4;testfunc3(c);"
assert_stdout 7 "$(echo -e "testfunc3,1,2 called!\ntestfunc3,3,4 called!")" "int **c;c=malloc(30);int *a;a=malloc(16);int *b;b=malloc(16);*c=a;*(c+1)=b;**c=1;*(a+1)=2;**(c+1)=3;*(b+1)=4;testfunc3(a);testfunc3(b);"
assert_file 5 "int main(){int *a; 1+sizeof (*a);}"
assert_file 9 "int main(){int *a; 1+sizeof (a);}"
assert_file 9 "int main(){int *a; 1+sizeof (a+1);}"
assert_file 9 "int main(){int *a; 1+sizeof (a-1);}"
assert_file 5 "int main(){int *a; 1+sizeof (*a-1);}"
assert_file 5 "int main(){int *a;int *b; 1+sizeof (a-b);}"
assert_file 36 "int main(){int a[9];sizeof(a);}"
assert_file 2 "int main(){int *a;int b;a=malloc(32);*a=1;*(a+1)=2;b=*(a+1);}"
assert_file 2 "int main(){int *a;int b;int *c;a=malloc(32);c=a+2;*a=1;*(a+1)=2;b=*(a+1);c-a;}"
assert_file 7 "int main(){int *a;int b;int *c;a=malloc(32);c=a+2;*a=7;*(a+1)=2;b=*(a+1);*(c-2);}"
assert_stdout 8 "" "int *a;int b;int *c;a=malloc(32);c=a+2;return natural_sub(c,a);"
assert_file 3 "int main(){int a[2];*a=1;*(a+1)=2;return *a+*(a+1);}"
assert_file 1 "int main(){int a[2];*a=1;int c;c=a[0];}"
assert_file 3 "int main(){int a[2];*a=1;*(a+1)=3;int c;c=a[1];}"
assert_file 4 "int a;int b[10];int main(){a=1;b[0]=a;b[1]=b[0]+3;b[1];}"
assert_file 2 "char a;int main(){char b;b=2;a=b;a;}"
assert_file 2 "char a;char b;int main(){a=2;b=3;a;}"
assert_file 4 "char foo(char a){return a+1;}int main(){char a;a=2;return foo(a+1);}"
assert_file 1 "int main(){char a;return sizeof(a);}"
assert_file 4 "int main(){char a;return sizeof(a+1);}"
assert_file 7 "int main(){char a[7];return sizeof(a);}"
assert_file 99 "int main(){char *a;a=\"abc\";a[2];}"
assert_file 100 "int main(){char *b;char *a;a=\"abc\";b=\"def\";b[0];}"
assert_stdout 12 "Hello world!" "printf(\"Hello world!\");"

echo OK
