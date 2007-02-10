struct node {
	int count;		/* number of occurrences */
	struct node *left;	/* left subtree */
	struct node *right;	/* right subtree */
	char *word;		/* pointer to the word */
};
extern struct node *lookup(char *, struct node **);
