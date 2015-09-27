#include <stdio.h>
#include <stdlib.h>

static void do_head(FILE *f, long n_lines);

int main(int argc, char *argv[])
{
  long n_lines;
 //エラー処理 引数がない場合はプログラムを終了させる
 if(argc < 2){
   fprintf(stderr, "Usage: %s n [file file ...]\n", argv[0]);
   exit(1);
 }
 n_lines = atol(argv[1]);
 if(argc == 2){
  do_head(stdin, n_lines);
 } else {
   int i;
   for(i = 2; i < argc; i++){
    FILE *f;
    f = fopen(argv[i],"r");
    if(!f){
      perror(argv[i]);
      exit(1);
    }
    do_head(f,n_lines);
    fclose(f);
   }
 }
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
