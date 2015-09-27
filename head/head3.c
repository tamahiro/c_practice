#include <stdio.h>
#include <stdlib.h>

//getopt_long()のプロトタイプ宣言を取り込む
#define _GNU_SOURCE
#include <getopt.h>

static void do_head(FILE *f, long n_lines);

#define DEFAULT_N_LINES 10

//struct optionの構造体宣言と同時に配列を初期化
//末尾に全メンバの値を0にした要素を最後に置くことでgetopt_long()に引数の最後を教えることができる
static struct option longopts[] = {
  { "n_lines", required_argument, NULL, 'n' },
  { "help", no_argument, NULL, 'h' },
  { 0, 0, 0, 0  }
};

int main(int argc, char *argv[])
{
  int opt;
  long n_lines = DEFAULT_N_LINES;

  while((opt = getopt_long(argc,argv,"n:",longopts,NULL)) != -1){
    switch(opt){
      case 'n':
        n_lines = atoi(optarg);
        break;
      case 'h':
        fprintf(stdout, "Usage: %s [-n LINES] [FILE ...]\n",argv[0]);
        exit(0);
      case '?':
        fprintf(stderr, "Usage: %s [-n LINES] [FILE ...]\n",argv[0]);
        exit(1);
    }
  }
  if(optind == argc) {
    do_head(stdin, n_lines);
  } else {
    int i;

    for(i = optind; i < argc; i++){
      FILE *f;
      f = fopen(argv[i], "r");
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
