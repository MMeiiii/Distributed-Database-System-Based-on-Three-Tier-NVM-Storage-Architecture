#include "skiplist.h"
#include <vector>
#include <algorithm> //sort
#include <sstream>   //stringstream
#include <fstream>   //ifstream

// 各層SSTable大小為 上層一個SSTable元素個數*(上層最大可容SSTable個數/2)
#define MAX_NUM_DRAM_Element 2  // DRAM_SSTable最大可容元素個數
#define MAX_NUM_Hot_Element 2   // Hot_SSTable最大可容元素個數
#define MAX_NUM_Warm_Element 4  // Warm_SSTabble最大可容元素個數
#define MAX_NUM_Cold_Element 16 // Cold_SSTable最大可容元素個數

// 各層SSTable個數(下層為上一層之兩倍)
#define MAX_NUM_DRAM_SSTable 1  // DRAM_Level最大可容SSTable個數
#define MAX_NUM_Hot_SSTable 4   // Hot_Level最大可容SSTable個數
#define MAX_NUM_Warm_SSTable 8  // Warm_Level最大可容SSTable個數
#define MAX_NUM_Cold_SSTable 16 // Cold_Level最大可容SSTable個數

// 總Level層數
#define Total_Level 10

// 時間測量
#define NVM_Seek_time 220
#define NVM_Next 22
#define Disk_Seek_time 2570 // 2570 * 8 = 20560
#define Disk_Next 257

int system_time = 0;

skip_list *sl[Total_Level];             // 各層正指著的skip_list
vector<skip_list *> Level[Total_Level]; // 紀錄各層之skip_list的指標

ofstream output_file; // output_file

bool cmp_key(node a, node b)
{
    return a.key < b.key;
}

bool cmp_time(node a, node b)
{
    return a.time < b.time;
}

// SSTable最多"元素"個數
int Get_MAX_Num_Element(int level)
{
    if (level == 0)
    {
        return MAX_NUM_DRAM_Element;
    }
    else if (level == 1)
    {
        return MAX_NUM_Hot_Element;
    }
    else if (level == 2)
    {
        return MAX_NUM_Warm_Element;
    }
    else
    {
        return MAX_NUM_Cold_Element;
    }
}

// 各層最多"SSTable"個數
int Get_MAX_Num_SSTable(int level)
{
    if (level == 0)
    {
        return MAX_NUM_DRAM_SSTable;
    }
    else if (level == 1)
    {
        return MAX_NUM_Hot_SSTable;
    }
    else if (level == 2)
    {
        return MAX_NUM_Warm_SSTable;
    }
    else
    {
        return MAX_NUM_Cold_SSTable;
    }
}

void Flush(int level, vector<node> temp_list, int start, int end)
{
    sl[level] = create_sl();
    for (int i = start; i < end; i++)
    {
        insert(sl[level], temp_list[i].key, temp_list[i].value, temp_list[i].state, temp_list[i].time, temp_list[i].chance);
        int MAX_Num_Element = Get_MAX_Num_Element(level);

        // SSTable滿了
        if (sl[level]->num == MAX_Num_Element)
        {
            Level[level].push_back(sl[level]);
            sl[level] = create_sl();
        }
    }
    if (sl[level]->num != 0)
    {
        Level[level].push_back(sl[level]);
    }
}

vector<skip_list *> Flush_Overlapped(int level, vector<node> temp_list, int start, int end)
{
    skip_list *sl_temp = create_sl();
    vector<skip_list *> overlapped;
    for (int i = start; i < end; i++)
    {
        insert(sl_temp, temp_list[i].key, temp_list[i].value, temp_list[i].state, temp_list[i].time, temp_list[i].chance);
        int MAX_Num_Element = Get_MAX_Num_Element(level);
        if (sl_temp->num == MAX_Num_Element)
        {
            overlapped.push_back(sl_temp);
            sl_temp = create_sl();
        }
    }
    if (sl_temp->num != 0)
    {
        overlapped.push_back(sl_temp);
    }
    return overlapped;
}

void Delete_Level_Skip_List(int level)
{
    int MAX_Num_SSTable = Get_MAX_Num_SSTable(level);

    // skip_list結構free
    for (int i = 0; i < MAX_Num_SSTable; i++)
    {
        sl_free(Level[level][i]);
    }
    // 此層用vector串接之skip_list*清除
    Level[level].clear();
}

void Level_Reorder(int level, vector<node> temp_list)
{
    int temp_list_num = temp_list.size();

    // 清除原本此層的結構
    Delete_Level_Skip_List(level);

    // 重新為此層建立新skip_list
    sl[level] = create_sl();
    for (int i = 0; i < temp_list_num; i++)
    {
        insert(sl[level], temp_list[i].key, temp_list[i].value, temp_list[i].state, temp_list[i].time, temp_list[i].chance);
        int MAX_Num_Element = Get_MAX_Num_Element(level);

        // SSTable滿了
        if (sl[level]->num == MAX_Num_Element)
        {
            Level[level].push_back(sl[level]);
            sl[level] = create_sl();
        }
    }
    if (sl[level]->num != 0)
    {
        Level[level].push_back(sl[level]);
    }
}

vector<node> Key_Compaction(vector<node> list)
{
    int list_num = list.size();
    vector<node> temp_list, temp_list_2;
    int temp_list_num;
    node temp;

    for (int i = 0; i < list_num; i++)
    {
        int check = 0;
        temp_list_num = temp_list.size();
        for (int j = 0; j < temp_list_num; j++)
        {
            if (list[i].key == temp_list[j].key)
            {
                check = 1;
                // delete
                if (list[i].state == 1)
                {
                    temp_list[j].key = -1;
                }
                // update
                else if (list[i].state == 2)
                {
                    temp_list[j].value = list[i].value;
                    temp_list[j].time = list[i].time;
                }
                // insert(insert-delte-insert)
                else if (list[i].state == 0)
                {
                    check = 0;
                }
            }
        }
        if (check == 0)
        {
            temp.key = list[i].key;
            temp.value = list[i].value;
            temp.state = list[i].state;
            temp.time = list[i].time;
            temp.chance = list[i].chance;
            temp_list.push_back(temp);
        }
    }

    // 對串接的temp_list進行整理(erase的部分刪掉)
    temp_list_num = temp_list.size();
    for (int i = 0; i < temp_list_num; i++)
    {
        if (temp_list[i].key != -1)
        {
            temp_list_2.push_back(temp_list[i]);
        }
    }

    sort(temp_list_2.begin(), temp_list_2.end(), cmp_key);

    return temp_list_2;
}

vector<node> Paste_Node(vector<skip_list *> overlapped_range, vector<node> temp_list)
{
    Node *q;
    int Num_SSTable = overlapped_range.size(); // SSTable個數
    int temp_list_num = temp_list.size();
    vector<node> temp_list_2; // 整個串接

    for (int i = 0; i < Num_SSTable; i++)
    {
        q = overlapped_range[i]->head->next[0];
        node temp;
        while (q)
        {
            temp.key = q->key;
            temp.value = q->value;
            temp.state = q->state;
            temp.time = q->time;
            temp.chance = q->chance;
            temp_list_2.push_back(temp);
            q = q->next[0];
        }
    }
    for (int i = 0; i < temp_list_num; i++)
    {
        temp_list_2.push_back(temp_list[i]);
    }

    return temp_list_2;
}

void Delete_Overlapped_skip_list(vector<skip_list *> overlapped)
{
    int overlapped_num = overlapped.size();

    // skip_list結構free
    for (int i = 0; i < overlapped_num; i++)
    {
        sl_free(overlapped[i]);
    }
}

void Compaction(int level)
{
    vector<node> temp_list;
    int temp_list_num;
    int MAX_Num_Element = Get_MAX_Num_Element(level);
    int MAX_Num_SSTable = Get_MAX_Num_SSTable(level);
    int Flush_Num = Get_MAX_Num_Element(level + 1);

    // step1. 整理上層

    // 1-1串接所有node
    temp_list = Paste_Node(Level[level], temp_list);
    // Hot_Level會有Overlapped需先整理
    if (level == 1)
    {
        temp_list = Key_Compaction(temp_list);
    }
    // 1-2檢查上層元素是否不足
    temp_list_num = temp_list.size();
    if (temp_list_num <= MAX_Num_Element * (MAX_Num_SSTable - 1))
    {
        Level_Reorder(level, temp_list);
        return;
    }

    // step2. 與下層進行合併

    // 2-1下層沒有SSTable
    if (Level[level + 1].empty())
    {
        sort(temp_list.begin(), temp_list.end(), cmp_key);
        Delete_Level_Skip_List(level);
        Flush(level + 1, temp_list, 0, temp_list_num);
        return;
    }

    // 2-2下層有SSTable

    // 對上層的key取range，以確認下層的range
    sort(temp_list.begin(), temp_list.end(), cmp_key);
    int small_key = temp_list[0].key;
    int big_key = temp_list[temp_list_num - 1].key;
    int Down_SSTable_Num = Level[level + 1].size();
    vector<skip_list *> small_skip_list, big_skip_list, overlapped_skip_list; // 下層對比後依range區分skip_list

    // 確認overlapped的SSTable
    for (int i = 0; i < Down_SSTable_Num; i++)
    {
        // 小於此區間
        if (Level[level + 1][i]->big < small_key)
        {
            small_skip_list.push_back(Level[level + 1][i]);
        }
        // 不再此區間
        else if (Level[level + 1][i]->small > big_key)
        {
            big_skip_list.push_back(Level[level + 1][i]);
        }
        else
        {
            overlapped_skip_list.push_back(Level[level + 1][i]);
        }
    }

    temp_list = Paste_Node(overlapped_skip_list, temp_list);

    temp_list = Key_Compaction(temp_list);

    temp_list_num = temp_list.size();

    // 進行Flush
    Delete_Level_Skip_List(level);                     // skip_list + Level(上)
    Delete_Overlapped_skip_list(overlapped_skip_list); // skip_list(下)
    overlapped_skip_list.clear();
    Level[level + 1].clear(); // Level(下)

    sort(temp_list.begin(), temp_list.end(), cmp_key);
    overlapped_skip_list = Flush_Overlapped(level + 1, temp_list, 0, temp_list_num);

    int next_level_num = small_skip_list.size();

    for (int i = 0; i < next_level_num; i++)
    {
        Level[level + 1].push_back(small_skip_list[i]);
        if (Level[level + 1].size() == Get_MAX_Num_SSTable(level + 1))
        {

            Compaction(level + 1);
        }
    }

    next_level_num = overlapped_skip_list.size();

    for (int i = 0; i < next_level_num; i++)
    {
        Level[level + 1].push_back(overlapped_skip_list[i]);
        if (Level[level + 1].size() == Get_MAX_Num_SSTable(level + 1))
        {

            Compaction(level + 1);
        }
    }

    next_level_num = big_skip_list.size();

    for (int i = 0; i < next_level_num; i++)
    {
        Level[level + 1].push_back(big_skip_list[i]);
        if (Level[level + 1].size() == Get_MAX_Num_SSTable(level + 1))
        {

            Compaction(level + 1);
        }
    }
    return;
}

// 檢查Level是否滿了
void check_Level(int level)
{
    // 目前Level的SSTable個數
    int Num_SSTable = Level[level].size();
    int MAX_Num_SSTable = Get_MAX_Num_SSTable(level);

    if (Num_SSTable == MAX_Num_SSTable)
    {
        // 若是DRAM滿可直接Flush
        if (level == 0)
        {
            for (int i = 0; i < Num_SSTable; i++)
            {
                Level[level + 1].push_back(Level[level][i]);
                check_Level(level + 1);
            }

            Level[level].clear();
            return;
        }
        Compaction(level);
    }
    return;
}

// 確認SSTable的Element是否有滿，滿了則將此skip_list加到相應Level的vector
// 接著檢查是否因為加入此SSTable而導致此Level滿了
// level為此SSTable在的層數 DRAM-->0, Hot-->1, Warm-->2, Cold-->3
void check_Element(int level)
{
    int MAX_Num_SSTable_Element = Get_MAX_Num_Element(level);
    // SSTable滿了
    if (sl[level]->num == MAX_Num_SSTable_Element)
    {
        // push到Level，並為這層重新創建一個skip_list
        Level[level].push_back(sl[level]);
        sl[level] = create_sl();

        // 檢查是否因為加入此SSTable而導致此Level滿了
        check_Level(level);
    }
}

void print_Level()
{
    for (int i = 0; i < Total_Level; i++)
    {
        cout << "Level:" << i << "\n";
        if (i == 0)
        {
            print_sl(sl[0]);
        }
        int Num_SSTable = Level[i].size();
        for (int j = 0; j < Num_SSTable; j++)
        {
            cout << "Memtable No." << j + 1 << "\n";
            cout << Level[i][j]->small << " " << Level[i][j]->big << "\n";
            print_sl(Level[i][j]);
        }
        cout << "--------------\n";
    }
}

void Search_Key(valueType key)
{
    ;
    int find_time = 0;

    for (int i = 1; i <= MAX_L; i++)
    {
        // 時間估算
        if (i == 1 || i == 2)
        {
            find_time += NVM_Seek_time + NVM_Next * i;
        }
        else
        {
            find_time += Disk_Seek_time + Disk_Next * i;
        }

        int Num_SSTable = Level[i].size();
        for (int j = 0; j < Num_SSTable; j++)
        {
            valueType *value = search(Level[i][j], key, system_time);
            if (value != NULL)
            {
                output_file << "Key:" << key << " value is " << *value << ". It takes time: " << find_time << "\n";
                return;
            }
        }
    }
}

void DRAM(vector<string> result, int level)
{
    // 創建skip_list
    if (sl[level] == NULL)
    {
        sl[level] = create_sl();
    }

    int key = stoi(result[1]);

    if (result[0] == "Insert")
    {
        int val = stoi(result[2]);
        insert(sl[level], key, val, 0, system_time, 0);
    }
    else if (result[0] == "Update")
    {
        int val = stoi(result[2]);
        insert(sl[level], key, val, 2, system_time, 0);
    }
    else if (result[0] == "Delete")
    {
        erase(sl[level], key, system_time);
    }
    else if (result[0] == "Search")
    {
        Search_Key(key);
    }

    check_Element(level);
}

void Read_Input_File(string input)
{
    ifstream input_file(input);
    string line;
    if (!input_file.is_open())
    {
        cout << "Couldn't open the file of '" << input << "'.\n";
        return;
    }

    output_file.open("output.txt");
    if (!output_file.is_open())
    {
        cout << "Couldn't open the file of output\n";
    }

    // 讀取每一行操作
    while (getline(input_file, line))
    {
        system_time++;
        // 對操作指令進行切割
        vector<string> result;
        stringstream ss(line);
        char d = ' ';
        string tok;

        // 行資訊以空白做分割
        while (getline(ss, tok, d))
        {
            result.push_back(tok);
        }
        DRAM(result, 0);
    }
    input_file.close();
    output_file.close();
}

int main()
{
    string input;
    input = "input.txt";
    Read_Input_File(input);
    print_Level();
}