/*
 * Search for up to k predecessors of the given key in the 
 * B+ tree, sorted lexicographically. 
 * The key is not included in the result. 
 * If there are less than k predecessors, the result should
 * still return them. 
 *
 * TODO need to free pagerecord
 * TODO need to deal with potential memory leak
 *
 * Author: Xiaoxiang Wu
 * Andrew ID: xiaoxiaw
 */
#include <stdio.h>
#include <string.h>
#include "def.h"

typedef struct PageRecord {
    struct PageHdr* pagePtr;
    int offset;
    PAGENO targetPageNo;
    struct PageRecord* prev;
    struct PageRecord* next;
} PageRecord;

extern int iscommon(char *word);
extern int check_word(char *word);
extern int strtolow(char *s);
extern int FreePage(struct PageHdr *PagePtr);
extern struct PageHdr *FetchPage(PAGENO Page);
extern int FindInsertionPosition(struct KeyRecord *KeyListTraverser, char *Key, 
        int *Found, NUMKEYS NumKeys, int Count);
extern PAGENO FindNumPagesInTree(void);
void searchLeafPage(char** result, struct PageHdr* pagePtr, int offset, int* k);

void FindPageRecord(PageRecord* pageRecord, struct PageHdr *PagePtr, 
        struct KeyRecord *KeyListTraverser, char *Key, NUMKEYS NumKeys) {
    /* Auxiliary Definitions */
    int Result;
    char *Word; /* Key stored in B-Tree */
    int CompareKeys(char *Key, char *Word);

    /* Compare the possible new key with key stored in B-Tree */
    Word = KeyListTraverser->StoredKey;
    (*(Word + KeyListTraverser->KeyLen)) = '\0';
    Result = CompareKeys(Key, Word);

    if (NumKeys > 1) {   // change to 1, since I delete NumKeys -= 1
        if (Result == 2) { /* New key > stored key:  keep searching */
            KeyListTraverser = KeyListTraverser->Next;
            FindPageRecord(pageRecord, PagePtr, KeyListTraverser, Key, NumKeys - 1);
        } else {  /* New key <= stored key */
            pageRecord->pagePtr = PagePtr;
            pageRecord->offset = PagePtr->NumKeys - NumKeys;
            pageRecord->targetPageNo = KeyListTraverser->PgNum;
            return; /* return left child */
        }
    } else {  /* This is the last key in this page */
        pageRecord->pagePtr = PagePtr;
        if ((Result == 1) || (Result == 0)) { /* New key <= stored key */
            pageRecord->targetPageNo = KeyListTraverser->PgNum;
            pageRecord->offset = PagePtr->NumKeys - 1;
        }
        else {  /* New key > stored key return rightmost child */
            pageRecord->targetPageNo = PagePtr->PtrToFinalRtgPg;
            pageRecord->offset = PagePtr->NumKeys;
        }
    }
}

/**
 * recursive call to find the page in which the key should reside
 * and return the page number (guaranteed to be a leaf page).
 */
PAGENO searchPage(PageRecord* pageRecord, PAGENO PageNo, char *key) {
    PAGENO result = 0;
    struct PageHdr *PagePtr = FetchPage(PageNo);
    PageRecord* nextPageRecord;
    if (IsLeaf(PagePtr)) { /* found leaf */
        result = PageNo;
    } else if ((IsNonLeaf(PagePtr)) && (PagePtr->NumKeys == 0)) {
        /* keys, if any, will be stored in Page# 2
           THESE PIECE OF CODE SHOULD GO soon! **/
        printf("should no come here\n");
        result = searchPage(pageRecord, FIRSTLEAFPG, key);
    } else if ((IsNonLeaf(PagePtr)) && (PagePtr->NumKeys > 0)) {
        nextPageRecord = (PageRecord*)malloc(sizeof(PageRecord));
        FindPageRecord(nextPageRecord, PagePtr, PagePtr->KeyListPtr, key, PagePtr->NumKeys);
        nextPageRecord->prev = pageRecord;
        nextPageRecord->next = pageRecord->next;
        pageRecord->next->prev = nextPageRecord;
        pageRecord->next = nextPageRecord;
        PAGENO ChildPage = nextPageRecord->targetPageNo;
        result = searchPage(nextPageRecord, ChildPage, key);
    } else {
        assert(0 && "this should never happen");
    }
    // TODO free the page later
    //FreePage(PagePtr);  // do not free at this time
    return result;
}

void storeOnePageResult(struct KeyRecord* keyPtr, char** result, int k, int keyNum) {
    while (keyNum > 0) {
        strncpy(result[k - keyNum], keyPtr->StoredKey, keyPtr->KeyLen);
        keyPtr = keyPtr->Next;
        --keyNum;
    }
}

void mySearch(struct PageHdr* prevPage, char** result, int* k, struct KeyRecord* keyPtr) {
    int diff;
    if (IsLeaf(prevPage)) {
        keyPtr = prevPage->KeyListPtr;
        if (prevPage->NumKeys >= *k) {
            diff = prevPage->NumKeys - *k;
            while (diff > 0) {
                keyPtr = keyPtr->Next;
                --diff;
            }
            storeOnePageResult(keyPtr, result, *k, *k);
            *k = 0;
        } else {
            storeOnePageResult(prevPage->KeyListPtr, result, *k, prevPage->NumKeys);
            *k -= prevPage->NumKeys;
        }
    } else {
        searchLeafPage(result, prevPage, -1, k);
    }
}

void searchLeafPage(char** result, struct PageHdr* pagePtr, int offset, int* k) {
    struct PageHdr* prevPage;
    if (offset == -1) {
        offset = pagePtr->NumKeys;
        prevPage = FetchPage(pagePtr->PtrToFinalRtgPg);
        mySearch(prevPage, result, k, prevPage->KeyListPtr);
        FreePage(prevPage);
    }
    struct KeyRecord** prevKeys = (struct KeyRecord**)calloc(
            offset, sizeof(struct KeyRecord*));
    struct KeyRecord* keyPtr = pagePtr->KeyListPtr;
    int i;
    
    for (i = 0; i < offset; ++i) {
        prevKeys[i] = keyPtr;
        keyPtr = keyPtr->Next;
    }
    int j;
    for (j = offset - 1; j >= 0; --j) {
        struct PageHdr* prevPage = FetchPage(prevKeys[j]->PgNum);
        mySearch(prevPage, result, k, prevPage->KeyListPtr);
        FreePage(prevPage);  // do not forget to free it!
    }
}

int findParentPage(PageRecord* dummyNode, PageRecord* pageRecord, char** result, int k) {
    // no more previous keys to find
    if (pageRecord == dummyNode) {  
        return k;  // TODO should no return 0
    }
    struct PageHdr* pagePtr = pageRecord->pagePtr;
    int offset = pageRecord->offset;
    searchLeafPage(result, pagePtr, offset, &k);
    FreePage(pagePtr);
    if (k == 0) {
        return 0;
    }
    return findParentPage(dummyNode, pageRecord->prev, result, k);
}

int get_predecessors(char *key, int k) {
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

    PageRecord* dummyNode = (PageRecord*)malloc(sizeof(PageRecord)); 
    dummyNode->next = dummyNode;
    dummyNode->prev = dummyNode;

    const PAGENO page = searchPage(dummyNode, ROOT, key);
    struct PageHdr *pagePtr = FetchPage(page);
    struct KeyRecord* firstKey = pagePtr->KeyListPtr;
    int pos, found, count = 0;
    pos = FindInsertionPosition(firstKey, key, &found, 
            pagePtr->NumKeys, count) - 1;
    if (found != TRUE) {
        printf("key: \"%s\" does not exist\n", key);
        return -1;
    }

    char** result = (char**)calloc(k, sizeof(char*));
    int originalK = k;
    int i;
    int resultStart;
    for (i = 0; i < k; ++i) {
        result[i] = (char*)malloc(sizeof(char*));
    }
    struct KeyRecord* keyPtr = firstKey;
    if (pos >= k) {  // in the same leaf page
        int diff = pos - k;
        int j;
        for (j = 0; j < diff; ++j) {
            keyPtr = keyPtr->Next;
        }
        storeOnePageResult(keyPtr, result, k, k);
        resultStart = 0;
    } else {  // need recursively go to parent page
        storeOnePageResult(keyPtr, result, k, pos);
        k -= pos;
        PageRecord* curPageRecord = dummyNode->prev;
        resultStart = findParentPage(dummyNode, curPageRecord, result, k);
    }
    while (resultStart < originalK) {
        printf("%s\n", result[resultStart++]);
    }
    return 0;
}
