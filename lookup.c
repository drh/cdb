#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lookup.h"

static void err(char *s) {
	printf("? %s\n", s);
	exit(1);
}

static struct node words[2000];
static int next = 0;

struct node *lookup(char *word, struct node **p) {
	if (*p) {
		int cond = strcmp(word, (*p)->word);
		if (cond < 0)
			return lookup(word, &(*p)->left);
		else if (cond > 0)
			return lookup(word, &(*p)->right);
		else
			return *p;
	}
	if (next >= sizeof words/sizeof words[0])
		err("out of node storage");
	words[next].count = 0;
	words[next].left = words[next].right = NULL;
	words[next].word = malloc(strlen(word) + 1);
	if (words[next].word == NULL)
		err("out of word storage");
	strcpy(words[next].word, word);
	return *p = &words[next++];
}
