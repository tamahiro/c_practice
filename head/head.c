#include <stdio.h>
#include <stdlib.h>

static void do_head(FILE *f, long n_lines);

int main(int argc, char *argv[])
{
 //エラー処理 引数がない場合はプログラムを終了させる
 if(argc < 2){
   fprintf(stderr, "Usage: %s n\n", argv[0]);
   exit(1);
 }

 do_head(stdin, atol(argv[1]));
 exit(0);
}

static void do_head(FILE *f, long n_lines)
{
  int c;
  if(n_lines <= 0) return;
  while((c = getc(f)) != EOF){
    if(putchar(c) < 0) exit(1);
    if(c == '\n'){
      n_lines--;
      if(n_lines == 0) return;
    }
  }
  exit(0);
}
