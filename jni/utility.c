#include "utility.h"

int if_file_exists(char *p_filename) {
    FILE *l_file;
    l_file = fopen(p_filename, "r");
    if (l_file != NULL) {
	fclose(l_file);
	return 1; 
    } else {
	return 0;
    }
}
