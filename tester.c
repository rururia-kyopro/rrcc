char *fopen(char *filename, char *mode);
char *fwrite(char *buf, int len, int n, char *fp);
char *fread(char *buf, int len, int n, char *fp);
char *fclose(char *fp);
char *system(char *cmd);
int strlen(char *s);
int strcmp(char *s, char *t);
int printf(char *s);
int memset(char *s, int b, int len);
int exit(int a);

int compile(char *prefix, char *source, char *suffix) {
    int ret;
    int *fp;
    fp = fopen("tmp.c", "w");
    fwrite(prefix, strlen(prefix), 1, fp);
    fwrite(source, strlen(source), 1, fp);
    fwrite(suffix, strlen(suffix), 1, fp);
    fclose(fp);
    ret = system("./rrcc -I/usr/include -I./include tmp.c > tmp.s");
    if(ret != 0) {
        return ret;
    }
    ret = system("cc -o tmp tmp.s calltest.c");
    if(ret != 0) {
        return ret;
    }
}

int assert(int expected, char *source) {
    if(compile("int main(){", source, "}")) {
        printf("Compile failed %s\n", source);
        exit(1);
    }

    int ret;
    ret = system("./tmp");
    if(ret == -1) {
        printf("Execution failed. %s\n", source);
        exit(1);
    }
    ret = ret / 256;

    if(ret == expected) {
        printf("%s => %d\n", source, ret);
    }else{
        printf("%s => %d expected, but got %d\n", source, expected, ret);
        exit(1);
    }
}

int assert_file(int expected, char *source) {
    if(compile("", source, "")) {
        printf("Compile failed %s\n", source);
        exit(1);
    }

    int ret;
    ret = system("./tmp");
    if(ret == -1) {
        printf("Execution failed. %s\n", source);
        exit(1);
    }
    ret = ret / 256;

    if(ret == expected) {
        printf("%s => %d\n", source, ret);
    }else{
        printf("%s => %d expected, but got %d\n", source, expected, ret);
        exit(1);
    }
}

int assert_compile_fail(char *source) {
    if(compile("", source, "")) {
        printf("OK: Compile failed %s\n", source);
        return 0;
    }
    printf("%s => 'failed compilation' expected, but succeeded\n", source);
    exit(1);
}

int assert_stdout(int expected, char *str, char *source) {
    if(compile("", source, "")) {
        printf("Compile failed %s\n", source);
        exit(1);
    }
    int ret;
    ret = system("./tmp > tmp.output");
    if(ret == -1) {
        printf("Execution failed. %s\n", source);
        exit(1);
    }
    ret = ret / 256;

    if(ret == expected) {
        int *fp;
        char buf[1000];
        memset(buf, 0, 1000);
        fp = fopen("tmp.output", "r");
        fread(buf, 1000, 1, fp);
        fclose(fp);
        if(strcmp(buf, str) == 0) {
            printf("%s => %d, %s\n", source, ret, str);
        }else{
            printf("%s => %d, '%s' expected, but got '%s'\n", source, expected, str, buf);
            exit(1);
        }
    }else{
        printf("%s => %d expected, but got %d\n", source, expected, ret);
        exit(1);
    }
}

int assert_file_inc(int expected, char *source, char *inc_content) {
    int *fp;
    fp = fopen("tmpinc.h", "w");
    fwrite(inc_content, strlen(inc_content), 1, fp);
    fclose(fp);

    if(compile("", source, "")) {
        printf("Compile failed %s\n", source);
        exit(1);
    }

    int ret;
    ret = system("./tmp");
    if(ret == -1) {
        printf("Execution failed. %s\n", source);
        exit(1);
    }
    ret = ret / 256;

    if(ret == expected) {
        printf("%s => %d\n", source, ret);
    }else{
        printf("%s => %d expected, but got %d\n", source, expected, ret);
        exit(1);
    }
}


int main() {
    assert(0, "0;");
    assert(42, "42;");
    assert(21, "5+20-4;");
    assert(41, " 12 + 34 - 5 ;");
    assert(7, "1+2*3;");
    assert(9, "(1+2)*3;");
    assert(47, "5+6*7;");
    assert(15, "5*(9-6);");
    assert(4, "(3+5)/2;");
    assert(4, "(3+5)/+2;");
    assert(6, "-5*+2+16;");
    assert(1, "10==10;");
    assert(0, "10!=10;");
    assert(1, "9<10;");
    assert(0, "9<9;");
    assert(0, "10<9;");
    assert(1, "9<=10;");
    assert(1, "9<=9;");
    assert(0, "10<=9;");
    assert(1, "10<=9==20<=19;");
    assert(0, "1*1==20<=19;");
    assert(1, "1*0==20<=19;");
    assert(14, "int a;a=7;a*2;");
    assert(20, "int a;a=10;a*2;");
    assert(30, "int abc;abc=10;abc*3;");
    assert(2, "int abc;int def;int ghi;abc=10;def=2*1;ghi=4+1;abc/ghi;");
    assert(5, "int abc;int def;int ghi;abc=10;def=2*1;return ghi=4+1;abc/ghi;");
    assert(20, "int a;int b;if(1){a=10;b=2;}else{a=1;b=9;}return a*b;");
    assert(9, "int a;int b;if(0){a=10;b=2;}else{a=1;b=9;}return a*b;");
    assert(20, "int a;int b;b=2;if(1)a=10;else{a=1;b=9;}return a*b;");
    assert(27, "int b;int a;b=2;if(b==1)a=10;else{a=1;b=9;if(a=9){b=3;}}return a*b;");
    assert(45, "int a;int b;b=0;for(a=0;a<10;a=a+1){b=b+a;}b;");
    assert(0, "int a;int b;b=0;for(a=0;a==1;a=a+1){b=1;}b;");
    assert(31, "int a;int b;a=0;b=32;while(b>0){b=b/2;a=a+b;}a;");
    assert(10, "int a;a=0;do a=a+2; while(a<9);a;");
    assert_stdout(3, "testfunc1 called!\n", "int testfunc1();int main(){testfunc1();}");
    assert_stdout(16, "testfunc2,7,9 called!\n", "int testfunc2();int main(){testfunc2(1+6,9);}");
    assert_file(34, "int tra(int a){if(a==1){return 1;}if(a==2){return 2;}return tra(a-1)+tra(a-2);}int main(){return tra(8);}");
    assert_file(3, "int main(){int a;int *b;a=2;b=&a;a=3;return *b;}");
    assert_stdout(14, "testfunc3,10,4 called!\n", "char *malloc(int size);int testfunc3(int *c);int main(){int *c;c=malloc(30);*c=10;*(c+1)=4;testfunc3(c);}");
    assert_stdout(7, "testfunc3,1,2 called!\ntestfunc3,3,4 called!\n", "char *malloc(int size);int testfunc3(int *c);int main(){int **c;c=malloc(30);int *a;a=malloc(16);int *b;b=malloc(16);*c=a;*(c+1)=b;**c=1;*(a+1)=2;**(c+1)=3;*(b+1)=4;testfunc3(a);testfunc3(b);}");
    assert_file(5, "int main(){int *a; 1+sizeof (*a);}");
    assert_file(9, "int main(){int *a; 1+sizeof (a);}");
    assert_file(9, "int main(){int *a; 1+sizeof (a+1);}");
    assert_file(9, "int main(){int *a; 1+sizeof (a-1);}");
    assert_file(5, "int main(){int *a; 1+sizeof (*a-1);}");
    assert_file(9, "int main(){int *a;int *b; 1+sizeof (a-b);}");
    assert_file(36, "int main(){int a[9];sizeof(a);}");
    assert_file(2, "char *malloc(int size);int main(){int *a;int b;a=malloc(32);*a=1;*(a+1)=2;b=*(a+1);}");
    assert_file(2, "char *malloc(int size);int main(){int *a;int b;int *c;a=malloc(32);c=a+2;*a=1;*(a+1)=2;b=*(a+1);c-a;}");
    assert_file(7, "char *malloc(int size);int main(){int *a;int b;int *c;a=malloc(32);c=a+2;*a=7;*(a+1)=2;b=*(a+1);*(c-2);}");
    assert_stdout(8, "", "char *malloc(int size);int natural_sub(int *a,int *c);int main(){int *a;int b;int *c;a=malloc(32);c=a+2;return natural_sub(c,a);}");
    assert_file(3, "int main(){int a[2];*a=1;*(a+1)=2;return *a+*(a+1);}");
    assert_file(1, "int main(){int a[2];*a=1;int c;c=a[0];}");
    assert_file(3, "int main(){int a[2];*a=1;*(a+1)=3;int c;c=a[1];}");
    assert_file(4, "int a;int b[10];int main(){a=1;b[0]=a;b[1]=b[0]+3;b[1];}");
    assert_file(2, "char a;int main(){char b;b=2;a=b;a;}");
    assert_file(2, "char a;char b;int main(){a=2;b=3;a;}");
    assert_file(4, "char foo(char a){return a+1;}int main(){char a;a=2;return foo(a+1);}");
    assert_file(1, "int main(){char a;return sizeof(a);}");
    assert_file(4, "int main(){char a;return sizeof(a+1);}");
    assert_file(7, "int main(){char a[7];return sizeof(a);}");
    assert_file(99, "int main(){char *a;a=\"zbc\";a[2];}");
    assert_file(122, "int main(){char *b;char *a;a=\"zbc\";b=\"zef\";b[0];}");
    assert_stdout(12, "Hello world!", "int printf(char*f);int main(){printf(\"Hello world!\");}");
    assert_file(2, "int main(){int a;a=1/*bb b */+1;return a;}");
    assert_file(2, "int main(){// test comment\nint a;a=1\n///*bb b */\n+1;return a;}");
    assert_file(97, "char a[2]=\"/\";int main(){char b[4]=\"g23\";a[0]+b[1];}");
    assert_file(20, "int a[2]={10,20};int main(){return a[1];}");
    assert_file(8, "int fun(){return 4;}int main(){int a[2]={fun(),fun()};return a[0]+a[1];}");
    assert_file(5, "int fun(){return 4;}int main(){int a=fun();return a+1;}");
    assert_file(40, "int *a[5];int main(){return sizeof(a);}");
    assert_file(8, "int (*a)[5];int main(){return sizeof(a);}");
    assert_file(1, "int (*a)[5];int main(){int b[5];a=&b;b[0]=2;return *a == b;}");
    assert_file(16, "int (*a[2])[5];int main(){return sizeof(a);}");
    assert_file(1, "int func(int a);int main(){return 1;}");
    assert_file(1, "struct a{};int main(){return 1;}");
    assert_file(1, "struct a{}b;int main(){return 1;}");
    assert_file(21, "struct aa{int f;int q;int m;int *g;char p;}p;int main(){return sizeof(p);}");
    assert_file(8, "int printf(char *s);struct st{int a;int b;}c;int main(){char *p=&c;c.a=1;c.b=7;return p[0]+p[4];}");
    assert_file(11, "int printf(char *s);struct st{int a;int b;}c;int main(){struct st *q;q=&c;char *p=&c;q->a=2;q->b=9;return p[0]+c.b;}");
    assert_file(3, "enum a{f,g,h};int main(){return g+h;}");
    assert_file(8, "enum a{f=2,g,h=5};int main(){return g+h;}");
    assert_stdout(4, "1 1 1", "int printf(char *s);enum a{f,g,h};int main(){enum a p;p=1;enum a q;q=g;printf(\"%d %d %d\", p, q, p==q);return sizeof(p);}");
    assert_file(3, "int printf(char *s);typedef int *cp;int main(){int a[10]={3,4,5};cp p;p=a;return p[0];}");
    assert_file(12, "int printf(char *s);typedef int *cp;int main(){cp p;return sizeof(*p)+sizeof(p);}");
    assert_file(14, "int printf(char *s);typedef int ca[3];int main(){ca a={1,2,3};return sizeof(a)+a[1];}");
    assert_file(34, "int printf(char *s);int main(){char s[3]=\"\\\"\";return s[0];}");
    assert_file(48, "int printf(char *s);int main(){char s[3]=\"\\60\";return s[0];}");
    assert_file(82, "int printf(char *s);int main(){char s[3]=\"\\x52\";return s[0];}");
    assert_stdout(39, "b", "int putchar(char s);int main(){putchar('b');return '\\'';}");
    assert_stdout(34, "o", "int putchar(char s);int main(){putchar('\\x6f');return '\"';}");
    assert_file(10, "extern int printf(char *s);int main(){printf(\"\");return 10;}");
    assert_stdout(0, "1", "int fprintf();int a;extern int *stderr;extern int *stdout;int main(){a=1;fprintf(stdout, \"%d\", a);return 0;}");
    assert_stdout(0, "1", "int fprintf();int a;struct FILE {int dummy;};typedef struct FILE FILE;extern FILE *stderr;extern FILE *stdout;int main(){a=1;fprintf(stdout, \"%d\", a);return 0;}");
    assert_file(1, "int fprintf();typedef struct S S;struct S {int a;};int main(){S a;a.a=1;return a.a;}");
    assert_file(10, "#define M 10\nint main() { return M; }");
    assert_file(14, "#define M 10+2\nint main() { return M*2; }");
    assert_file(24, "#define M (10+2)\nint main() { return M*2; }");
    assert_file(13, "#define M(a,b) a + b\nint main() { return M(3,5)*2; }");
    assert_file(13, "#define M(a,b) a + b\nint main() { int a; a = 9; return M(3,5)*2; }");
    assert_file(19, "#define M(c,b) a + b\nint main() { int a; a = 9; return M(3,5)*2; }");
    assert_file(8, "#define N 2+2\n#define M(a) N * a\nint main() { return M(3); }");
    assert_file(21, "#define P(a) (a* 3) \n#define N P(5)+2\n#define M(a) N * a\nint main() { return M(3); }");
    assert_file_inc(8, "#include <tmpinc.h>\nint main() { return func(4)+M(3); }", "int func(int n){return n+2;}\n#define M(a) (a-1)\n");
    assert_file_inc(8, "#include \"tmpinc.h\"\nint main() { return func(4)+M(3); }", "int func(int n){return n+2;}\n#define M(a) (a-1)\n");
    assert_file_inc(8, "#define H \"tmpinc.h\"\n#include H\nint main() { return func(4)+M(3); }", "int func(int n){return n+2;}\n#define M(a) (a-1)\n");
    assert_file(0, "#define H 1\n#undef H\nint main(){return 0;}");
    assert_file(0, "#define H 1\n#undef A\nint main(){return 0;}");
    assert_file(1, "int main(){int a;a=2;\n#if 1\na=1;\n#endif\nreturn a;}");
    assert_file(2, "int main(){int a;a=2;\n#if 0\na=1;\n#endif\nreturn a;}");
    assert_file(1, "int main(){int a;a=2;\n#if 1\na=1;\n#else\na=3;\n#endif\nreturn a;}");
    assert_file(3, "int main(){int a;a=2;\n#if 0\na=1;\n#else\na=3;\n#endif\nreturn a;}");
    assert_file(1, "int main(){\n#define A 1\n#ifdef A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(0, "int main(){\n#define A 1\n#ifdef B\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(0, "int main(){\n#define A 1\n#ifndef A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(1, "int main(){\n#define A 1\n#ifndef B\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(0, "int main(){\n#define A 1-1\n#if A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(1, "int main(){\n#define A 1-1*2\n#if A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(0, "int main(){\n#define A 2*(3-3)\n#if A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(0, "int main(){\n#define A 2 ?0:1\n#if A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(1, "int main(){\n#define A 0 ?0:1\n#if A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_file(1, "int main(){\n#define A C==0\n#if A\nreturn 1;\n#else\nreturn 0;\n#endif\n}");
    assert_compile_fail("int main(){}\n#if 1\n");
    assert_file(1, "#if 1\n#endif\nint main(){return 1;}");
    assert_compile_fail("int main(){}\n#if 1\n#if 0\n#elif a\n#else\n#endif\n");
    assert_file_inc(8, "#define A\n#include \"tmpinc.h\"\nint main() { return func(4)+M(3); }", "#ifdef A\nint func(int n){return n+2;}\n#define M(a) (a-1)\n#endif");
    assert_file(1, "#define A\n#define B\n#if defined A && defined B\nint main(){return 1;}\n#endif");
    assert_file(1, "#define A 1\n#define B 2\n#define C(a,b) a ## b\nint main(){return C(A,B) == 12;}\n");
    assert_file(1, "#define C 1 ## 2\nint main(){return C == 12;}\n");
    assert_stdout(1, "c:t d: e:3", "int printf();\n#define C(a,b,...) printf(\"c:%s d:%s e:%d\", __VA_ARGS__);\nint main(){C(10,\"z\", \"t\", \"\", 3)return 1;}\n");
    assert_file(8, "int main(){long int a = 0; return sizeof(a);}");
    assert_file(8, "int main(){long a = 0; return sizeof(a);}");
    assert_file(2, "int main(){short int a = 0; return sizeof(a);}");
    assert_file(2, "int main(){short a = 0; return sizeof(a);}");
    assert_file(4, "union a{int f;char g;};int main(){union a p; p.f=0;p.g=10; return sizeof(p);}");
    assert_file(2, "union a{short f;char g;};int main(){union a p; p.f=0;p.g=10; return sizeof(p);}");
    assert_file(10, "union a{short f;char g;};int main(){union a p; p.f=0;p.g=10; return p.g;}");
    assert_file(0, "union a{short f;char g;};int main(){union a p; p.f=0;p.g=10; p.f=0; return p.g;}");
    assert_stdout(0, "266", "int printf();union a{short f;char g;};int main(){union a p; p.f=300;p.g=10; printf(\"%d\", p.f); return 0;}");
    assert_file(8, "char *const a;int main(){return sizeof(a);}");
    assert_file(8, "char *restrict a;int main(){return sizeof(a);}");
    assert_file(8, "char *volatile a;int main(){return sizeof(a);}");
    assert_file(8, "char *const volatile restrict a;int main(){return sizeof(a);}");
    assert_file(8, "char *const volatile restrict**const* a;int main(){return sizeof(a);}");
    assert_file(8, "char *const volatile restrict**const* (*const a)[1];int main(){return sizeof(a);}");
    assert_file(0, "int a(int a,...){}int b(...){}int main(){return 0;}");
    assert_file(1, "int main(){return __STDC__;}");
    assert_file(1, "#define M(a) a\nint main(){return M((1));}");
    assert_file(1, "int main() {if(1&&1) {return 1;}else{return 0;}}");
    assert_file(0, "int main() {if(1&&0) {return 1;}else{return 0;}}");
    assert_file(0, "int main() {if(0&&1) {return 1;}else{return 0;}}");
    assert_file(0, "int main() {if(0&&0) {return 1;}else{return 0;}}");
    assert_file(2, "int main() {int a=1;if(1&&(a=2)) {}return a;}");
    assert_file(1, "int main() {int a=1;if(0&&(a=2)) {}return a;}");
    assert_file(1, "int main() {if(1||1) {return 1;}else{return 0;}}");
    assert_file(1, "int main() {if(1||0) {return 1;}else{return 0;}}");
    assert_file(1, "int main() {if(0||1) {return 1;}else{return 0;}}");
    assert_file(0, "int main() {if(0||0) {return 1;}else{return 0;}}");
    assert_file(1, "int main() {int a=1;if(1||(a=2)) {}return a;}");
    assert_file(2, "int main() {int a=1;if(0||(a=2)) {}return a;}");
    assert_file(2, "int main() {int a=(10&3); return a;}");
    assert_file(11, "int main() {int a=(10|3); return a;}");
    assert_file(9, "int main() {int a=(10^3); return a;}");
    assert_file(40, "int main() {int a=(10 << 2); return a;}");
    assert_file(2, "int main() {int a=(10 >> 2); return a;}");
    assert_file(3, "int main() {int a;a=3;if(1){int a;a=4;}return a;}");
    assert_file(4, "int main() {int a;a=3;if(1){int k;a=4;}return a;}");
    assert_file(3, "int main() {int a;a=3;if(1){int k;k=4;}return a;}");
    assert_file(3, "int main() {int a;a=3;if(1){int b;}int b;b=1;return a;}");
    assert_compile_fail("int main() {int a;if(1){int a;}int a;}");
    assert_file(3, "int main() {int a;a=0;for(int i = 0; i < 3; i=i+1){a=a+i;}return a;}");
    assert_file(9, "int main() {int a;a=0;for(int i = 0; i < 3; i=i+1){int i;a=a+1;}for(int i = 0; i < 3; i=i+1){a=a+i*2;}return a;}");
    assert_file(9, "int main() {int a;a=0;for(int i = 0, j = 1; i < 3; i=i+j){int i;a=a+1;}for(int i = 0, j=3; i < 3; i=i+1){a=a+i*2;}return a;}");
    assert_file(253, "int main() {char a;a=~2;return a;}");
    assert_file(1, "int main() {int a;a=~9;return a==-10;}");
    assert_file(2, "int main() {int a;a=1;int b;b=a++;return a;}");
    assert_file(1, "int main() {int a;a=1;int b;b=a++;return b;}");
    assert_file(2, "int main() {int v[2];v[0]=1;v[1]=2;int *a;a=v;a++;return *a;}");
    assert_file(10, "struct A{int a;struct {int b;};};int main() {struct A f;f.b=10;f.a=9;return f.b;}");
    assert_file(2, "int main() {char a[10-sizeof(int*)];return sizeof(a);}");
    assert_file(12, "int main() {return sizeof(int [3]);}");
    assert_file(24, "int main() {return sizeof(int *[3]);}");
    assert_file(2, "int main() {int a=3;a ^= 1;return a;}");
    assert_file(8, "int main() {int a=3;a += 5;return a;}");
    assert_file(12, "int main() {int a=4;a *= 3;return a;}");
    assert_file(2, "int main() {int a=7;a &= 10;return a;}");
    assert_file(15, "int main() {int a=7;a |= 10;return a;}");
    assert_file(15, "int main() {int a=7,b=0;b = a |= 10;return b;}");
    assert_file(2, "int main() {return 12 % 5;}");
    assert_file(0, "int main() {return 20 % 5;}");
    assert_file(0, "int main() {return 2 % 1;}");
    assert_file(4, "int main() {return 100 % 12;}");
    assert_file(1, "int main() {int a=10,b=3;int c = a %= b; return c;}");
    assert_file(1, "int main() {switch(1) {case 1: return 1; } return 0;}");
    assert_file(2, "int main() {switch(2) {case 1: return 1; case 2: return 2; } return 0;}");
    assert_file(2, "int main() {switch(2) {case 2: return 2; case 1: return 1; } return 0;}");
    assert_file(3, "int main() {int a=0;switch(2) {case 3: a+=3; case 2: a+=2; case 1: a+=1; } return a;}");
    assert_file(0, "int main() {int a=0;switch(4) {case 3: a+=3; case 2: a+=2; case 1: a+=1; } return a;}");
    assert_file(1, "int main() {int a=0;switch(4) {case 3: a+=3; case 2: a+=2; default: case 1: a+=1; } return a;}");
    assert_file(3, "int main() {int a=0;switch(3) {case 3: a+=3; break; case 2: a+=2; default: case 1: a+=1; } return a;}");
    assert_file(6, "void fun(int *a){for(int i = 0; i < 10; i++){if(i==4){return;}*a+=i;}return;}int main() {int v=0;fun(&v);return v;}");
    assert_file(1, "int main(){int a=0;int b=++a;return b;}");
    assert_file(1, "int main(){int a=0;int b=++a;return a;}");
    assert_file(1, "int main(){int a=2;int b=--a;return b;}");
    assert_file(2, "int main(){int a=2;int b=3;int c=1;return c ? a : b;}");
    assert_file(3, "int main(){int a=2;int b=3;int c=0;return c ? a : b;}");
    assert_file(3, "int main(){return 1,2,3;}");
    assert_file(5, "int main(){int a=0;return a=1,a=2,a+3;}");
    assert_file(12, "int a=10;int main(){a+=2;return a;}");
    assert_file(1, "#define a a\nint main(){int a=1;return a;}");
    assert_file(1, "int a=1;int b=2;\n#define a b\n#define b a\nint main(){return a;}");
    assert_file(8, "int g(int c,int d){return c*d;}int a=1;int b=2;\n#define f(a,b) g(a,b) + b\n#define g(a,b) f(a,b)\nint main(){return g(3,2);}");
    assert_file(6, "int main(){int a=0;for(int i = 0; i < 10; i++){if(i==4){break;}a+=i;}return a;}");
    assert_file(6, "int main(){int a=0;int i=0;while(1){if(i==4){break;}a+=i;i++;}return a;}");
    assert_file(6, "int main(){int a=0;int i=0;do{if(i==4){break;}a+=i;i++;}while(1);return a;}");
    assert_file(0, "int main(){int a=1;return !a;}");
    assert_file(1, "int main(){int a=0;return !a;}");
    assert_file(70, "struct A {int a;char b;};struct A p = {30, 40};int main(){return p.a+p.b;}");
    assert_stdout(0, "tmp.c:1", "int printf(...);int main(){printf(\"%s:%d\", __FILE__, __LINE__);return 0;}");
    assert_stdout(0, "tmp.c:2", "int printf(...);int main(){\nprintf(\"%s:%d\", __FILE__, __LINE__);return 0;}");
    assert_stdout(0, "tmp.c:3", "int printf(...);int main(){\n\nprintf(\"%s:%d\", __FILE__, __LINE__);return 0;}");
    assert_file_inc(3, "#include <tmpinc.h>\nint main() { return func(); }", "\n\nint func(){return __LINE__;}\n");
    assert_stdout(0, "main", "int printf(...);int main(){printf(\"%s\", __func__);return 0;}");
    assert_stdout(0, "myfunc", "int printf(...);int myfunc(){printf(\"%s\", __func__);}int main(){myfunc();return 0;}");
    assert_file(12, "int main(){int a = 0;for(int i = 0; i < 10; i++){if(i==3){i=8;continue;}a += i;}return a;}");
    assert_file(20, "int main(){int a = 0;int i = 0;while(i < 10){if(i==3){i=8;continue;}a+=i;i++;}return a;}");
    assert_file(107, "struct A {int a; char b[10];};struct A a={10, \"abc\"};int main(){return a.a + a.b[0];}");
    assert_file(107, "struct A {int a; char *b;};struct A a={10, \"abc\"};int main(){return a.a + a.b[0];}");
    assert_file(110, "struct A {int a; char b[10];};struct A a[]={{10, \"abc\"}, {3, \"def\"}};int main(){return a[0].a + a[1].b[0];}");
    assert_file(110, "struct A {int a; char *b;};struct A a[]={{10, \"abc\"}, {3, \"def\"}};int main(){return a[0].a + a[1].b[0];}");
    assert_file(134, "struct A {int a; char *b;};struct A a[]={{10, \"abc\"}, {3, \"def\"},};int main(){return a[0].a + a[1].b[0] + sizeof(a);}");
    assert_file(134, "struct A {int a; char *b;};int main(){struct A a[]={{10, \"abc\"}, {3, \"def\"},};return a[0].a + a[1].b[0] + sizeof(a);}");
    assert_file(11, "int strlen();int func(int a, char *p, char c, ...) {return a+strlen(p)+c;}int main(){return func(3,\"aaaa\",4);}");
    assert_stdout(24, "aaaa:99 Hello va_list", "#define va_list __builtin_va_list\n#define va_start __builtin_va_start\n#define va_end __builtin_va_end\nint vprintf();int func(int a, char *p, char c, ...) {va_list ap;va_start(ap, c);vprintf(p, ap);va_end(ap);return sizeof(ap);}int main(){return func(3,\"aaaa:%d %s\",4,99,\"Hello va_list\");}");
    assert_file(45, "int main(){int i = 0;int a=0; while(1){i++;int j = 0; while(1){j++;a+=j;if(j>=5)break;} if(i<3){continue;}break;}return a;}");
    assert_file(1, "int main(){\n#ifdef __x86_64__\nreturn 1;\n#else\nreturn 2;\n#endif\n}");
    assert_file(1, "#define A\n#ifdef A //aa\n#else\n#endif\nint main(){return 1;}");
    assert_file(4, "int main(){char a;short b;return sizeof(a+b);}");
    assert_file(1, "int main(){char a=20;char b=30;return (a*b) == 600;}");
    assert_file(1, "int main(){char a=20;short b=30;return (a*b) == 600;}");
    assert_file(1, "int main(){char a=20;int b=30;return (a*b) == 600;}");
    assert_file(1, "int main(){char a=20;long b=30000000000L;return (a*b) == 600000000000;}");
    assert_file(1, "int main(){long b=30000000000L;int a=b;return a == -64771072;}");
    assert_file(1, "#include <string.h>\nint main(){int a=-839970252;unsigned char *p=(unsigned char*)&a;return memcmp(p, \"\\x34\\x12\\xef\\xcd\", 4) == 0;}");
    assert_file(1, "#include <string.h>\nint main(){long a=35243;unsigned char *p=(unsigned char*)&a;return memcmp(p, \"\\xab\\x89\\x00\\x00\\x00\\x00\\x00\\x00\", 8) == 0;}");
    assert_file(1, "#include <string.h>\nint main(){long a=-9223372036854775808;unsigned char *p=(unsigned char*)&a;return memcmp(p, \"\\x00\\x00\\x00\\x00\\x00\\x00\\x00\\x80\", 8) == 0;}");
    assert_file(1, "#include <string.h>\nint main(){long a=4021214123015969202;unsigned char *p=(unsigned char*)&a;return memcmp(p, \"\\xb2\\x99\\x26\\x88\\xf0\\x38\\xce\\x37\", 8) == 0;}");
    assert_stdout(0, "-16\n", "#include <stdio.h>\nint main(){signed char a=0xf0;short b = a;printf(\"%hd\\n\", b);return 0;}");
    assert_stdout(0, "240\n", "#include <stdio.h>\nint main(){unsigned char a=0xf0;short b = a;printf(\"%hd\\n\", b);return 0;}");
    assert_stdout(0, "61440\n", "#include <stdio.h>\nint main(){unsigned short a=0xf000;int b = a;printf(\"%d\\n\", b);return 0;}");
    assert_file(0, "int main(){int *p=0x10000000000;int *q=0;return p==q;}");
    assert_file(0, "int main(){long p=0x10000000000;int q=0;return p==q;}");
    assert_file(1, "int main(){long p=0x10000000000;int q=1;return p!=q;}");
    assert_file(1, "int main(){int *p=0x10000000000;void *q=(void *)1;return p > q;}");
    assert_file(0, "int main(){int *p=0x10000000000;void *q=(void *)1;return q > p;}");
    assert_file(1, "int main(){long p=0x10000000000;return (1L<<40)==p;}");
    assert_file(1, "int main(){long p=0x10000000000;long r=0x30000000000;return (p|r)==r;}");
    assert_file(1, "#include <stddef.h>\nstruct a{int c;char k;};int main(){return (offsetof(struct a,k)) == 4;}");
    assert_file(1, "#include <stddef.h>\nint main(){char *p=0;return p == NULL;}");
    assert_file(2, "#define ma() 1+\nint main(){return ma() 1;}");
    assert_file(42, "int main(){return (-1-(1*2+3)/2+(2*3*(4/9*5+(1+2-4-(3+6))))) + ((10*10*10)/10) + (1*2*3*4*5/1/2/3/(22/11*9/4));}");
    assert_file(1, "long p(int k){return (long)k * 100;}int main(){return p(1000000000) == 100000000000;}");
    assert_file(1, "long p(int k){return k * 100;}int main(){return p(1000000000) != 100000000000;}");
    assert_file(1, "int main(){long k=100000000000;return k == 100000000000;}");
    assert_file(0, "int main(){long k=100000000000;return (int)k == 100000000000;}");
    assert_file(1, "int main(){long k=100000000000;return (long)k == 100000000000;}");
    assert_file(0, "int main(){long k=100000000000;return (long)(int)k == 100000000000;}");
    assert_file(1, "int main(){int a[1+(int)sizeof(long)];return sizeof(a)==4*9;}");
    assert_file(1, "int main(){int a[1+(char)(250+sizeof(long))];return sizeof(a)==4*3;}");
    printf("OK\n");
    return 0;
}
