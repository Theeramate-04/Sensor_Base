#ifndef NVS_FUNCTION_H
#define NVS_FUNCTION_H

void init_nvs(void);
esp_err_t NVS_Read(const char *data, const char *Get_Data);
esp_err_t NVS_Read(const char *data, int *Get_Data);
void NVS_Write(const char *data,const char *write_string);
void NVS_Write(const char *data,int write_string);

#endif 