#define CONF_DEVICE_NAME "Bedmat"
#define CONF_DEVICE_ID "bedmat"
#define CONF_DEVICE_MANUFACTURER "Your name here <3"

#define CONF_WIFI_COUNTRY "DE"
#define CONF_WIFI_SSID "ssid"
#define CONF_WIFI_PASSWORD "password"

#define CONF_MQTT_ADDRESS "192.168.0.2"
#define CONF_MQTT_PORT "1883"
#define CONF_MQTT_USERNAME "username"
#define CONF_MQTT_PASSWORD "password"
#define CONF_MQTT_IDENTIFIER CONF_DEVICE_ID

#define CONF_TOPIC_STATE "bedroom/" CONF_DEVICE_ID "/status"
#define CONF_TOPIC_AVAILABILITY "bedroom/" CONF_DEVICE_ID "/available"
#define CONF_TOPIC_DISCOVERY "homeassistant/binary_sensor/bedroom/" CONF_DEVICE_ID "/config"