﻿#include <dict.h>
#include <assert.h>

struct keypair {
    struct keypair * next;
    unsigned hash;
    void * val;
    char key[];
};

// 销毁结点数据
static inline void _keypair_delete(node_f fdie, struct keypair * pair) {
    if (pair->val && fdie)
        fdie(pair->val);
    free(pair);
}

// 创建结点数据
static inline struct keypair * _keypair_create(unsigned hash, void * v, const char * k) {
    size_t len = strlen(k) + 1;
    struct keypair * pair = malloc(sizeof(struct keypair) + len);
    pair->hash = hash;
    pair->val = v;
    memcpy(pair->key, k, len);
    return pair;
}

//
// 质数表
//
static const unsigned _primes[][2] = {
    { (1<<6)-1  ,         53 },
    { (1<<7)-1  ,         97 },
    { (1<<8)-1  ,        193 },
    { (1<<9)-1  ,        389 },
    { (1<<10)-1 ,        769 },
    { (1<<11)-1 ,       1543 },
    { (1<<12)-1 ,       3079 },
    { (1<<13)-1 ,       6151 },
    { (1<<14)-1 ,      12289 },
    { (1<<15)-1 ,      24593 },
    { (1<<16)-1 ,      49157 },
    { (1<<17)-1 ,      98317 },
    { (1<<18)-1 ,     196613 },
    { (1<<19)-1 ,     393241 },
    { (1<<20)-1 ,     786433 },
    { (1<<21)-1 ,    1572869 },
    { (1<<22)-1 ,    3145739 },
    { (1<<23)-1 ,    6291469 },
    { (1<<24)-1 ,   12582917 },
    { (1<<25)-1 ,   25165843 },
    { (1<<26)-1 ,   50331653 },
    { (1<<27)-1 ,  100663319 },
    { (1<<28)-1 ,  201326611 },
    { (1<<29)-1 ,  402653189 },
    { (1<<30)-1 ,  805306457 },
    { UINT_MAX  , 1610612741 },
};

struct dict {
    node_f fdie;                // 结点注册的销毁函数
    unsigned idx;               // 使用 _primes 质数表索引
    unsigned used;              // 用户已经使用的结点个数
    struct keypair ** table;    // size = primes[idx][0]
};

static void _dict_resize(struct dict * d) {
    unsigned size, prime, i;
    struct keypair ** table;
    unsigned used = d->used;

    if (used < _primes[d->idx][0])
        return;
    
    // 构造新的内存布局大小
    size = _primes[d->idx][1];
    prime = _primes[++d->idx][1];
    table = calloc(prime, sizeof(struct keypair *));

    // 开始转移数据
    for (i = 0; i < size; ++i) {
        struct keypair * pair = d->table[i];
        while (pair) {
            struct keypair * next = pair->next;
            unsigned index = pair->hash % prime;

            pair->next = table[index];
            table[index] = pair;
            pair = next;
        }
    }

    // table 重新变化
    free(d->table);
    d->table = table;
}

//
// dict_create - 字典创建
// fdie     : v 销毁函数
// return   : dict_t
//
inline dict_t 
dict_create(node_f fdie) {
    struct dict * d = malloc(sizeof(struct dict));
    unsigned size = _primes[d->idx = 0][1];
    d->used = 0;
    d->fdie = fdie;
    // 默认构建的第一个素数表 index = 0
    d->table = calloc(size, sizeof(struct keypair *));
    return d;
}

//
// dict_delete - 字典销毁
// d        : dict_create 创建的字典对象
// return   : void 
//
void 
dict_delete(dict_t d) {
    unsigned i, size;
    if (NULL == d) return;
    size = _primes[d->idx][1];
    for (i = 0; i < size; ++i) {
        struct keypair * pair = d->table[i];
        while (pair) {
            struct keypair * next = pair->next;
            _keypair_delete(d->fdie, pair);
            pair = next;
        }
    }
    free(d->table);
    free(d);
}

//
// dict_set - 设置一个 <k, v> 结构
// d        : dict_create 创建的字典对象
// k        : 插入的 key
// v        : 插入数据的值, NULL 会销毁 k
// return   : void
//
void 
dict_set(dict_t d, const char * k, void * v) {
    unsigned hash, index;
    struct keypair * pair, * prev;
    assert(NULL != d && k != NULL);

    // 检查一下内存, 看是否要扩充
    _dict_resize(d);

    // 开始寻找数据
    hash = str_hash(k);
    index = hash % _primes[d->idx][1];
    pair = d->table[index];
    prev = NULL;

    while (pair) {
        // 找见了数据
        if (pair->hash == hash && !strcmp(pair->key, k)) {
            // 相同数据直接返回什么都不操作
            if (pair->val == v)
                return;

            // 删除操作
            if (NULL == v) {
                if (NULL == prev)
                    d->table[index] = pair->next;
                else
                    prev->next = pair->next;

                // 销毁结点, 直接返回
                _keypair_delete(d->fdie, pair);
                return;
            }

            // 更新结点
            if (d->fdie)
                d->fdie(pair->val);
            pair->val = v;
            return;
        }

        prev = pair;
        pair = pair->next;
    }

    // 没有找见设置操作, 直接插入数据
    if (NULL != v) {
        pair = _keypair_create(hash, v, k);
        pair->next = d->table[index];
        d->table[index] = pair;
        ++d->used;
    }
}

//
// dict_get - 获取字典中对映的 v
// d        : dict_create 创建的字典对象
// k        : 查找的 key 
// return   : 查找的 v, NULL 表示没有
//
void * 
dict_get(dict_t d, const char * k) {
    unsigned hash, index;
    struct keypair * pair;
    assert(NULL != d && k != NULL);

    hash = str_hash(k);
    index = hash % _primes[d->idx][1];
    pair = d->table[index];

    while (pair) {
        if (!strcmp(pair->key, k))
            return pair->val;
        pair = pair->next;
    }

    return NULL;
}
