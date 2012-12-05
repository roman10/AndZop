#include "andutils.h"

int get_file_size(char *fName) {
	FILE *f;
	int lSize;
	f = fopen(fName, "r");
	if (f == NULL) {
		return -1;
	}
	fseek(f, 0L, SEEK_END);
	lSize = ftell(f);
	fclose(f);
	return lSize;
}
