#include <stdio.h>
#include "lookup.h"

static int isletter(int c) {
	if (c >= 'A' && c <= 'Z')
		c += 'a' - 'A';
	if (c >= 'a' && c <= 'z')
		return c;
	return 0;
}

static int getword(char *buf) {
	char *s;
	int c;

	while ((c = getchar()) != -1 && isletter(c) == 0)
		;
	for (s = buf; (c = isletter(c)) != 0; c = getchar())
		*s++ = c;
	*s = 0;
	if (s > buf)
		return 1;
	return 0;
}

void tprint(struct node *tree) {
	if (tree) {
		tprint(tree->left);
		printf("%d\t%s\n", tree->count, tree->word);
		tprint(tree->right);
	}
}

static struct node *words = NULL;

int main(int argc, char *argv[]) {
	char buf[40];

	while (getword(buf))
		lookup(buf, &words)->count++;
	tprint(words);
	return 0;
}
