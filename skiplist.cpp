#include "skiplist.h"

// 創建節點
Node *create_node(int level, keyType key, valueType val, stateType state, timeType time, chanceType chance)
{
    Node *p = new_node(level);
    if (!p)
    {
        return NULL;
    }
    p->key = key;
    p->value = val;
    p->state = state;
    p->time = time;
    p->chance = chance;

    return p;
}

// 創建skip_list
skip_list *create_sl()
{
    skip_list *sl = (skip_list *)malloc(sizeof(skip_list));
    if (!sl)
    {
        return NULL;
    }
    sl->big = -1;
    sl->small = INF;
    sl->level = 0; // 初始層數
    sl->num = 0;   // 初始skip_list元素個數

    Node *h = create_node(MAX_L - 1, 0, 0, 0, 0, 0); // 創建頭節點

    if (!h)
    {
        free(sl);
        return NULL;
    }
    sl->head = h;

    // header的next全部清成NULL
    for (int i = 0; i < MAX_L; i++)
    {
        h->next[i] = NULL;
    }
    srand(time(0));
    return sl;
}

// 插入時, 層數隨機
int randomLevel()
{
    int level = 1;
    while (rand() % 2)
    {
        level++;
    }
    level = (MAX_L > level) ? level : MAX_L;
    return level;
}

/*
step1. 查找每層插入位置
step2. 隨機產生層數
step3. 往下插入
*/
bool insert(skip_list *sl, keyType key, valueType val, stateType state, timeType time, chanceType chance)
{
    Node *update[MAX_L];
    Node *q = NULL, *p = sl->head;

    // insert skip_list num ++
    sl->num++;
    // step1. 查找插入位置
    for (int i = sl->level - 1; i >= 0; i--)
    {
        while ((q = p->next[i]) && q->key < key)
        {
            p = q;
        }
        update[i] = p;
    }

    // Update (key存在在這個skip_list)
    if (q && q->key == key)
    {
        sl->num--;
        q->value = val;
        q->time = time;
        return true;
    }

    // step2. 隨機產生層數
    int level = randomLevel();

    // 新生成層數比原來層數高
    if (level > sl->level)
    {
        for (int i = sl->level; i < level; i++)
        {
            update[i] = sl->head;
        }
        sl->level = level;
    }

    q = create_node(level, key, val, state, time, chance);

    if (!q)
    {
        return false;
    }

    // step3. 往下插入
    for (int i = level - 1; i >= 0; i--)
    {
        q->next[i] = update[i]->next[i];
        update[i]->next[i] = q;
    }

    // 更改此skip_list的最大和最小值
    if (key != -1)
    {
        if (key < sl->small)
        {
            sl->small = key;
        }
        if (key > sl->big)
        {
            sl->big = key;
        }
    }

    return true;
}

bool erase(skip_list *sl, keyType key, timeType time)
{
    Node *update[MAX_L];
    Node *q = NULL, *p = sl->head;
    sl->num--;

    // step1. 查找刪除位置
    for (int i = sl->level - 1; i >= 0; i--)
    {
        while ((q = p->next[i]) && q->key < key)
        {
            p = q;
        }
        update[i] = p;
    }

    // 此skip_list不含此刪除元素，則使用insert之erase
    if (!q || (q && q->key != key))
    {
        sl->num++;
        insert(sl, key, -1, 1, time, -1);
        return false;
    }

    for (int i = sl->level - 1; i >= 0; i--)
    {
        if (update[i]->next[i] == q)
        {
            update[i]->next[i] = q->next[i];

            if (sl->head->next[i] == NULL)
            {
                sl->level--;
            }
        }
    }
    free(q);
    q = NULL;
    return true;
}

valueType *search(skip_list *sl, keyType key, timeType time)
{
    Node *q, *p = sl->head;
    q = NULL;

    for (int i = sl->level - 1; i >= 0; i--)
    {
        while ((q = p->next[i]) && q->key < key)
        {
            p = q;
        }
        if (q && (key == q->key) && (q->state != 1))
        {
            q->time = time;
            return &(q->value);
        }
    }
    return NULL;
}

void print_sl(skip_list *sl)
{
    Node *q;
    for (int i = sl->level - 1; i >= 0; --i)
    {
        q = sl->head->next[i];
        cout << "line:" << i + 1 << "\n";
        while (q)
        {
            cout << "key:" << q->key << " value:" << q->value << " state:" << q->state << " time:" << q->time << " |\t";
            q = q->next[i];
        }
        cout << "\n";
    }
}

void sl_free(skip_list *sl)
{
    if (!sl)
    {
        return;
    }
    Node *q = sl->head;
    Node *next;

    while (q)
    {
        next = q->next[0];
        free(q);
        q = next;
    }
    free(sl);
}