#include "bb_cli.h"

int main_mid2bba(char **argv,int argc);

/* Usage.
 */
 
static void print_usage(const char *exename) {
  fprintf(stderr,"\nUsage: %s COMMAND [ARGS...]\n\n",exename);
  fprintf(stderr,
    "help|--help\n"
    "  Print this message.\n"
    "\n"
  );
  fprintf(stderr,
    "mid2bba -oOUTPUT INPUT\n"
    "  OUTPUT is a binary bba song.\n"
    "  INPUT is a MIDI file.\n"
    "\n"
  );
}

/* Main, dispatch on first argument.
 */

int main(int argc,char **argv) {

  if (argc<2) {
    print_usage((argc>=1)?argv[0]:"beepbot");
    return 1;
  }
       if (!strcmp(argv[1],"mid2bba")) return main_mid2bba(argv+2,argc-2);
  else if (!strcmp(argv[1],"help")) { print_usage(argv[0]); return 0; }
  else if (!strcmp(argv[1],"--help")) { print_usage(argv[0]); return 0; }
  
  print_usage(argv[0]);
  return 1;
}
