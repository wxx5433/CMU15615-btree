/*
 * Search for up to k successors of the given key in the 
 * B+ tree, sorted lexicographically. 
 * The key is not included in the result. 
 * If there are less than k successors, the result should
 * still return them. 
 *
 * Author: Xiaoxiang Wu
 * Andrew ID: xiaoxiaw
 */
#include "def.h"

extern int iscommon(char *word);
extern int check_word(char *word);
extern int strtolow(char *s);
extern PAGENO treesearch_page(PAGENO pageNo, char *key);
extern int FreePage(struct PageHdr *PagePtr);
extern struct PageHdr *FetchPage(PAGENO Page);
extern int FindInsertionPosition(struct KeyRecord *KeyListTraverser, char *Key, 
        int *Found, NUMKEYS NumKeys, int Count);
extern PAGENO FindNumPagesInTree(void);

struct KeyRecord* findKey(struct PageHdr* PagePtr, char *key) {
    struct KeyRecord *KeyListTraverser;
    int i, pos, found, count = 0; 

    KeyListTraverser = PagePtr->KeyListPtr;
    pos = FindInsertionPosition(KeyListTraverser, 
            key, &found, PagePtr->NumKeys, count);
    for (i = 0; i < pos - 1; ++i) {
        KeyListTraverser = KeyListTraverser->Next;
    }

    if (found == TRUE) {
        return KeyListTraverser;
    } else {  // cannot find the key in the page
        return NULL;
    }
}

int get_successors(char *key, int k) {
    struct KeyRecord *KeyListTraverser;

    // check parameters first
    if (k <= 0) {
        printf("k should be positive not %d\n", k);
        return -1;
    }

    /* Print an error message if strlen(key) > MAXWORDSIZE */
    if (strlen(key) > MAXWORDSIZE) {
        printf("ERROR in \"search\":  Length of key Exceeds Maximum Allowed\n");
        printf(" and key May Be Truncated\n");
    }
    if (iscommon(key)) {
        printf("\"%s\" is a common word - no searching is done\n", key);
        return -1;
    }
    if (check_word(key) == FALSE) {
        return -1;
    }
    /* turn to lower case, for uniformity */
    strtolow(key);

    /* recursive call to find page number */
    const PAGENO page = treesearch_page(ROOT, key);
    /* from page number we traverse the leaf page */
    struct PageHdr *PagePtr = FetchPage(page);
    // get the key pointer from the page
    KeyListTraverser = findKey(PagePtr, key);
    // check if the key is not in the B-tree
    if (KeyListTraverser == NULL) {
        printf("key: \"%s\" does not exist\n", key);
        return -1;
    }
    char** result = (char**)calloc(k, sizeof(char*));
    int i;
    for (i = 0; i < k; ++i) {
        result[i] = (char*)calloc(MAXWORDSIZE, sizeof(char));
    }
    struct PageHdr* nextPagePtr = NULL;
    // remember to check if k execeed remaining key nums
    for (i = 0; i < k; ++i) {
        KeyListTraverser = KeyListTraverser->Next;
        if (KeyListTraverser == NULL) {
            PAGENO page = PagePtr->PgNumOfNxtLfPg;
            if (page < 1 || page > FindNumPagesInTree()) {
                break;
            }
            nextPagePtr = FetchPage(PagePtr->PgNumOfNxtLfPg);
            KeyListTraverser = nextPagePtr->KeyListPtr;
            FreePage(PagePtr);  // free space
            PagePtr = nextPagePtr;
        }
        printf("%s\n", KeyListTraverser->StoredKey);
        strncpy(result[i], KeyListTraverser->StoredKey, KeyListTraverser->KeyLen);
    }

    // print out the result
    printf("found %d successors:\n", i);
    int j;
    for (j = 0; j < i; ++j) {
        printf("%s\n", result[j]);
        free(result[j]);
    }
    free(result);

    // free space
    if (nextPagePtr != NULL) {
        free(nextPagePtr);
    }

    return 0;
}
