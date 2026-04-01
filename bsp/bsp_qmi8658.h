#ifndef BSP_QMI8658_H
#define BSP_QMI8658_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;
    int16_t gyr_x;
    int16_t gyr_y;
    int16_t gyr_z;
} qmi8658_data_t;

bool bsp_qmi8658_init(void);
bool bsp_qmi8658_read_data(qmi8658_data_t *data);
void bsp_qmi8658_deinit(void);

#endif
