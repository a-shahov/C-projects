#include "ac_gree.h"

static const char *const TAG = "AC module";
static const char *const generic_key = "a3K8Bx%2r8Y7#xDh";
static const int ac_port = 7000;
static const char *const ac_password = "12345678";
static const char *const ac_ap_ssid = "GR-AC";
static const char *const scan_request = "{\"t\": \"scan\"}";

const char *const ac_parameters_str[] = {
    "Pow", /* 0 - off, 1 - on */ 
    "Mod", /* 0 - auto, 1 - cool, 2 - dry, 3 - fan, 4 - heat */
    "TemUn", /* 0 - Celsius, 1 - Fahrenheit */
    "SetTem", 
    "WdSpd", /* 0 - auto, 1 - low, 2 - medium-low, 3 - medium, 4 - medium-high, 5 - high */
    "Air", /* 0 - off, 1 - on */
    "Blo", /* 0 - off, 1 - on ??? */
    "Health", /* 0 - off, 1 - on */
    "SwhSlp", /* 0 - off, 1 - on */
    "Lig", /* 0 - off, 1 - on */
    "SwingLfRig", /* 0 - default, 1 - full swing, 2-6: fixed position from leftmost to rightmost */
    "SwUpDn",  /* 0 - default, 1 - swing in full range, 2 - fixed in the upmost position (1/5) */
               /* 3 - fixed in the middle-up position (2/5), 4 - fixed in the middle position (3/5) */
               /* 5 - fixed in the middle-low position (4/5), 6 - fixed in the lowest position (5/5) */
               /* 7 - swing in the downmost region (5/5), 8 - swing in the middle-low region (4/5) */
               /* 9 - swing in the middle region (3/5), 10 - swing in the middle-up region (2/5) */
               /* 11 - swing in the upmost region (1/5) */
    "Quiet", /* 0 - off, 1 - on */
    "Tur", /* 0 - off, 1 - on */
    "StHt", /* 0 - off, 1 - on ??? */
    "SvSt" /* 0 - off, 1 - on */
};

/* Global linked list of ac records. It is controlled by functions */
/* add_new_ac_record, delete_ac_record */
static pac_instance ac_list = NULL; 

static inline size_t get_output_length(size_t length)
{
    return (size_t)((((length / 16 + 1) * 16 / 3 + 1) * 3) * 1.33333 + 3);
}

static int ac_encrypt(const char *key, const char *plain_text, size_t in_len, char *output_buffer, size_t out_len)
{
    mbedtls_aes_context aes;
    uint32_t number_blocks;
    uint32_t padding_legth;
    char *tmp_buffer_in = NULL;
    char *tmp_buffer_out = NULL;
    int err; /* 1 - success, 0 - failed */
    size_t olen;

    // the number of blocks that will be encrypted independently of each other
    // here in_len is the length of the string excluding the null character
    number_blocks = in_len / 16 + 1;
    padding_legth = 16 - in_len % 16;

    // is there enough output buffer for base64 encoded string
    if (out_len < get_output_length(in_len)) {
        ESP_LOGI(TAG, "not enough output buffer length");
        return 0;
    }

    tmp_buffer_in = (char*)malloc(number_blocks * 16);
    if (!tmp_buffer_in) {
        ESP_LOGI(TAG, "malloc is failed");
        return 0;
    }

    tmp_buffer_out = (char*)malloc(number_blocks * 16);
    if (!tmp_buffer_out) {
        ESP_LOGI(TAG, "malloc is failed");
        free(tmp_buffer_in);
        return 0;
    }

    memcpy((void*)tmp_buffer_in, (const void*)plain_text, in_len);
    memset((void*)(tmp_buffer_in + (number_blocks - 1) * 16 + in_len % 16), padding_legth, padding_legth);

    mbedtls_aes_init(&aes);
    err = mbedtls_aes_setkey_enc(&aes, (const uint8_t*)key, strlen(key) * 8);
    if (err != 0) {
        ESP_LOGI(TAG, "mbedtls_aes_setkey_enc is failed with error code = 0x%x", err);
        mbedtls_aes_free(&aes);
        free(tmp_buffer_in);
        free(tmp_buffer_out);
        return 0;
    }

    for (int i = 0; i < number_blocks; ++i) {
        err = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, (const uint8_t*)(tmp_buffer_in + i * 16), (uint8_t*)(tmp_buffer_out + i * 16));
        if (err != 0) {
            ESP_LOGI(TAG, "mbedtls_aes_crypt_ecb is failed with error code = 0x%x", err);
            mbedtls_aes_free(&aes);
            free(tmp_buffer_in);
            free(tmp_buffer_out);
            return 0;
        }
    }

    err = mbedtls_base64_encode((uint8_t*)output_buffer, out_len, &olen, (const uint8_t*)tmp_buffer_out, number_blocks * 16);

    if (err != 0) {
        ESP_LOGI(TAG, "mbedtls_base64_encode is failed with error code = 0x%x", err);
    }

    mbedtls_aes_free(&aes);
    free(tmp_buffer_in);
    free(tmp_buffer_out);

    return 1;
}

static int ac_decrypt(const char *key, const char *chipher_text, size_t in_len, char *output_buffer, size_t out_len)
{
    mbedtls_aes_context aes;
    size_t olen;
    char *tmp_buffer_in = NULL;
    uint32_t number_blocks;
    uint32_t padding_legth;
    int err; /* 1 - success, 0 - failed */

    mbedtls_base64_decode(NULL, 0, &olen, (const uint8_t*)chipher_text, in_len);
    if (olen % 16) {
        ESP_LOGI(TAG, "base64 decrypted text is not a multiple of 16");
        return 0;
    }

    if (out_len < olen) {
        ESP_LOGI(TAG, "not enough output buffer length");
        return 0;
    }

    tmp_buffer_in = (char*)malloc(olen);
    if (!tmp_buffer_in) {
        ESP_LOGI(TAG, "malloc is failed");
        return 0;
    }

    err = mbedtls_base64_decode((uint8_t*)tmp_buffer_in, olen, &olen, (const uint8_t*)chipher_text, in_len);
    if (err != 0) {
        ESP_LOGI(TAG, "mbedtls_base64_decode is failed with error code = 0x%x", err);
        free(tmp_buffer_in);
        return 0;
    }

    mbedtls_aes_init(&aes);
    err = mbedtls_aes_setkey_dec(&aes, (const uint8_t*)key, strlen(key) * 8);
    if (err != 0) {
        ESP_LOGI(TAG, "mbedtls_aes_setkey_dec is failed with error code = 0x%x", err);
        mbedtls_aes_free(&aes);
        free(tmp_buffer_in);
        return 0;
    }

    number_blocks = olen / 16;

    for (int i = 0; i < number_blocks; ++i) {
        err = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, (const uint8_t*)(tmp_buffer_in + i * 16), (uint8_t*)(output_buffer + i * 16));
        if (err != 0) {
            ESP_LOGI(TAG, "mbedtls_aes_crypt_ecb is failed with error code = 0x%x", err);
            mbedtls_aes_free(&aes);
            free(tmp_buffer_in);
            return 0;
        }
    }

    padding_legth = output_buffer[olen - 1];
    memset((void*)(output_buffer + number_blocks * 16 - padding_legth), '\0', padding_legth);

    mbedtls_aes_free(&aes);
    free(tmp_buffer_in);

    return 1;
}

static int get_aes_key_from_json(const char *mac, const char *message_buffer, size_t buffer_size, char **out_key)
{
    cJSON *json_response = NULL;
    cJSON *pack = NULL;
    cJSON *tcid = NULL;
    cJSON *json_pack_dec = NULL;
    cJSON *ret = NULL;
    cJSON *key = NULL;
    char *pack_dec = NULL;
    int err = 1; /* 1 - success, 0 - failed */

    json_response = cJSON_ParseWithLength(message_buffer, buffer_size);
    if (!json_response) {
        err = 0;
        goto end;
    }

    if (!cJSON_HasObjectItem(json_response, "tcid")) {
        err = 0;
        goto end;
    }

    tcid = cJSON_GetObjectItemCaseSensitive(json_response, "tcid");
    if (!tcid || !cJSON_IsString(tcid) || !strcmp(tcid->valuestring, mac)) {
        err = 0;
        goto end;
    }

    if (!cJSON_HasObjectItem(json_response, "pack")) {
        err = 0;
        goto end;
    }

    pack = cJSON_GetObjectItemCaseSensitive(json_response, "pack");
    if (!pack) {
        err = 0;
        goto end;
    }

    pack_dec = (char*)malloc(strlen(pack->valuestring));
    if (!pack_dec) {
        err = 0;
        goto end;
    }

    err = ac_decrypt(generic_key, (const char*)(pack->valuestring), strlen(pack->valuestring), pack_dec, strlen(pack->valuestring));
    if (!err) {
        free(pack_dec);
        goto end;
    }

    json_pack_dec = cJSON_Parse((const char*)pack_dec);
    if (!json_pack_dec) {
        err = 0;
        free(pack_dec);
        goto end;
    }
    free(pack_dec);

    if (!cJSON_HasObjectItem(json_pack_dec, "r") || !cJSON_HasObjectItem(json_pack_dec, "key")) {
        err = 0;
        goto end;
    }

    ret = cJSON_GetObjectItemCaseSensitive(json_pack_dec, "r");
    if (!ret || !cJSON_IsNumber(ret) || (ret->valueint != 200)) {
        err = 0;
        goto end;
    }

    key = cJSON_GetObjectItemCaseSensitive(json_pack_dec, "key");
    if (!key || !cJSON_IsString(key)) {
        err = 0;
        goto end;
    }

    *out_key = (char*)malloc(strlen(key->valuestring) + 1);
    if (*out_key == NULL) {
        err = 0;
        goto end;
    }
    
    memcpy((void*)(*out_key), (void*)key->valuestring, strlen(key->valuestring) + 1);

end:
    cJSON_Delete(json_response);
    cJSON_Delete(json_pack_dec);
    return err;
}

static int get_mac_from_json_scan(const char *message_buffer, size_t buffer_size, char **out_mac) //реализовать эту функцию 
{
    cJSON *json_response = NULL;
    int err = 1; /* 1 - success, 0 - failed */

}

static int get_mac_from_json_wlan(const char *message_buffer, size_t buffer_size, char **out_mac)
{
    cJSON *json_response = NULL;
    cJSON *ret = NULL;
    cJSON *mac = NULL;
    int err = 1; /* 1 - success, 0 - failed */

    json_response = cJSON_ParseWithLength(message_buffer, buffer_size);
    if (!json_response) {
        err = 0;
        goto end;
    }

    if (!cJSON_HasObjectItem(json_response, "r") || !cJSON_HasObjectItem(json_response, "mac")) {
        err = 0;
        goto end;
    }
    
    ret = cJSON_GetObjectItemCaseSensitive(json_response, "r");
    if (!ret || !cJSON_IsNumber(ret) || (ret->valueint != 200)) {
        err = 0;
        goto end;
    }

    mac = cJSON_GetObjectItemCaseSensitive(json_response, "mac");
    if (!cJSON_IsString(mac) || !(mac->valuestring)) {
        err = 0;
        goto end;
    }
    
    *out_mac = (char*)malloc(strlen(mac->valuestring) + 1);
    if (*out_mac == NULL) {
        err = 0;
        goto end;
    }

    memcpy((void*)(*out_mac), (void*)mac->valuestring, strlen(mac->valuestring) + 1);

end:
    cJSON_Delete(json_response);
    return err;
}

static int check_answer_from_json(const char *message_buffer, size_t buffer_size, const char *key,
                                  const char *mac, control_ac_pair control_pairs[],  uint32_t count_pairs)
{
    cJSON *json_response = NULL;
    cJSON *ret = NULL;
    cJSON *pack = NULL;
    cJSON *tcid = NULL;
    cJSON *json_pack_dec = NULL;
    cJSON *opt = NULL;
    cJSON *p = NULL;
    cJSON *parameter = NULL;
    cJSON *p_value = NULL;
    char *pack_dec = NULL;
    int err = 1; /* 1 - success, 0 - failed */

    json_response = cJSON_ParseWithLength(message_buffer, buffer_size);
    if (!json_response) {
        err = 0;
        goto end;
    }

    tcid = cJSON_GetObjectItemCaseSensitive(json_response, "tcid");
    if (!tcid || !cJSON_IsString(tcid) || !strcmp(tcid->valuestring, mac)) {
        err = 0;
        goto end;
    }

    pack = cJSON_GetObjectItemCaseSensitive(json_response, "pack");
    if (!pack) {
        err = 0;
        goto end;
    }

    pack_dec = (char*)malloc(strlen(pack->valuestring));
    if (!pack_dec) {
        err = 0;
        goto end;
    }

    err = ac_decrypt(key, (const char*)(pack->valuestring), strlen(pack->valuestring), pack_dec, strlen(pack->valuestring));
    if (!err) {
        free(pack_dec);
        goto end;
    }

    json_pack_dec = cJSON_Parse((const char*)pack_dec);
    if (!json_pack_dec) {
        err = 0;
        free(pack_dec);
        goto end;
    }
    free(pack_dec);

    ret = cJSON_GetObjectItemCaseSensitive(json_pack_dec, "ret");
    if (!ret || !cJSON_IsNumber(ret) || (ret->valueint != 200)) {
        err = 0;
        goto end;
    }

    opt = cJSON_GetObjectItemCaseSensitive(json_pack_dec, "opt");
    if (!opt) {
        err = 0;
        goto end;
    }

    p = cJSON_GetObjectItemCaseSensitive(json_pack_dec, "p");
    if (!p) {
        err = 0;
        goto end;
    }

    for (parameter = opt->child, p_value = p->child; (parameter != NULL) && (p_value != NULL); parameter = parameter->next, p_value = p_value->next) {
        bool match = false;
        for (uint32_t i = 0; i < count_pairs; ++i) {
            if (0 == strcmp(ac_parameters_str[control_pairs[i].parameter], parameter->valuestring)) {
                match = true;
                if (control_pairs[i].value != p_value->valueint) {
                    err = 0;
                    goto end;
                }
            }
        }
        if (!match) {
            err = 0;
            goto end;
        }
    }

end:
    cJSON_Delete(json_response);
    cJSON_Delete(json_pack_dec);
    return err;
}

static pac_instance get_ac_record(const char *mac)
{
    pac_instance current_ac = ac_list;

    while (current_ac) {
        if (strcmp(mac, current_ac->mac) == 0) {
            return current_ac;
        }
        current_ac = current_ac->next;
    }

    return NULL;
}

static pac_instance add_new_ac_record()
{
    pac_instance next_ac = NULL;
    pac_instance current_ac = NULL;

    if (!ac_list) {
        ac_list = (pac_instance)malloc(sizeof(ac_instance));
        if (ac_list) {
            ac_list->next = NULL;
            ac_list->prev = NULL;
        }
        return ac_list;
    }

    current_ac = ac_list;
    next_ac = ac_list->next;

    while (next_ac) {
        current_ac = next_ac;
        next_ac = next_ac->next;
    }

    current_ac->next = (pac_instance)malloc(sizeof(ac_instance));
    if (!current_ac->next) {
        return current_ac->next;
    }

    next_ac = current_ac->next;
    next_ac->prev = current_ac;
    next_ac->next = NULL;

    return next_ac;
}

static void delete_ac_record(pac_instance ac_instance)
{
    if (!ac_instance) {
        return;
    }

    pac_instance left_ac_instance = ac_instance->prev;
    pac_instance right_ac_instance = ac_instance->next;

    if (ac_instance->mac) {
        free(ac_instance->mac);
    }

    if (ac_instance->aes_key) {
        free(ac_instance->aes_key);
    }

    if (left_ac_instance) {
        left_ac_instance->next = right_ac_instance;
    } else {
        ac_list = right_ac_instance;
    }

    if (right_ac_instance) {
        right_ac_instance->prev = left_ac_instance;
    }

    free(ac_instance);
}

static int create_bind_json_request(const char *mac, char *out_buffer, size_t out_len)
{
    cJSON *request = NULL;
    cJSON *pack = NULL;
    char *pack_dec = NULL;
    char *pack_enc = NULL;
    int err = 1; /* 1 - success, 0 - failed */

    pack = cJSON_CreateObject();
    if (!pack) {
        err = 0;
        goto end;
    }

    err = cJSON_AddItemToObject(pack, "mac", cJSON_CreateString(mac));
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(pack, "t", cJSON_CreateString("bind"));
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(pack, "uid", cJSON_CreateNumber(0));
    if (!err) {
        goto end;
    }

    pack_dec = (char*)cJSON_PrintUnformatted(pack);
    if (!pack_dec) {
        err = 0;
        goto end;
    }

    pack_enc = (char*)malloc(get_output_length(strlen(pack_dec)));
    if (!pack_enc) {
        free(pack_dec);
        err = 0;
        goto end;
    }

    err = ac_encrypt(generic_key, pack_dec, strlen(pack_dec), pack_enc, get_output_length(strlen(pack_dec)));
    if (!err) {
        free(pack_dec);
        free(pack_enc);
        goto end;
    }
    free(pack_dec);

    request = cJSON_CreateObject();
    if (!request) {
        err = 0;
        free(pack_enc);
        goto end;
    }

    err = cJSON_AddItemToObject(request, "cid", cJSON_CreateString("app"));
    if (!err) {
        free(pack_enc);
        goto end;
    }

    err = cJSON_AddItemToObject(request, "i", cJSON_CreateNumber(1));
    if (!err) {
        free(pack_enc);
        goto end;
    }

    err = cJSON_AddItemToObject(request, "pack", cJSON_CreateString((const char*)pack_enc));
    if (!err) {
        free(pack_enc);
        goto end;
    }
    free(pack_enc);

    err = cJSON_AddItemToObject(request, "t", cJSON_CreateString("pack"));
    if (!err) {
        goto end;
    }
    
    err = cJSON_AddItemToObject(request, "tcid", cJSON_CreateString(mac));
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(request, "uid", cJSON_CreateNumber(0));
    if (!err) {
        goto end;
    }

    err = cJSON_PrintPreallocated(request, out_buffer, out_len, false);
    if (!err) {
        goto end;
    }

end:
    cJSON_Delete(pack);
    cJSON_Delete(request);
    return err;
}

static int create_wlan_json_request(const char *ssid, const char *password, char *out_buffer, size_t out_len)
{
    cJSON *request = NULL;
    int err = 1; /* 1 - success, 0 - failed */

    request = cJSON_CreateObject();
    if (!request) {
        err = 0;
        goto end;
    }

    err = cJSON_AddItemToObject(request, "psw", cJSON_CreateString(password));
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(request, "ssid", cJSON_CreateString(ssid));
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(request, "t", cJSON_CreateString("wlan"));
    if (!err) {
        goto end;
    }

    err = cJSON_PrintPreallocated(request, out_buffer, out_len, false);
    if (!err) {
        goto end;
    }

end:
    cJSON_Delete(request);
    return err;
}

static int create_control_json_request(const char *key, const char *mac, control_ac_pair control_pairs[],  uint32_t count_pairs, char *out_buffer, size_t out_len)
{
    cJSON *request = NULL;
    cJSON *pack = NULL;
    cJSON *opt = NULL;
    cJSON *p = NULL;
    char *pack_dec = NULL;
    char *pack_enc = NULL;
    int err = 1; /* 1 - success, 0 - failed */

    pack = cJSON_CreateObject();
    if (!pack) {
        err = 0;
        goto end;
    }

    opt = cJSON_CreateArray();
    if (!opt) {
        err = 0;
        goto end;
    }

    for (uint32_t i = 0; i < count_pairs; ++i) { 
        err = cJSON_AddItemToArray(opt, cJSON_CreateString(ac_parameters_str[control_pairs[i].parameter]));
        if (!err) {
            goto end;
        }
    }

    err = cJSON_AddItemToObject(pack, "opt", opt);
    if (!err) {
        goto end;
    }

    p = cJSON_CreateArray();
    if (!p) {
        err = 0;
        goto end;
    }

    for (uint32_t i = 0; i < count_pairs; ++i) {
        err = cJSON_AddItemToArray(p, cJSON_CreateNumber(control_pairs[i].value));
        if (!err) {
            goto end;
        }
    }

    err = cJSON_AddItemToObject(pack, "p", p);
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(pack, "t", cJSON_CreateString("cmd"));
    if (!err) {
        goto end;
    }

    pack_dec = (char*)cJSON_PrintUnformatted(pack);
    if (!pack_dec) {
        err = 0;
        goto end;
    }

    pack_enc = (char*)malloc(get_output_length(strlen(pack_dec)));
    if (!pack_enc) {
        free(pack_dec);
        err = 0;
        goto end;
    }

    err = ac_encrypt(key, pack_dec, strlen(pack_dec), pack_enc, get_output_length(strlen(pack_dec)));
    if (!err) {
        free(pack_dec);
        free(pack_enc);
        goto end;
    }
    free(pack_dec);

    request = cJSON_CreateObject();
    if (!request) {
        err = 0;
        free(pack_enc);
        goto end;
    }

    err = cJSON_AddItemToObject(request, "cid", cJSON_CreateString("app"));
    if (!err) {
        free(pack_enc);
        goto end;
    }

    err = cJSON_AddItemToObject(request, "i", cJSON_CreateNumber(0));
    if (!err) {
        free(pack_enc);
        goto end;
    }

    err = cJSON_AddItemToObject(request, "pack", cJSON_CreateString((const char*)pack_enc));
    if (!err) {
        free(pack_enc);
        goto end;
    }
    free(pack_enc);

    err = cJSON_AddItemToObject(request, "t", cJSON_CreateString("pack"));
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(request, "tcid", cJSON_CreateString(mac));
    if (!err) {
        goto end;
    }

    err = cJSON_AddItemToObject(request, "uid", cJSON_CreateNumber(0));
    if (!err) {
        goto end;
    }

    err = cJSON_PrintPreallocated(request, out_buffer, out_len, false);
    if (!err) {
        goto end;
    }

end:
    cJSON_Delete(pack);
    cJSON_Delete(request);
    return err;
}

ppipe_interconnection ac_cli_init()
{
    ppipe_interconnection ac_pipe = (ppipe_interconnection)malloc(sizeof(pipe_interconnection));

    ac_pipe->pipeEventGroup = xEventGroupCreate();
    ac_pipe->pipeQueueHandle = xQueueCreate(1, sizeof(ac_data));
    ac_pipe->pipeMessageBuffer = xMessageBufferCreate(SIZE_MESSAGE_BUFFER_AC);
    ac_pipe->pipeMutex = xSemaphoreCreateMutex();

    return ac_pipe;
}

void ac_client(void *arg)
{
    EventGroupHandle_t acEventGroup = (((ppipe_interconnection*)arg)[0])->pipeEventGroup;
    QueueHandle_t acQueueHandle = (((ppipe_interconnection*)arg)[0])->pipeQueueHandle;
    MessageBufferHandle_t acMessageBuffer = (((ppipe_interconnection*)arg)[0])->pipeMessageBuffer;

    EventGroupHandle_t WiFiEventGroup = (((ppipe_interconnection*)arg)[1])->pipeEventGroup;
    QueueHandle_t WiFiQueueHandle = (((ppipe_interconnection*)arg)[1])->pipeQueueHandle;
    SemaphoreHandle_t WiFiMutex = (((ppipe_interconnection*)arg)[1])->pipeMutex;

    EventGroupHandle_t UDPEventGroup = (((ppipe_interconnection*)arg)[2])->pipeEventGroup;
    QueueHandle_t UDPQueueHandle = (((ppipe_interconnection*)arg)[2])->pipeQueueHandle;
    MessageBufferHandle_t UDPMessageBuffer = (((ppipe_interconnection*)arg)[2])->pipeMessageBuffer;
    SemaphoreHandle_t UDPMutex = (((ppipe_interconnection*)arg)[2])->pipeMutex;

    udp_data my_udp_data;
    wifi_data my_wifi_data;
    ac_data my_ac_data;
    pac_instance current_ac = NULL;
    EventBits_t bits;
    size_t received_bytes, sent_bytes;
    bool send_data_to_Q_in_end = false; 

    void *message_buffer_udp = NULL;
    void *message_buffer_ac = NULL; 
    
    while (true) {
        bits = xEventGroupWaitBits(acEventGroup, BINDING_WITH_AC | SEND_CONTROL_TO_AC | SEND_WLAN_TO_AC | SCAN_AC, pdTRUE, pdFALSE, portMAX_DELAY);
        send_data_to_Q_in_end = false;
        if (bits & SEND_WLAN_TO_AC) {
            if (xQueueReceive(acQueueHandle, &my_ac_data, 0)) {
                /* First step connect to Access Point of AC */
                memset(&my_wifi_data, 0, sizeof(wifi_data));
                strcpy((char*)my_wifi_data.ssid, ac_ap_ssid);
                strcpy((char*)my_wifi_data.password, ac_password);
                my_wifi_data.ceaseless_conn = false;
                my_wifi_data.number_attempts = 10;
                my_wifi_data.need_response = true;
                
                if (!xSemaphoreTake(WiFiMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(WiFiMutex) in SEND_WLAN_TO_AC");
                    goto failed;
                }

                xQueueReset(WiFiQueueHandle);
                xQueueSendToBack(WiFiQueueHandle, &my_wifi_data, 0);
                xEventGroupSetBits(WiFiEventGroup, SCAN_AND_CONNECT_TO_AP);
                bits = xEventGroupWaitBits(WiFiEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY);
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(WiFiEventGroup) in first step in SEND_WLAN_TO_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }
                if (my_wifi_data.need_response) { /* my_wifi_data.need_response is always true */
                    memset(&my_wifi_data, 0, sizeof(wifi_data));
                    if (!xQueueReceive(WiFiQueueHandle, &my_wifi_data, 0)) {
                        ESP_LOGI(TAG, "xQueueReceive(WiFiQueueHandle) in first step in SEND_WLAN_TO_AC");
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                }

                /* Second step send ssid and password of new Access Point to AC */
                memset(&my_udp_data, 0, sizeof(my_udp_data)); 
                my_udp_data.port = ac_port;
                my_udp_data.need_response = true;
                memcpy((void*)my_udp_data.ip, (const void*)my_wifi_data.broadcast, IP4_MAX_SIZE);
                 //ОШИБКА!
                if (!create_wlan_json_request(my_ac_data.ssid, my_ac_data.password, message_buffer_udp, sizeof(message_buffer_udp))) {
                    ESP_LOGI(TAG, "create_wlan_json_request in SEND_WLAN_TO_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (!xSemaphoreTake(UDPMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(UDPMutex) in SEND_WLAN_TO_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                xQueueReset(UDPQueueHandle);
                xQueueSendToBack(UDPQueueHandle, &my_udp_data, 0);
                xMessageBufferReset(UDPMessageBuffer);
                sent_bytes = xMessageBufferSend(UDPMessageBuffer, (void*)message_buffer_udp, strlen((const char*)message_buffer_udp) + 1, 0);
                if (sent_bytes != strlen((char*)message_buffer_udp) + 1) {
                    ESP_LOGI(TAG, "sent_bytes != strlen((char*)message_buffer_udp) + 1 in second step in SEND_WLAN_TO_AC");
                    ESP_LOGI(TAG, "sent_bytes = %d, strlen = %d", sent_bytes, strlen((char*)message_buffer_udp) + 1);
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }
                xEventGroupSetBits(UDPEventGroup, SEND_MESSAGE);
                bits = xEventGroupWaitBits(UDPEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY);
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(UDPEventGroup) in second step in SEND_WLAN_TO_AC");
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (my_udp_data.need_response) { /* my_udp_data.need_response is always true */
                    received_bytes = xMessageBufferReceive(UDPMessageBuffer, (void*)message_buffer_udp, sizeof(message_buffer_udp), 0);
                    if (!received_bytes) {
                        ESP_LOGI(TAG, "received_bytes = 0 in second step in SEND_WLAN_TO_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }

                    current_ac = add_new_ac_record();
                    if (!current_ac) {
                        ESP_LOGI(TAG, "current_ac is NULL in second step in SEND_WLAN_TO_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }

                    if (!get_mac_from_json_wlan((const char*)message_buffer_udp, received_bytes, &(current_ac->mac))) {
                        ESP_LOGI(TAG, "get_mac_from_json_wlan is failed in second step in SEND_WLAN_TO_AC");
                        delete_ac_record(current_ac);
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }

                    xMessageBufferReset(acMessageBuffer);
                    sent_bytes = xMessageBufferSend(acMessageBuffer, (void*)current_ac->mac, strlen(current_ac->mac) + 1, 0);
                    if (sent_bytes != strlen(current_ac->mac) + 1) {
                        ESP_LOGI(TAG, "mac = %s", current_ac->mac);
                        ESP_LOGI(TAG, "sent_bytes = %d, strlen = %d", sent_bytes, strlen(current_ac->mac) + 1);
                        ESP_LOGI(TAG, "xMessageBufferSend(acMessageBuffer) is failed in second step in SEND_WLAN_TO_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                }

                xSemaphoreGive(UDPMutex);
                xSemaphoreGive(WiFiMutex);
                xEventGroupSetBits(acEventGroup, OPERATION_SUCCESS);
            } else {
                ESP_LOGI(TAG, "xQueueReceive(acQueueHandle) is failed in SEND_WLAN_TO_AC");
                goto failed;
            }
        } else if (bits & SEND_CONTROL_TO_AC) {
            if (xQueueReceive(acQueueHandle, &my_ac_data, 0)) {
                /* First step connect to AP */
                /* in this case in message_buffer_ac contains a mac */
                if (!custom_allocate_memory_for_buffer(acMessageBuffer, &message_buffer_ac, &received_bytes)) {
                    ESP_LOGI(TAG, "custom_allocate_memory_for_buffer is failed in SEND_CONTROL_TO_AC, first message");
                    goto failed;
                }
                received_bytes = xMessageBufferReceive(acMessageBuffer, message_buffer_ac, received_bytes, 0);
                if (!received_bytes) {
                    ESP_LOGI(TAG, "received_bytes = 0 in SEND_CONTROL_TO_AC, first message");
                    goto failed;
                }

                current_ac = get_ac_record((const char*)message_buffer_ac);
                if (!current_ac) {
                    ESP_LOGI(TAG, "current_ac is NULL in BINDING_WITH_AC");
                    goto failed;
                }

                /* in this case in message buffer contains control_ac_pairs */
                if (!custom_allocate_memory_for_buffer(acMessageBuffer, &message_buffer_ac, &received_bytes)) {
                    ESP_LOGI(TAG, "custom_allocate_memory_for_buffer is failed in SEND_CONTROL_TO_AC, second message");
                    goto failed;
                }
                received_bytes = xMessageBufferReceive(acMessageBuffer, message_buffer_ac, received_bytes, 0);
                if (!received_bytes) {
                    ESP_LOGI(TAG, "received_bytes = 0 in SEND_CONTROL_TO_AC, second message");
                    goto failed;
                }
                uint32_t count_pairs = received_bytes / sizeof(control_ac_pair);

                memset(&my_wifi_data, 0, sizeof(wifi_data));
                strcpy((char*)my_wifi_data.ssid, (const char*)my_ac_data.ssid);
                strcpy((char*)my_wifi_data.password, (const char*)my_ac_data.password);
                my_wifi_data.ceaseless_conn = true;
                my_wifi_data.number_attempts = 10;
                my_wifi_data.need_response = false;

                if (!xSemaphoreTake(WiFiMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(WiFiMutex) in SEND_CONTROL_TO_AC");
                    goto failed;
                }

                xQueueReset(WiFiQueueHandle);
                xQueueSendToBack(WiFiQueueHandle, &my_wifi_data, 0);
                xEventGroupSetBits(WiFiEventGroup, CONNECT_TO_AP);
                bits = xEventGroupWaitBits(WiFiEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY); 
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(WiFiEventGroup) in first step in SEND_CONTROL_TO_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                memset(&my_udp_data, 0, sizeof(my_udp_data)); 
                my_udp_data.port = ac_port;
                my_udp_data.need_response = true;
                memcpy((void*)my_udp_data.ip, (const void*)current_ac->ip, IP4_MAX_SIZE);

                message_buffer_udp = malloc(SIZE_MESSAGE_BUFFER_UDP - 4);
                if (!message_buffer_udp) {
                    ESP_LOGI(TAG, "message_buffer_udp is NULL in SEND_CONTROL_TO_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (!create_control_json_request((const char*)current_ac->aes_key, (const char*)current_ac->mac, 
                                                (pcontrol_ac_pair)message_buffer_ac, count_pairs, 
                                                (char*)message_buffer_udp, SIZE_MESSAGE_BUFFER_UDP - 4)) {
                    ESP_LOGI(TAG, "create_control_json_request is failed in SEND_CONTROL_TO_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (!xSemaphoreTake(UDPMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(UDPMutex) in SEND_CONTROL_TO_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                xQueueReset(UDPQueueHandle);
                xQueueSendToBack(UDPQueueHandle, &my_udp_data, 0);
                xMessageBufferReset(UDPMessageBuffer);
                sent_bytes = xMessageBufferSend(UDPMessageBuffer, message_buffer_udp, strlen((char*)message_buffer_udp) + 1, 0);
                if (sent_bytes != strlen((char*)message_buffer_udp) + 1) {
                    ESP_LOGI(TAG, "sent_bytes != strlen((char*)message_buffer_udp) + 1 in SEND_CONTROL_TO_AC");
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }
                xEventGroupSetBits(UDPEventGroup, SEND_MESSAGE);
                bits = xEventGroupWaitBits(UDPEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY); 
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(UDPEventGroup) in SEND_CONTROL_TO_AC");
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (my_udp_data.need_response) { /* my_udp_data.need_response is always true */
                    if (!custom_allocate_memory_for_buffer(UDPMessageBuffer, &message_buffer_udp, &received_bytes)) {
                        ESP_LOGI(TAG, "custom_allocate_memory_for_buffer is failed in SEND_CONTROL_TO_AC in my_udp_data.need_response");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                    received_bytes = xMessageBufferReceive(UDPMessageBuffer, message_buffer_udp, received_bytes, 0);
                    if (!received_bytes) {
                        ESP_LOGI(TAG, "received_bytes = 0 in SEND_CONTROL_TO_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }

                    if (!check_answer_from_json((const char*)message_buffer_udp, received_bytes, (const char*)current_ac->aes_key, 
                                                (const char*)current_ac->mac, (pcontrol_ac_pair)message_buffer_ac, count_pairs)) {
                        ESP_LOGI(TAG, "check_answer_from_json is failed in SEND_CONTROL_TO_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                }

                xSemaphoreGive(UDPMutex);
                xSemaphoreGive(WiFiMutex);
                xEventGroupSetBits(acEventGroup, OPERATION_SUCCESS);
            } else {
                ESP_LOGI(TAG, "xQueueReceive(acQueueHandle) is failed in SEND_CONTROL_TO_AC");
                goto failed;
            }
        } else if (bits & BINDING_WITH_AC) {
            if (xQueueReceive(acQueueHandle, &my_ac_data, 0)) { 
                /* First step connect to AP */
                /* in this case in message_buffer_ac contains a mac */
                if (!custom_allocate_memory_for_buffer(acMessageBuffer, &message_buffer_ac, &received_bytes)) {
                    ESP_LOGI(TAG, "custom_allocate_memory_for_buffer is failed in BINDING_WITH_AC");
                    goto failed;
                }
                received_bytes = xMessageBufferReceive(acMessageBuffer, message_buffer_ac, received_bytes, 0);
                if (!received_bytes) {
                    ESP_LOGI(TAG, "received_bytes = 0 in BINDING_WITH_AC");
                    goto failed;
                }

                memset(&my_wifi_data, 0, sizeof(wifi_data)); 
                strcpy((char*)my_wifi_data.ssid, (const char*)my_ac_data.ssid);
                strcpy((char*)my_wifi_data.password, (const char*)my_ac_data.password);
                my_wifi_data.ceaseless_conn = true;
                my_wifi_data.number_attempts = 10;
                my_wifi_data.need_response = true;

                memset(&my_ac_data, 0, sizeof(my_ac_data));
                send_data_to_Q_in_end = true;

                current_ac = get_ac_record((const char*)message_buffer_ac);
                if (!current_ac) {
                    ESP_LOGI(TAG, "current_ac is NULL in BINDING_WITH_AC");
                    my_ac_data.valid_mac = false;
                    goto failed;
                }
                my_ac_data.valid_mac = true;

                if (!xSemaphoreTake(WiFiMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(WiFiMutex) in BINDING_WITH_AC");
                    goto failed;
                }

                xQueueReset(WiFiQueueHandle);
                xQueueSendToBack(WiFiQueueHandle, &my_wifi_data, 0);
                xEventGroupSetBits(WiFiEventGroup, CONNECT_TO_AP);
                bits = xEventGroupWaitBits(WiFiEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY); 
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(WiFiEventGroup) in first step in BINDING_WITH_AC");
                    my_ac_data.valid_wifi = false;
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (my_wifi_data.need_response) { /* my_udp_data.need_response is always true */
                    memset(&my_wifi_data, 0, sizeof(wifi_data));
                    if (!xQueueReceive(WiFiQueueHandle, &my_wifi_data, 0)) {
                        ESP_LOGI(TAG, "xQueueReceive(WiFiQueueHandle) in first step in BINDING_WITH_AC");
                        my_ac_data.valid_wifi = false;
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                    my_ac_data.valid_wifi = true;
                }

                memset(&my_udp_data, 0, sizeof(my_udp_data)); 
                my_udp_data.port = ac_port;
                my_udp_data.need_response = true;
                memcpy((void*)my_udp_data.ip, (const void*)current_ac->ip, IP4_MAX_SIZE);

                message_buffer_udp = malloc(SIZE_MESSAGE_BUFFER_UDP - 4);
                if (!message_buffer_udp) {
                    ESP_LOGI(TAG, "message_buffer_udp is NULL in BINDING_WITH_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (!create_bind_json_request((const char*)current_ac->mac, (char*)message_buffer_udp, SIZE_MESSAGE_BUFFER_UDP - 4)) { 
                    ESP_LOGI(TAG, "create_bind_json_request is failed in BINDING_WITH_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (!xSemaphoreTake(UDPMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(UDPMutex) in BINDING_WITH_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                xQueueReset(UDPQueueHandle);
                xQueueSendToBack(UDPQueueHandle, &my_udp_data, 0);
                xMessageBufferReset(UDPMessageBuffer);
                sent_bytes = xMessageBufferSend(UDPMessageBuffer, message_buffer_udp, strlen((char*)message_buffer_udp) + 1, 0);
                if (sent_bytes != strlen((char*)message_buffer_udp) + 1) {
                    ESP_LOGI(TAG, "sent_bytes != strlen((char*)message_buffer_udp) + 1 in BINDING_WITH_AC");
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }
                xEventGroupSetBits(UDPEventGroup, SEND_MESSAGE);
                bits = xEventGroupWaitBits(UDPEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY); 
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(UDPEventGroup) in BINDING_WITH_AC");
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (my_udp_data.need_response) { // my_udp_data.need_response is always true
                    if (!custom_allocate_memory_for_buffer(UDPMessageBuffer, &message_buffer_udp, &received_bytes)) {
                        ESP_LOGI(TAG, "custom_allocate_memory_for_buffer is failed in my_udp_data.need_response in BINDING_WITH_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                    received_bytes = xMessageBufferReceive(UDPMessageBuffer, message_buffer_udp, received_bytes, 0);
                    if (!received_bytes) {
                        ESP_LOGI(TAG, "received_bytes = 0 in BINDING_WITH_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }

                    if (!get_aes_key_from_json((const char*)current_ac->mac, (const char*)message_buffer_udp, received_bytes, &(current_ac->aes_key))) {
                        ESP_LOGI(TAG, "get_aes_key_from_json is failed in BINDING_WITH_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                }

                xSemaphoreGive(UDPMutex);
                xSemaphoreGive(WiFiMutex);
                xEventGroupSetBits(acEventGroup, OPERATION_SUCCESS);
            } else {
                ESP_LOGI(TAG, "xQueueReceive(acQueueHandle) is failed in BINDING_WITH_AC");
                goto failed;
            }
        } else if (bits & SCAN_AC) {
            if (xQueueReceive(acQueueHandle, &my_ac_data, 0)) {

                memset(&my_wifi_data, 0, sizeof(wifi_data)); 
                strcpy((char*)my_wifi_data.ssid, (const char*)my_ac_data.ssid);
                strcpy((char*)my_wifi_data.password, (const char*)my_ac_data.password);
                my_wifi_data.ceaseless_conn = true;
                my_wifi_data.number_attempts = 10;
                my_wifi_data.need_response = true;

                if (!xSemaphoreTake(WiFiMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(WiFiMutex) in SCAN_AC");
                    goto failed;
                }

                xQueueReset(WiFiQueueHandle);
                xQueueSendToBack(WiFiQueueHandle, &my_wifi_data, 0);
                xEventGroupSetBits(WiFiEventGroup, CONNECT_TO_AP);
                bits = xEventGroupWaitBits(WiFiEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY); 
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(WiFiEventGroup) in first step in SCAN_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (my_wifi_data.need_response) { /* my_wifi_data.need_response is always true */
                    memset(&my_wifi_data, 0, sizeof(wifi_data));
                    if (!xQueueReceive(WiFiQueueHandle, &my_wifi_data, 0)) {
                        ESP_LOGI(TAG, "xQueueReceive(WiFiQueueHandle) in first step in SCAN_AC");
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                }

                memset(&my_udp_data, 0, sizeof(my_udp_data)); 
                my_udp_data.port = ac_port;
                my_udp_data.need_response = true;
                memcpy((void*)my_udp_data.ip, (const void*)my_wifi_data.broadcast, IP4_MAX_SIZE);

                if (!xSemaphoreTake(UDPMutex, MAX_TIME_FOR_TAKE_MUTEX)) {
                    ESP_LOGI(TAG, "xSemaphoreTake(UDPMutex) in SCAN_AC");
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                xQueueReset(UDPQueueHandle);
                xQueueSendToBack(UDPQueueHandle, &my_udp_data, 0);
                xMessageBufferReset(UDPMessageBuffer);
                sent_bytes = xMessageBufferSend(UDPMessageBuffer, scan_request, strlen(scan_request) + 1, 0);
                if (sent_bytes != strlen(scan_request) + 1) {
                    ESP_LOGI(TAG, "sent_bytes != strlen(scan_request) + 1 in SCAN_AC");
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }
                xEventGroupSetBits(UDPEventGroup, SEND_MESSAGE);
                bits = xEventGroupWaitBits(UDPEventGroup, OPERATION_SUCCESS | OPERATION_FAILED, pdTRUE, pdFALSE, portMAX_DELAY); 
                if (bits & OPERATION_FAILED) {
                    ESP_LOGI(TAG, "xEventGroupWaitBits(UDPEventGroup) in SCAN_AC");
                    xSemaphoreGive(UDPMutex);
                    xSemaphoreGive(WiFiMutex);
                    goto failed;
                }

                if (my_udp_data.need_response) { /* my_udp_data.need_response is always true */
                    memset(&my_udp_data, 0, sizeof(my_udp_data));
                    if (!xQueueReceive(UDPQueueHandle, &my_udp_data, 0)) {
                        ESP_LOGI(TAG, "xQueueReceive(UDPQueueHandle, &my_udp_data, 0) in SCAN_AC in my_udp_data.need_response");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }

                    if (!custom_allocate_memory_for_buffer(UDPMessageBuffer, &message_buffer_udp, &received_bytes)) {
                        ESP_LOGI(TAG, "custom_allocate_memory_for_buffer is failed in SCAN_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }
                    received_bytes = xMessageBufferReceive(UDPMessageBuffer, message_buffer_udp, received_bytes, 0);
                    if (!received_bytes) {
                        ESP_LOGI(TAG, "received_bytes = 0 in SCAN_AC");
                        xSemaphoreGive(UDPMutex);
                        xSemaphoreGive(WiFiMutex);
                        goto failed;
                    }

                    //Реализовать парсинг ответа!
                }

                xSemaphoreGive(UDPMutex);
                xSemaphoreGive(WiFiMutex);
                xEventGroupSetBits(acEventGroup, OPERATION_SUCCESS);
            } else {
                ESP_LOGI(TAG, "xQueueReceive(acQueueHandle) is failed in SCAN_AC");
                goto failed;
            }
        }
        goto end;
        failed:
        xMessageBufferReset(acMessageBuffer);
        xEventGroupSetBits(acEventGroup, OPERATION_FAILED);
        end:
        free(message_buffer_udp);
        free(message_buffer_ac);
        if (send_data_to_Q_in_end) {
            xQueueSendToBack(acQueueHandle, &my_ac_data, 0);
        }
    }
}