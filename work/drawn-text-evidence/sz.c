#include <stdio.h>
#include "screen.h"
int main(void) { printf("screen_line %zu screen_page %zu\n", sizeof(screen_line), sizeof(screen_page)); return 0; }
