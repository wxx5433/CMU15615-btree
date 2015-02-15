/*
 * Search for up to k predecessors of the given key in the 
 * B+ tree, sorted lexicographically. 
 * The key is not included in the result. 
 * If there are less than k predecessors, the result should
 * still return them. 
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
extern int FindInsertionPosition(struct KeyRecord *KeyListTraverser, 
        char *Key, int *Found, NUMKEYS NumKeys, int Count);
extern PAGENO FindNumPagesInTree(void);
void searchLeafPage(char** result, struct PageHdr* pagePtr, int offset, int* k);


/*
 * Recursively search down the tree from the root to leaf
 * until find the page where the key should reside. 
 * At the same time, save the page information and the key's 
 * offset into the pageRecord.
 */
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

    if (NumKeys > 1) {   
        if (Result == 2) { /* New key > stored key:  keep searching */
            KeyListTraverser = KeyListTraverser->Next;
            FindPageRecord(pageRecord, PagePtr, KeyListTraverser, 
                    Key, NumKeys - 1);
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

/*
 * recursively call to find the page in which the key reside
 * and return the page number (guaranteed to be a leaf page).
 * At the same time, store the path into pageRecord linkedList,
 * since we need to backtrace the path later.
 */
PAGENO getPageNum(PageRecord* pageRecord, PAGENO PageNo, char *key) {
    PAGENO result = 0;
    struct PageHdr *PagePtr = FetchPage(PageNo);
    PageRecord* nextPageRecord;
    if (IsLeaf(PagePtr)) { /* found leaf */
        result = PageNo;
    } else if ((IsNonLeaf(PagePtr)) && (PagePtr->NumKeys == 0)) {
        /* keys, if any, will be stored in Page# 2
           THESE PIECE OF CODE SHOULD GO soon! **/
        printf("should not come here\n");
        result = getPageNum(pageRecord, FIRSTLEAFPG, key);
    } else if ((IsNonLeaf(PagePtr)) && (PagePtr->NumKeys > 0)) {
        nextPageRecord = (PageRecord*)malloc(sizeof(PageRecord));
        FindPageRecord(nextPageRecord, PagePtr, PagePtr->KeyListPtr, 
                key, PagePtr->NumKeys);
        nextPageRecord->prev = pageRecord;
        nextPageRecord->next = pageRecord->next;
        pageRecord->next->prev = nextPageRecord;
        pageRecord->next = nextPageRecord;
        PAGENO ChildPage = nextPageRecord->targetPageNo;
        result = getPageNum(nextPageRecord, ChildPage, key);
    } else {
        assert(0 && "this should never happen");
    }
    return result;
}

/* 
 * store the page's keys(starting from keyPtr) to the result array. 
 */
void storeOnePageResult(struct KeyRecord* keyPtr, char** result, int k, int keyNum) {
    while (keyNum > 0) {
        strcpy(result[k - keyNum], keyPtr->StoredKey);
        keyPtr = keyPtr->Next;
        --keyNum;
    }
}

/*
 * If pagePtr is leaf page, then store keys from right to left
 * into result array. 
 * Otherwise, continue search down the tree. 
 */
void searchLeafPageHelper(struct PageHdr* pagePtr, char** result, 
        int* k, struct KeyRecord* keyPtr) {
    int diff;
    if (IsLeaf(pagePtr)) {  // leaf page, store keys into result
        keyPtr = pagePtr->KeyListPtr;
        if (pagePtr->NumKeys >= *k) {  // we have found enough predecessors!
            diff = pagePtr->NumKeys - *k;
            while (diff > 0) {
                keyPtr = keyPtr->Next;
                --diff;
            }
            storeOnePageResult(keyPtr, result, *k, *k);
            *k = 0;
        } else {
            storeOnePageResult(pagePtr->KeyListPtr, result, *k, pagePtr->NumKeys);
            *k -= pagePtr->NumKeys;
        }
    } else {  // not leaf page, continue searching down
        searchLeafPage(result, pagePtr, -1, k);
    }
}

/*
 * Search down the tree to put precessors keys into result array 
 * from RIGHT to LEFT. 
 * If offset is -1, start from the last pagePtr's key entry. 
 */
void searchLeafPage(char** result, struct PageHdr* pagePtr, int offset, int* k) {
    struct PageHdr* newPagePtr;
    if (offset == -1) {
        offset = pagePtr->NumKeys;
        // search the last key entry
        newPagePtr = FetchPage(pagePtr->PtrToFinalRtgPg);
        searchLeafPageHelper(newPagePtr, result, k, newPagePtr->KeyListPtr);
        FreePage(newPagePtr);
    }
    // store all keys in this page
    struct KeyRecord** prevKeys = (struct KeyRecord**)calloc(
            offset, sizeof(struct KeyRecord*));
    struct KeyRecord* keyPtr = pagePtr->KeyListPtr;
    int i;
    for (i = 0; i < offset; ++i) {
        prevKeys[i] = keyPtr;
        keyPtr = keyPtr->Next;
    }
    // search from the right to left
    int j;
    for (j = offset - 1; j >= 0; --j) {
        newPagePtr = FetchPage(prevKeys[j]->PgNum);
        searchLeafPageHelper(newPagePtr, result, k, newPagePtr->KeyListPtr);
        FreePage(newPagePtr);  // do not forget to free it!
        if (k == 0) {  // have found enough predecessors!
            return;
        }
    }
    free(prevKeys);  // free the space!
}

/*
 * Backtrace from the leaf page where the key reside to root
 * until find k predecessors, or there is no predecessors any more
 */
int findParentPage(PageRecord* dummyNode, PageRecord* pageRecord, char** result, int k) {
    // no more predecessors keys to find
    if (pageRecord == dummyNode) {  
        return k;  
    }
    struct PageHdr* pagePtr = pageRecord->pagePtr;
    int offset = pageRecord->offset;
    // start from the page and search down again
    searchLeafPage(result, pagePtr, offset, &k);
    if (k == 0) {
        return 0;
    }
    // continue search up since we have not found enough predecessors yet
    return findParentPage(dummyNode, pageRecord->prev, result, k);
}

int get_predecessors(char *key, int k) {
    // check parameters first
    if (k <= 0) {
        printf("k should be positive not %d", k);
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

    // Use a circular doubly linkedList to store path
    // from root to leaf page that the key reside.
    PageRecord* dummyNode = (PageRecord*)malloc(sizeof(PageRecord)); 
    dummyNode->next = dummyNode;
    dummyNode->prev = dummyNode;

    // get the leaf page number where the key should reside, and
    // save the path from root to leaf at the same time.
    const PAGENO page = getPageNum(dummyNode, ROOT, key);
    struct PageHdr *pagePtr = FetchPage(page);
    int pos, found, count = 0;
    pos = FindInsertionPosition(pagePtr->KeyListPtr, key, 
            &found, pagePtr->NumKeys, count) - 1;
    if (found != TRUE) {
        printf("key: \"%s\": not found\n", key);
        return -1;
    }

    // malloc space to store results
    char** result = (char**)calloc(k, sizeof(char*));
    int originalK = k;
    int i;
    for (i = 0; i < k; ++i) {
        result[i] = (char*)malloc(sizeof(char*));
    }
    struct KeyRecord* keyPtr = pagePtr->KeyListPtr;
    int resultStart;  // to record the actual start of result
    if (pos >= k) {  // easy case: in the same leaf page
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
        // backtrace 
        resultStart = findParentPage(dummyNode, curPageRecord, result, k);
    }
    
    // output result
    printf("found %d predecessors:\n", originalK - resultStart);
    while (resultStart < originalK) {
        printf("%s\n", result[resultStart++]);
    }

    // free all space now!! Yeah! We are done!
    FreePage(pagePtr);
    PageRecord* pageRecordPtr = dummyNode->next;
    PageRecord* nextPageRecordPtr; 
    while (pageRecordPtr != dummyNode) {
        nextPageRecordPtr = pageRecordPtr->next;
        FreePage(pageRecordPtr->pagePtr);  // do not forget!
        free(pageRecordPtr);
        pageRecordPtr = nextPageRecordPtr;
    }
    free(dummyNode);
    for (i = 0; i < originalK; ++i) {
        free(result[i]);
    }
    free(result);

    return 0;
}
