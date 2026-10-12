#pragma once
#include "FreeRTOS.h"
#include <deque>
#include <vector>
#include <cstring>
struct StaticQueue_t {size_t capacity=0,item_size=0;std::deque<std::vector<uint8_t>> entries;};
using QueueHandle_t=StaticQueue_t*;
inline QueueHandle_t xQueueCreateStatic(size_t n,size_t item,uint8_t*,StaticQueue_t *q){q->capacity=n;q->item_size=item;return q;}
inline int xQueueSend(QueueHandle_t q,const void *item,TickType_t){if(q->entries.size()==q->capacity)return pdFALSE;const auto *b=static_cast<const uint8_t*>(item);q->entries.emplace_back(b,b+q->item_size);return pdTRUE;}
inline int xQueueReceive(QueueHandle_t q,void *item,TickType_t){if(q->entries.empty())return pdFALSE;std::memcpy(item,q->entries.front().data(),q->item_size);q->entries.pop_front();return pdTRUE;}
inline int xQueuePeek(QueueHandle_t q,void *item,TickType_t){if(q->entries.empty())return pdFALSE;std::memcpy(item,q->entries.front().data(),q->item_size);return pdTRUE;}
