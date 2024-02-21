#ifndef _SKIPLIST_H_
#define _SKIPLIST_H_

#include <iostream>
#include <string>
#include <time.h>
using namespace std;

#define MAX_L 16 // skip_list最大層數
#define INF 0x3f3f3f3f

// new_node為生成一個Node的結構體，並且包含生成n個Node*(依層數生成)
#define new_node(n) ((Node *)malloc(sizeof(Node) + n * sizeof(Node *)))

typedef int keyType;
typedef int valueType;
typedef int timeType;
typedef int chanceType;
typedef int stateType;

typedef struct node
{
    keyType key;          // key值
    valueType value;      // value值
    stateType state;      // 判斷是否為erase或update 0-->not erase and update, 1--> erase, 2--> update
    timeType time;        // time值
    chanceType chance;    // chance值
    struct node *next[1]; // 指向後面node指標
} Node;

typedef struct skip_list
{
    int small;  // skip_list的最小元素
    int big;    // skip_list的最大元素
    int num;    // skip_list的總元素個數
    int level;  // 最大層數
    Node *head; // 指向頭節點
} skip_list;

// create node
Node *create_node(int level, keyType key, valueType val, stateType state, timeType time, chanceType chance);

// create skip_list
skip_list *create_sl();

// 插入元素的時候所占有的層數是隨機，返回隨機層數
int randomLevel();

// sl 跳表指針, key 節點字, val 節點值
bool insert(skip_list *sl, keyType key, valueType val, stateType state, timeType time, chanceType chance);

// sl 跳表指針, key 節點字
bool erase(skip_list *sl, keyType key, timeType time);

valueType *search(skip_list *sl, keyType key, timeType time);

void print_sl(skip_list *sl);

void sl_free(skip_list *sl);
#endif