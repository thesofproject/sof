#ifndef __AW_API_PUBLIC_H__
#define __AW_API_PUBLIC_H__

uint64_t sktune_api_get_size(void);
int32_t sktune_api_init(void *env_ptr);
int32_t sktune_api_end(void *env_ptr);
int32_t sktune_api_process(void *env_ptr,void *data_buf,void* iv_data_buf);
int32_t sktune_api_set_media_info(void *env_ptr,void *info);
int32_t sktune_api_set_params(void *env_ptr, char *params, uint32_t params_size);
int32_t sktune_api_get_params(void *env_ptr, char *params, uint32_t params_size);

int32_t sktune_api_get_vmax(void *env_ptr, int32_t* vmax, int32_t channel);
int32_t sktune_api_set_vmax(void *env_ptr, int32_t vmax, int32_t channel);
int32_t sktune_api_get_start_cali_cfg(void *env_ptr, void *params, uint32_t params_size, int32_t channel);
int32_t sktune_api_set_start_cali_cfg(void *env_ptr, void *params, uint32_t params_size, int32_t channel);
int32_t sktune_api_get_cali_re(void *env_ptr, int32_t* Re, int32_t channel);
int32_t sktune_api_set_cali_re(void *env_ptr, int32_t Re,int32_t channel);
int32_t sktune_api_get_cali_f0(void *env_ptr, int32_t* f0, int32_t channel);
int32_t sktune_api_get_cali_data(void *env_ptr, void *params, uint32_t params_size,int32_t channel);
int32_t sktune_api_set_noise(void *env_ptr, int32_t is_enable,int32_t channel);
int32_t sktune_api_set_data(void *env_ptr, uint32_t params_id, char *data, uint32_t data_len);
int32_t sktune_api_get_data(void *env_ptr, uint32_t params_id, char *data_ptr, uint32_t data_len);
int32_t sktune_api_set_spin_mode(void *env_ptr, uint32_t *mode);
int32_t sktune_api_get_spin_mode(void *env_ptr, uint32_t *mode);

#endif
