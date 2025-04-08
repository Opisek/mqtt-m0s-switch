/****************************************************************************
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

 // Original code by Bouffalo Lab
 // Modified by Opisek

 // Imports

 #include "FreeRTOS.h"
 #include "task.h"
 #include "timers.h"
 #include "mem.h"
 
 #include <lwip/tcpip.h>
 #include <lwip/sockets.h>
 #include <lwip/netdb.h>
 
 #include "bl_fw_api.h"
 #include "wifi_mgmr_ext.h"
 #include "wifi_mgmr.h"
 
 #include "bflb_irq.h"
 #include "bflb_gpio.h"
 
 #include "bl616_glb.h"
 #include "rfparam_adapter.h"
 
 #include "mqtt.h"
 
 #include "board.h"

 #include "config.h"
 
 // Definitions

 #define WIFI_STACK_SIZE  (1536)
 #define TASK_PRIORITY_FW (16)

 // Variables
 
 struct bflb_device_s *gpio;
 
 static TaskHandle_t wifi_fw_task;
 static TaskHandle_t wifi_status_task;
 static TaskHandle_t wifi_connection_task;
 static TaskHandle_t mqtt_status_task;
 static TaskHandle_t mqtt_connection_task;
 volatile uint32_t wifi_state = 0;
 volatile uint32_t wifi_connected = 0;
 volatile uint32_t wifi_connection_result = 0;
 volatile uint32_t mqtt_status = 0;
 volatile uint32_t mqtt_connection_result = 0;
 volatile uint32_t sock_creation_result = 0;
 
 static wifi_conf_t conf = {
     .country_code = CONF_WIFI_COUNTRY,
 };
 
 uint8_t sendbuf[2048]; /* sendbuf should be large enough to hold multiple whole mqtt messages */
 uint8_t recvbuf[1024]; /* recvbuf should be large enough any whole mqtt message expected to be received */
 
 static TaskHandle_t client_daemon;
 int mqtt_sockfd;

 // WiFi
 
 int wifi_start_firmware_task(void) {
     GLB_PER_Clock_UnGate(GLB_AHB_CLOCK_IP_WIFI_PHY | GLB_AHB_CLOCK_IP_WIFI_MAC_PHY | GLB_AHB_CLOCK_IP_WIFI_PLATFORM);
     GLB_AHB_MCU_Software_Reset(GLB_AHB_MCU_SW_WIFI);
 
     extern void interrupt0_handler(void);
     bflb_irq_attach(WIFI_IRQn, (irq_callback)interrupt0_handler, NULL);
     bflb_irq_enable(WIFI_IRQn);
 
     xTaskCreate(wifi_main, (char *)"fw", WIFI_STACK_SIZE, NULL, TASK_PRIORITY_FW, &wifi_fw_task);
 
     return 0;
 }
 
 void wifi_event_handler(uint32_t code) {
     switch (code) {
         case CODE_WIFI_ON_INIT_DONE: {
             wifi_mgmr_init(&conf);
         } break;
         case CODE_WIFI_ON_MGMR_DONE: {
         } break;
         case CODE_WIFI_ON_SCAN_DONE: {
             wifi_mgmr_sta_scanlist();
         } break;
         case CODE_WIFI_ON_CONNECTED: {
             void mm_sec_keydump();
             mm_sec_keydump();
         } break;
         case CODE_WIFI_ON_GOT_IP: {
             wifi_state = 1;
         } break;
         case CODE_WIFI_ON_DISCONNECT: {
             wifi_state = 0;
             wifi_connected = 0;
         } break;
         case CODE_WIFI_ON_AP_STARTED: {
         } break;
         case CODE_WIFI_ON_AP_STOPPED: {
         } break;
         case CODE_WIFI_ON_AP_STA_ADD: {
         } break;
         case CODE_WIFI_ON_AP_STA_DEL: {
         } break;
         default: {
         }
     }
 }
 
 
 void wifiStatus(void *param) {
     bflb_gpio_init(gpio, GPIO_PIN_27, GPIO_OUTPUT | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_0);
 
     // Fast blinking = Connecting
     // Slow blinking = Error connecting
     // Solid = Connected
     while (1) {
         bflb_gpio_reset(gpio, GPIO_PIN_27);
         vTaskDelay(100);
 
         if (!wifi_state) {
             bflb_gpio_set(gpio, GPIO_PIN_27);
             if (wifi_connection_result) vTaskDelay(1000);
             else vTaskDelay(100);
         }
     }
 }
 
 void mqttStatus(void *param) {
     bflb_gpio_init(gpio, GPIO_PIN_28, GPIO_OUTPUT | GPIO_PULLDOWN | GPIO_SMT_EN | GPIO_DRV_0);
 
     // Off = No wifi
     // Fast blinking = Connecting
     // Slow blinking = Error connecting
     // Solid = Connected
     while (1) {
         if (!wifi_state) {
             bflb_gpio_set(gpio, GPIO_PIN_28);
             vTaskDelay(100);
         } else {
             bflb_gpio_reset(gpio, GPIO_PIN_28);
             vTaskDelay(100);
             if (mqtt_status != 2) {
                 if (mqtt_connection_result || sock_creation_result) {
                     bflb_gpio_set(gpio, GPIO_PIN_28);
                     vTaskDelay(1000);
                 } else {
                     bflb_gpio_set(gpio, GPIO_PIN_28);
                     vTaskDelay(100);
                 }
             }
         }
     }
 }
 
 void wifiConnection(void *param) {
     while (1) {
         if (!wifi_connected) {
             wifi_connection_result = wifi_sta_connect(CONF_WIFI_SSID, CONF_WIFI_PASSWORD, NULL, NULL, 1, 0, 0, 1);
             if (!wifi_connection_result) wifi_connected = 1;
         }
         vTaskDelay(3000);
     }
 }
 
 int open_nb_socket(const char* addr, const char* port) {
     struct addrinfo hints = {0};
 
     hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
     hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
     int sockfd = -1;
     int rv;
     struct addrinfo *p, *servinfo;
 
     /* get address information */
     rv = getaddrinfo(addr, port, &hints, &servinfo);
     if(rv != 0) {
         return -1;
     }
 
     /* open the first possible socket */
     for(p = servinfo; p != NULL; p = p->ai_next) {
         sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
         if (sockfd == -1) continue;
 
         /* connect to server */
         rv = connect(sockfd, p->ai_addr, p->ai_addrlen);
         if(rv == -1) {
           close(sockfd);
           sockfd = -1;
           continue;
         }
         break;
     }
 
     /* free servinfo */
     freeaddrinfo(servinfo);
 
     /* make non-blocking */
     if (sockfd != -1) {
         int iMode = 1;
         ioctlsocket(sockfd, FIONBIO, &iMode);
     }
 
     return sockfd;
 }
 
 void client_refresher(void* client);
 
 void kill_mqtt() {
     if (mqtt_sockfd) close(mqtt_sockfd);
     vTaskDelete(client_daemon);
 }
 
 void mqttConnection(void* param) {
     bflb_gpio_init(gpio, GPIO_PIN_16, GPIO_INPUT | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_0);
 
     struct mqtt_client client;
     uint32_t firstMessageSent = 0;
     uint32_t previousMessage = 0;
     uint32_t message;
     uint32_t res;
 
     char message_ha_discovery[1024] = {"{\"name\": \"Contact\", \"state_topic\": \"" CONF_TOPIC_STATE "\", \"availability_topic\": \"" CONF_TOPIC_AVAILABILITY "\", \"payload_available\": \"online\", \"payload_not_available\": \"offline\", \"payload_on\": \"contact\", \"payload_off\": \"clear\", \"retain\": true, \"unique_id\": \"contact\", \"device\": {\"identifiers\": [\"" CONF_DEVICE_ID "\"], \"name\": \"Bedmat\", \"model\": \"" CONF_DEVICE_NAME "\", \"manufacturer\": \"" CONF_DEVICE_MANUFACTURER "\"}}"};
     char message_in_bed[256] = {"contact"};
     char message_not_in_bed[256] = {"clear"};
     char message_available[256] = {"online"};
     char message_not_available[256] = {"offline"};
 
     while (1) {
         if (wifi_connected) {
             switch(mqtt_status) {
                 case 0: // Create socket
                     mqtt_sockfd = open_nb_socket(CONF_MQTT_ADDRESS, "1883");
                     sock_creation_result = mqtt_sockfd < 0;
                     if (!sock_creation_result) mqtt_status = 1;
                     break;
 
                 case 1: // Connect to broker
                     mqtt_init(&client, mqtt_sockfd, sendbuf, sizeof(sendbuf), recvbuf, sizeof(recvbuf), NULL);
                     uint8_t connect_flags = MQTT_CONNECT_CLEAN_SESSION | MQTT_CONNECT_WILL_QOS_0 | MQTT_CONNECT_WILL_RETAIN;
                     res = mqtt_connect(&client, CONF_MQTT_IDENTIFIER, CONF_TOPIC_AVAILABILITY, message_not_available, strlen(message_not_available), CONF_MQTT_USERNAME, CONF_MQTT_PASSWORD, connect_flags, 10);
 
                     if (res == MQTT_OK) {
                         mqtt_connection_result = 0;
                         mqtt_status = 2;
                         firstMessageSent = 0;
                         xTaskCreate(client_refresher, (char*)"client_ref", 1024,  &client, 10, &client_daemon);
                     } else {
                         mqtt_connection_result = -1;
                         mqtt_status = 1;
                     }
                     break;
 
                 case 2: // Send messages
                     message = bflb_gpio_read(gpio, GPIO_PIN_16);
                     if (!firstMessageSent || message != previousMessage) {
                         if (!firstMessageSent) {
                             mqtt_publish(&client, CONF_TOPIC_DISCOVERY, message_ha_discovery, strlen(message_ha_discovery), MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);
                             mqtt_publish(&client, CONF_TOPIC_AVAILABILITY, message_available, strlen(message_available), MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);
                         }
 
                         firstMessageSent = 1;
                         previousMessage = message;
 
                         char* application_message;
                         if (message) application_message = message_not_in_bed;
                         else application_message = message_in_bed;
 
                         mqtt_publish(&client, CONF_TOPIC_STATE, application_message, strlen(application_message), MQTT_PUBLISH_QOS_0 | MQTT_PUBLISH_RETAIN);
 
                         /* check for errors */
                         if (client.error != MQTT_OK) {
                             mqtt_connection_result = -1;
                             kill_mqtt();
                             mqtt_status = 0;
                         } else {
                             mqtt_connection_result = 0;
                         }
                     }
                     break;
             }
         } else {
             if (mqtt_status != 0) {
                 mqtt_connection_result = 0;
                 kill_mqtt();
                 mqtt_status = 0;
             }
         }
         vTaskDelay(100);
     }
 }
 
 void client_refresher(void* client) {
     while(1) {
         mqtt_sync((struct mqtt_client*) client);
         vTaskDelay(100);
     }
 
 }
 
 int main(void) {
     board_init();
 
     gpio = bflb_device_get_by_name("gpio");
 
     if (0 != rfparam_init(0, NULL, 0)) return -1;
 
     tcpip_init(NULL, NULL);
     wifi_start_firmware_task();
 
     xTaskCreate(wifiStatus, (char *)"wifiStatus", 1024, NULL, 5, &wifi_status_task);
     xTaskCreate(wifiConnection, (char *)"wifiConnection", 1024, NULL, 20, &wifi_connection_task);
     xTaskCreate(mqttStatus, (char *)"mqttStatus", 1024, NULL, 5, &mqtt_status_task);
     xTaskCreate(mqttConnection, (char *)"mqttConnection", 1024, NULL, 10, &mqtt_connection_task);
 
     vTaskStartScheduler();
 
     while (1) {
     }
 
     return -1;
 }