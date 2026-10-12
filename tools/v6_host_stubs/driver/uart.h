#pragma once
#include <cstddef>
#include <cstdint>
using uart_port_t=int;constexpr int UART_NUM_1=1,UART_NUM_2=2;constexpr int ESP_OK=0;
inline int uart_get_buffered_data_len(uart_port_t,size_t *n){*n=0;return ESP_OK;}
inline int uart_wait_tx_done(uart_port_t,uint32_t){return ESP_OK;}
