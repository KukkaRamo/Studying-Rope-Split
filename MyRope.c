// Invariants:
// leftLen is always the length of the left subtree
// Root never has a right subtree
// Root never has data (except terminating NULL character)
// Node is leaf if and only if its left and right subtree are NULL
// Data in each node starts from index zero and ends with a terminal NULL character
// If node is not leaf, its data has nothing but the terminal NULL character

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum ErrorCodes {OK, ARGS, PARAM, ALLOC, INTERNAL, NOTDOUBLE, BOUNDINGBOXARGS};
enum ErrorCodes currentError = OK;

void errorOccurred() {
	switch (currentError) {
		case ARGS:
		printf ("usage: programname \n");
		break;
		case ALLOC:
		printf ("Allocation failed, out of memory\n");
		break;
		case PARAM:
		printf ("Erroneous parameter for a function\n");
		break;
		case INTERNAL:
		printf ("Internal error in a rope, blame the software developer\n");
		break;
	}
	exit(EXIT_FAILURE);
}

struct node {
	char* data;
	int leftLen;
	struct node* left;
	struct node* right;
	struct node* parent;
};

struct location {
	struct node* myNode;
	int myIndex;
};

int myMin (const int a, const int b) { // Why does not inline work with gcc?
	return (a < b) ? a : b;
}

// Returns whether the rope contains any characters
short isEmpty (const struct node* const r) {
	return (r==NULL || r->left == NULL || r->leftLen==0) ? 1 : 0;
}

// Creates and initializes a new node
// Returns NULL if creation fails
struct node* initNode(const int dataSize) {
	struct node* n = malloc(sizeof(struct node));
	if (n != NULL) {
		n->left = NULL;
		n->right = NULL;
		n->leftLen = 0;
		n->parent = NULL;
		char* myData = malloc ((dataSize + 1) * sizeof(char));
		if (myData == NULL) {
			currentError = ALLOC;
			free (n);
			return NULL;
		}
		myData[dataSize] = '\0';
		n->data = myData;
	}
	else currentError = ALLOC;
	return n;
}

// Free a single node
void freeNode (struct node* where) {
	if (where->data != NULL)
		free (where->data);
	free (where);
}

// Frees all nodes from node "where" on and included
void freeAll(struct node* where) {
	if (where != NULL) {
		if (where->left != NULL)
			freeAll(where->left);
		if (where->right != NULL)
			freeAll(where->right);
		freeNode(where);
	}
}

// Splits the leaf node in two
// Takes characters from position pos to the end out of the leaf and moves them into a new leaf
// The new leaf has NULL parent
// Returns a pointer to the new leaf
// And moves the characters as a side effect
// Does not update leaf's parent's leftLen even when leaf is a left child
// and does not update leftLen-variables of any ancestors
// Precondition: original leaf data must contain at least the NULL character
struct node* splitLeaf (struct node* const leaf, const int pos) {
	if (leaf==NULL || pos < 0) { // Erroneous parameters, no splitting
		currentError = PARAM;
		return NULL;
	}
	size_t totalLen = strlen(leaf->data);
	if (totalLen < 0 || pos > totalLen) { // Error, no splitting
		currentError = PARAM;
		return NULL;
	}
	
	int newNodeSize = totalLen - pos;
	struct node* newNode = NULL;
	newNode = initNode(newNodeSize);
	if (newNode == NULL) {
		goto splitLeafError;
	}
	
	strcpy(newNode->data, leaf->data + pos);
	
	char* p = realloc (leaf->data, (pos+1) * sizeof(char)); // Terminal NULL included
	if (p == NULL) {
		free (leaf->data); // To prevent the leak if realloc fails
		goto splitLeafError;
	}
	leaf->data = p;
	leaf->data[pos] = '\0';

	return newNode;

	splitLeafError:
	if (newNode != NULL)
		free (newNode);
	currentError = ALLOC;
	return NULL;
}

// Calculates the length of a subtree rooted at pnode.
// Precondition: if pnode is not NULL, it has at least the terminal NULL as data
int countLength (const struct node* const pnode) {
	int result = 0;
	if (pnode!=NULL) {
		if (pnode->left==NULL && pnode->right==NULL) {
			result = strlen(pnode->data);
		}
		else {
			if (pnode->left != NULL)
				result = pnode->leftLen;
			if (pnode->right != NULL)
				result += countLength(pnode->right);
		}
	}
	return result;
		
}

// Rope split as in wikipedia.  Here is an implementation that is logarithmic if the tree is balanced.
// split splits an input rope in two.  As a side effect, the first rope contains data until the given position
// and the output rope contains the rest of the data
// Position starts from zero, and position is the first index of the data that is transferred to output rope
// Wikipedia version does not remove an intermediate node (no root or leaf) if it has only one child
// Removing those nodes would make the rope simpler and more balanced
struct node* split (struct node* rope, int position) {
	if (rope == NULL || position < 0) {
		currentError = PARAM;
		return rope;
	}
	int currentLength = rope->leftLen; // Length of the original rope
	if (position >= currentLength) {
		currentError = PARAM;
		return rope;
	}
	int unsplit;
	// First, newtree is a new rope: 
	struct node* newtree = initNode(0);
	if (newtree == NULL)  {// Cannot allocate nodes, cannot split
		currentError = ALLOC;
		return rope;
	}
	struct node* currentNew = newtree;
	struct node* currentOrig = rope;

	// To descend
	while (currentOrig->left != NULL || currentOrig->right != NULL) {
		 // Left or right decision by comparing position to leftLen
		 if (position < currentOrig->leftLen) { // Going left
			 if (currentOrig->right != NULL)
			 {
				 // Use root's left node, which has been created when the rope was initialized
				 struct node* newLeft = initNode(0);
				 if (newLeft == NULL) { // If node allocation fails, split fails
					 freeAll(newtree);
					 currentError = ALLOC;
					 return NULL;
				 }
				 newLeft->right = currentOrig->right; // Right tree to be removed on ascent
				 currentOrig->right->parent = newLeft;
				 newLeft->parent = currentNew;
				 currentNew->leftLen = currentLength - currentOrig->leftLen; // Correct this (add the rest) when the tree is ready by going upwards
				 //  Not here, only on ascent currentOrig.right = NULL; // Remove from the original rope
				 currentNew->left = newLeft;
				 currentNew = currentNew->left;
			 }
			 currentLength = currentOrig->leftLen;
			 currentOrig = currentOrig->left;
		 }
		 else { // Going right
			position -= currentOrig->leftLen;
			currentLength -= currentOrig->leftLen;
			currentOrig = currentOrig->right;
		 }
	}
	
	short splitMe = 1;
	// Now we are at data node
	// split it at the position
	int origNumOfChars = strlen(currentOrig->data);
	unsplit = origNumOfChars;
	struct node* newLeft = splitLeaf(currentOrig, position);
	if (currentError == OK) {
		newLeft->parent = currentNew;
		currentNew->left = newLeft;
		currentNew->leftLen = origNumOfChars - position;
		if (currentOrig->parent != NULL &&
		  currentOrig->parent->right == currentOrig)
			splitMe = 0; // If I am right leaf, do not remove me
		unsplit = position;
	}
	else { // splitLeaf fails, e.g. malloc fails
		return NULL;
	}
	
	// To ascend and update the leftLen-fields of the path nodes in the original rope
	// Make changes only here, so that the original rope is preserved if some allocation fails
	while (currentOrig != rope) {
		if (currentOrig->parent->left == currentOrig) { // I am left
			currentOrig->parent->leftLen = unsplit;
			if (splitMe == 1) {
				currentOrig->parent->right = NULL; // Remove here the right subtrees on the path
			}
		}
		else { // I am right
			unsplit += currentOrig->parent->leftLen;
		}
		currentOrig = currentOrig->parent;
		splitMe = 1;
	}
	
	// Correct the leftlengths of the new tree
	int cumLen = 0;
	while (currentNew != newtree) {
		currentNew->leftLen += cumLen;
		cumLen = currentNew->leftLen;
		currentNew = currentNew->parent;
	}
	currentNew->leftLen += cumLen;
	
	return newtree;
}

// Returns the kth character, or NULL if the character does not exist or an error occurs
// k is the index, starting from zero
// Precondition: leftlen is the real length of the left subtree
char kthChar (struct node* from, int k) {
	if (from == NULL || k < 0) {
		currentError = PARAM;
		return '\0';
	}
	if (from->left == NULL && from->right == NULL) { // leaf, return data
		if (from->data == NULL) {
			currentError = INTERNAL;
			return '\0';
		}
		else if (strlen(from->data) <= k) { // length + index from zero offset
			currentError = PARAM;
			return '\0';
		}
		else return from->data[k]; // from zero
	}
	if (k < from->leftLen)
		return kthChar(from->left, k);
	else
		return kthChar(from->right, k - from->leftLen);
}

// Returns the node and index (starts from zero) in the node of the kth character in 
//   a subtree starting from pFrom.
// k is the index, starting from zero
// Precondition: leftlen is the real length of the left subtree
struct location* gotoNode (struct node* pFrom, int k) {
	if (pFrom == NULL || k < 0) {
		if (currentError != INTERNAL)
			currentError = PARAM;
		return NULL;
	}
	if (pFrom->left == NULL && pFrom->right == NULL) { // leaf, return from data
		if (pFrom->data == NULL || strlen(pFrom->data) <= k) {
			currentError = INTERNAL;
			return NULL; // Required data does not exist
		}
		else {
			struct location * loc = malloc (sizeof(struct location));
			loc->myNode = pFrom; // Throws if allocation fails
			loc->myIndex = k;
			return loc;
		}
	}
	if (k < pFrom->leftLen)
		return gotoNode(pFrom->left, k);
	else
		return gotoNode(pFrom->right, k - pFrom->leftLen);
}

// Returns a new node that has left as left subtree and right as right subtree
// The length of the left subtree is pLeftLen
// Precondition: if left and/or right exist, they must have NULL parents
struct node* concat (struct node* left, struct node* right, const int pLeftLen) {
	if (left != NULL && left->parent != NULL || right != NULL && right->parent != NULL) {
		currentError = PARAM;
		return NULL;
	}
	struct node* n = initNode(0);
	if (n != NULL) {
		n->left = left;
		n->right = right;
		n->leftLen = pLeftLen;
		if (left != NULL)
			left->parent = n;
		if (right != NULL)
			right->parent = n;
	}
	else currentError = ALLOC;
	return n;
}

// Inserts a string into the rope
// Precondition: original rope must not be NULL (but may be empty)
// 1..i-1, insertString, i..m like in wikipedia but more logical index for inserting
struct node* insert (struct node* rope, int i, char* insertString) {
	if (i < 1 || i > rope->leftLen + 1 || strlen(insertString) <= 0) {
		// Rope is NULL, erroneous index, or nothing to insert
		currentError = PARAM;
		return rope;
	}
	
	struct node* newNode = NULL, * rightrope = NULL, * retval = NULL;
	int dataLength = strlen(insertString);
	int origLength = rope->leftLen;
	newNode = initNode(dataLength);
	if (currentError != OK)
		goto errorInInsert;
	strcpy(newNode->data, insertString);
	
	if (isEmpty (rope) == 0) { // No need to concat if the original rope is empty	
		if (i==1 || i==rope->leftLen + 1) { // No need to split
			rope->left->parent = NULL; // Root will be removed
			if (i == 1)
				newNode = concat(newNode, rope->left, dataLength);
			else
				newNode = concat(rope->left, newNode, origLength);
			if (currentError != OK) 
				goto errorInInsert;
		}
		else { // Both left and right side have characters, split needed
			rightrope = split (rope, i-1);
			if (currentError != OK) 
				goto errorInInsert;
			
			// What is left from the rope after split, left side
			rope->left->parent = NULL; // The unnecessary root of rope will be removed
			newNode = concat(rope->left, newNode, rope->leftLen);
			if (currentError != OK) 
				goto errorInInsert;

			rightrope->left->parent = NULL; // Root will be removed
			newNode = concat(newNode, rightrope->left, i-1 + dataLength);
			if (currentError != OK)
				goto errorInInsert;
			freeNode(rightrope);
		}
	}
	
	if (rope != NULL)
		freeNode(rope);
	retval = initNode(0);
	if (currentError != OK)
		goto errorInInsert;
	retval->leftLen = dataLength + origLength;
	retval->left = newNode;
	newNode->parent = retval;
	return retval;
	
	errorInInsert:
	if (newNode != NULL)
		freeAll(newNode);
	if (retval != NULL)
		freeAll(retval);
	if (rightrope != NULL)
		freeAll(rightrope);
	if (rope != NULL)
		freeAll(rope);
	errorOccurred();
}

// Deletes a string from the rope
// 1..i-1, j+1..m saved, other characters deleted, like in wikipedia
struct node* delete (struct node* rope, int i, int j) {
	if (isEmpty(rope) != 0) {
		currentError = PARAM;
		return rope;
	}
	size_t origLength = rope->leftLen;
	if (i < 1 || j > origLength || j < i) {// Error
		currentError = PARAM;
		return rope;
	}
		
	struct node* leftRope=NULL, * middleRope = NULL, * rightRope = NULL, * retVal = NULL;
	
	if (i == 1) {
		middleRope = rope;
	}
	else {
		middleRope = split (rope, i-1);
		leftRope = rope;
		if (currentError != OK)
			goto errorInDelete;
	}
	// left rope has i-1 characters
	if (j < origLength) {
		rightRope = split (middleRope, j-i+1);
		if (currentError != OK)
			goto errorInDelete;
	}

	if (i==1) {
		if (j == origLength) {
			retVal = initNode(0);
			if (currentError != OK)
				goto errorInDelete;
		}
		else
			retVal = rightRope;
	}
	else if (j == origLength)
		retVal = leftRope;
	else { // Both leftRope and rightRope contain nodes
		int totSize = leftRope->leftLen + rightRope->leftLen;
		leftRope->left->parent = NULL; // Root will be removed
		rightRope->left->parent = NULL; // Root will be removed
		struct node* n = concat(leftRope->left, rightRope->left, leftRope->leftLen);
		if (currentError != OK) // If nodes cannot be allocated here, the rope is corrupted
			goto errorInDelete;
		retVal = initNode(0); // This is here in order to keep the original rope intact if creation fails
		if (currentError != OK)
			goto errorInDelete;
		n->parent = retVal;
		retVal->leftLen = totSize;
		retVal->left = n;
		freeNode(leftRope);
		freeNode(rightRope);
	}
	freeAll(middleRope);
	return retVal;
	
	errorInDelete:
	printf ("Error in delete\n");
	if (leftRope != NULL)
		freeAll(leftRope);
	if (middleRope != NULL)
		freeAll(middleRope);
	if (rightRope != NULL)
		freeAll(rightRope);
	if (retVal != NULL)
		freeAll(retVal);
	if (rope != NULL)
		freeAll(rope);
	errorOccurred();
}

// Recursively picks the characters into a buffer for collect-method, using inorder travelsal
int inOrderPick (struct node* location, int charsLeft, char* buffer) {
	if (charsLeft <	0) {
		currentError = PARAM;
		return 0;
	}
	if (location == NULL || charsLeft == 0)
		return 0;
	int origCharsLeft = charsLeft;
	charsLeft -= inOrderPick (location->left, charsLeft, buffer);
	if (location->left == NULL && location->right == NULL) { // I am leaf
		int charsPicked = myMin(charsLeft, strlen(location->data));
		strncat(buffer, location->data, charsPicked);
		charsLeft -= charsPicked;
	}
	charsLeft -= inOrderPick (location->right, charsLeft, buffer);
	return origCharsLeft - charsLeft;
}

// Collects the characters from index i to j, both included, and returns a string consisting of those characters
// starts from one
char* collect (struct node* collectRope, int i, int j) {
	if (i < 1 || j > collectRope->leftLen || j < i) {
		currentError = PARAM;
		return NULL;
	}
	char* nn;
	int charsLeft = j-i+1;
	struct location* location = gotoNode(collectRope, i-1);
	if (currentError != OK)
		goto errorInCollect;
	int charsPicked = myMin(charsLeft, strlen(location->myNode->data) - location->myIndex);
	nn = malloc ((j - i + 2) * sizeof(char)); // Terminal NULL
	if (currentError != OK) 
		goto errorInCollect;
	strncpy(nn, location->myNode->data + location->myIndex, charsPicked);
	nn[charsPicked] = '\0'; // strncpy does not add NULL (strncat does)
	 // The characters in the first node  
	charsLeft -= charsPicked;
	struct node* whereIAm = location->myNode;
	while (whereIAm != collectRope && charsLeft > 0) { // Continue inorder from here
		if (whereIAm->parent->left == (struct node*) whereIAm) {
			charsLeft -= inOrderPick (whereIAm->parent->right, charsLeft, nn);
			if (currentError != OK) {
				currentError = INTERNAL;
				goto errorInCollect;
			}				
		}
		whereIAm = whereIAm->parent;
	}
	if (charsLeft > 0) printf("Could not collect all characters \n");
	return nn;
	
	errorInCollect:
	printf (" %s ","Error in collect \n");
	if (location != NULL)
		free (location);
	if (nn != NULL)
		free (nn);
	errorOccurred();
}

// Rebuilds recursively the nodes for rebuild-method
struct node* rebuildNodes (struct node* rope, const int nodeSize, const int levels, int currentLevel, int* lengthLeft) {
	struct node* retval = NULL;
	if (currentLevel < levels) { // Non-leaf
		int origLengthLeft = *lengthLeft;
		retval = initNode(0);
		if (currentError != OK)
			errorOccurred();
	// retVal->left = rebuildNodes with left info, retVal->right = rebuildNodes with length info, retVal->leftLen = length of left
		retval->left = rebuildNodes(rope, nodeSize, levels, currentLevel + 1, lengthLeft);
		if (retval->left != NULL)
			retval->left->parent = retval;
		retval->leftLen = origLengthLeft - (int) *lengthLeft;
		retval->right = rebuildNodes(rope, nodeSize, levels, currentLevel + 1, lengthLeft);
		if (retval->right != NULL)
			retval->right->parent = retval;
	}
	else {
		// Leaf: make data with nodeSize or chars left if last char node and null if no more chars
		if ((int) *lengthLeft > 0) { // If no more characters left, return NULL
			int thisRound = myMin(nodeSize, (int) *lengthLeft);
			retval = initNode(thisRound);
			if (currentError != OK)
				errorOccurred();
			int begin = rope->leftLen - (int) *lengthLeft +1;
			retval->data = strcpy(retval->data, collect(rope, begin, begin + thisRound - 1));
			printf(" %s ",retval->data);
			if (currentError != OK) {
				free (retval);
				errorOccurred ();
			}
			*lengthLeft -= thisRound;
		}
	}
	return retval;
}

// Makes a balanced copy of the whole rope, so that:
// All data leafs have the same number of characters, except possibly the last one
// All paths from root to leafs that contain data have the same length
// Last level is filled from the left to right, the rightmost nodes may be missing
// The original rope is not freed (if you want to free it, use the freeAll-method)
// If there is no data in the rope, the original rope is returned and not copied
// Precondition: rope size must not be extremely small compared to nodeSize
struct node* rebuild (struct node* rope, const int nodeSize) {
	if (rope == NULL || rope->leftLen < 0 || nodeSize <= 0) {
		currentError = PARAM;
		errorOccurred();
	}
	int leaves = 0;
	int levels = 0;
	if (rope->leftLen > 0) { // TODO: Check if ceil gives an exact integer (ceil returns double)
		leaves = ceil ((double) rope->leftLen / nodeSize); // Cannot be zero 
		//unless inappropriate parameters result in precision issues
		levels = ceil (log2 (leaves));
	}
	else
		return rope; // Empty rope
	
	struct node* retVal = initNode(0);
	if (currentError != OK)
		errorOccurred();
	int leftLength = rope->leftLen;
	int* lenPtr = &leftLength;
	retVal->leftLen = leftLength;
	retVal->left = rebuildNodes(rope, nodeSize, levels, 0, lenPtr);
	retVal->left->parent = retVal;
	return retVal;
}

int main (int argc, char** argv) {
	if (argc != 1)
		errorOccurred (ARGS);
	struct node* rope = initNode(0);
	if (rope == NULL)
		errorOccurred (ALLOC);
	rope = insert (rope, 1, "Building sturdy");
	rope = insert (rope, 10, "rope ");
	rope = delete (rope,3,5);
	printf("%d",rope->leftLen);
	printf(" ");
	printf (" %d ",rope->left);
	printf (" %d ",rope->left->leftLen);
	printf (" %d ",rope->left->left);
	printf (" %d ", rope->left->left->leftLen);
	printf(" %d ",strlen(rope->left->left->data));
	printf("%s", rope->left->left->data);
	printf (" %s ", collect(rope, 1, 17));
	printf (" %s ", collect(rope, 10, 12));
	struct node* rope1 = rebuild(rope, 3);
	printf (" %s ", collect(rope1, 1, 17));
	rope = insert (rope, 1, "Xx");
	rope = insert (rope, 20, " Yyy");
	printf (" %s ", collect(rope, 1, 20));
	printf (" %s ", collect(rope, 1, 23));
	rope = delete (rope, 1, 3);
	printf (" %s ", collect(rope, 1, 20));
	rope = delete (rope, 19, 20);
	printf (" %s ", collect(rope, 1, 18));
	rope = delete(rope, 1,18);
	printf (" %d %d %d %d ", rope, rope->leftLen, rope->left, rope->right);
	printf (" %s ", collect(rope1, 1, 17));
	rope1 = insert(rope1, 5, "Bye now");
	printf (" %s ", collect(rope1, 1, 24));
	rope1 = delete(rope1, 2,3);
	printf (" %s ", collect(rope1, 1, 22));
	freeAll(rope);
	freeAll(rope1);
	exit(EXIT_SUCCESS);
}
