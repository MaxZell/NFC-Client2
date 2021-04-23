/** MQTT Publish von Sensordaten */
#include  "mbed.h"
#include "OLEDDisplay.h"
// #include "Motor.h"
// #include "Servo.h"
#include "MbedJSONValue.h"
#include "http_request.h"
#include "config.cpp"

#if MBED_CONF_IOTKIT_HTS221_SENSOR == true
#include "HTS221Sensor.h"
#endif
#if MBED_CONF_IOTKIT_BMP180_SENSOR == true
#include "BMP180Wrapper.h"
#endif
#include "MFRC522.h"

#ifdef TARGET_K64F
#include "QEI.h"

//Use X2 encoding by default.
QEI wheel (MBED_CONF_IOTKIT_BUTTON2, MBED_CONF_IOTKIT_BUTTON3, NC, 624);
#endif

#include <MQTTClientMbedOs.h>
#include <MQTTNetwork.h>
#include <MQTTClient.h>
#include <MQTTmbed.h> // Countdown

// Sensoren wo Daten fuer Topics produzieren
static DevI2C devI2c( MBED_CONF_IOTKIT_I2C_SDA, MBED_CONF_IOTKIT_I2C_SCL );
#if MBED_CONF_IOTKIT_HTS221_SENSOR == true
static HTS221Sensor hum_temp(&devI2c);
#endif
#if MBED_CONF_IOTKIT_BMP180_SENSOR == true
static BMP180Wrapper hum_temp( &devI2c );
#endif
AnalogIn hallSensor( MBED_CONF_IOTKIT_HALL_SENSOR );
DigitalIn button( MBED_CONF_IOTKIT_BUTTON1 );

// Topic's publish
char* topicTEMP = (char*) "8x3ebv3f4w3EUchR/sensor";
char* topicLOGIN = (char*) "8x3ebv3f4w3EUchR/login";
char* topicREGISTER = (char*) "8x3ebv3f4w3EUchR/register";
char* topicTEST = (char*) "8x3ebv3f4w3EUchR/test";
// Topic's subscribe
char* topicAll = (char*) "8x3ebv3f4w3EUchR/#";
char* topicANSWER = (char*) "8x3ebv3f4w3EUchR/answer";

// MQTT Brocker
char* hostname = (char*) BROKER_BASE_URL;
int port = 1883;
// MQTT Message
MQTT::Message message;
// I/O Buffer
char buf[100];
char uUID[2000];

// Klassifikation 
// char cls[3][10] = { "low", "middle", "high" };
int type = 0;

// UI
OLEDDisplay oled( MBED_CONF_IOTKIT_OLED_RST, MBED_CONF_IOTKIT_OLED_SDA, MBED_CONF_IOTKIT_OLED_SCL );
DigitalOut alert( MBED_CONF_IOTKIT_LED3 );

// Aktore(n)
PwmOut speaker( MBED_CONF_IOTKIT_BUZZER );
// Motor m1( MBED_CONF_IOTKIT_MOTOR2_PWM, MBED_CONF_IOTKIT_MOTOR2_FWD, MBED_CONF_IOTKIT_MOTOR2_REV ); // PWM, Vorwaerts, Rueckwarts
// NFC/RFID Reader (SPI)
MFRC522    rfidReader( MBED_CONF_IOTKIT_RFID_MOSI, MBED_CONF_IOTKIT_RFID_MISO, MBED_CONF_IOTKIT_RFID_SCLK, MBED_CONF_IOTKIT_RFID_SS, MBED_CONF_IOTKIT_RFID_RST ); 

/** Hilfsfunktion zum Publizieren auf MQTT Broker */
void publish( MQTTNetwork &mqttNetwork, MQTT::Client<MQTTNetwork, Countdown> &client, char* topic ){
    MQTT::Message message;    
    oled.cursor( 2, 0 );
    oled.printf( "Topi: %s\n", topic );
    oled.cursor( 3, 0 );    
    oled.printf( "Push: %s\n", buf );
    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*) buf;
    message.payloadlen = strlen(buf)+1;
    client.publish( topic, message);
}

/** Daten empfangen von MQTT Broker */
void messageArrived( MQTT::MessageData& md ) {
// Connect to the network with the default networking interface
    // if you use WiFi: see mbed_app.json for the credentials
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    if ( !wifi ) 
    {
        printf("ERROR: No WiFiInterface found.\n");
        oled.printf( "ERROR: No WiFiInterface found.\r\n" );
    }
    printf("\nConnecting to %s...\n", WIFI_SSID);
    int ret = wifi->connect( WIFI_SSID, WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2 );
    if ( ret != 0 ) 
    {
        printf("\nConnection error: %d\n", ret);
        oled.printf( "Connection error: %d\r\n" );
    }    

    // TCP/IP und MQTT initialisieren (muss in main erfolgen)
    MQTTNetwork mqttNetwork( wifi );
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc);


    oled.clear();

    float value;
    MQTT::Message &message = md.message;
    oled.clear();
    // oled.printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\n", message.qos, message.retained, message.dup, message.id);
    // oled.printf("Topic %.*s, ", md.topicName.lenstring.len, (char*) md.topicName.lenstring.data );
    oled.printf("Payload %.*s\n", message.payloadlen, (char*) message.payload);
    
    // thread_sleep_for( 30000 );

    if((char*) message.payload == "exist"){
        oled.clear();
        oled.printf("exist");

        // Temperator
        uint8_t id;
        float temp, hum;
        hum_temp.read_id(&id);  
        hum_temp.get_temperature(&temp);    
        if  ( type == 0 ){
            temp -= 5.0f;
        } else if  ( type == 2 ){
            temp += 5.0f;
        }
        // sprintf( buf, "'uid':'%s','temp':'%2.2f'", buf, temp); 
        sprintf( buf, "%2.2f", temp);
        type++;
        if  ( type > 2 )
            type = 0;       
        publish( mqttNetwork, client, topicTEMP );
        //send to db
        

    } else if ((char*) message.payload == "notexist") {
        oled.clear();
        oled.printf("not exist");
        oled.clear();
        // Magnet Sensor
        oled.printf("Attach Magnet\n");
        bool magnet_away = true;
        while(magnet_away) {
            // oled.clear();
            float value = hallSensor.read();
            // oled.cursor( 1, 0 );
            // oled.printf( "value %1.4f\r\n", value );
            // printf( "Hall Sensor %f\r\n", value );
            if ( value > 0.6f ){
                oled.clear();
                oled.printf("Magnet erkannt!");
                publish( mqttNetwork, client, topicREGISTER );

                magnet_away = false;
            }
            //Loop wartet hier eine Sekunde lang
            thread_sleep_for( 1000 );
        }
    }         
}

/** Hauptprogramm */
int main()
{
    uint8_t id;
    float temp, hum;
    int encoder;
    alert = 0;
    // servo2 = 0.5f;
    
    oled.clear();
    oled.printf( "MQTTPublish\r\n" );
    oled.printf( "host: %s:%s\r\n", hostname, port );
    
    oled.clear();
    printf("\nConnecting to %s...\n", WIFI_SSID);
    oled.printf( "SSID: %s\r\n", WIFI_SSID );
    oled.clear();
    
    // Connect to the network with the default networking interface
    // if you use WiFi: see mbed_app.json for the credentials
    WiFiInterface *wifi = WiFiInterface::get_default_instance();
    if ( !wifi ) 
    {
        printf("ERROR: No WiFiInterface found.\n");
        oled.printf( "ERROR: No WiFiInterface found.\r\n" );
        return -1;
    }
    printf("\nConnecting to %s...\n", WIFI_SSID);
    int ret = wifi->connect( WIFI_SSID, WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2 );
    if ( ret != 0 ) 
    {
        printf("\nConnection error: %d\n", ret);
        oled.printf( "Connection error: %d\r\n" );
        return -1;
    }    

    // TCP/IP und MQTT initialisieren (muss in main erfolgen)
    MQTTNetwork mqttNetwork( wifi );
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    printf("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        printf("rc from TCP connect is %d\r\n", rc); 

    // Zugangsdaten - der Mosquitto Broker ignoriert diese
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = (char*) wifi->get_mac_address(); // muss Eindeutig sein, ansonsten ist nur 1ne Connection moeglich
    data.username.cstring = (char*) wifi->get_mac_address(); // User und Password ohne Funktion
    data.password.cstring = (char*) "password";
    if ((rc = client.connect(data)) != 0)
        printf("rc from MQTT connect is %d\r\n", rc);           

    // MQTT Subscribe!
    client.subscribe( topicANSWER, MQTT::QOS0, messageArrived );
    printf("MQTT subscribe %s\n", topicAll );
    
    /* Init all sensors with default params */
    hum_temp.init(NULL);
    hum_temp.enable(); 
    // RFID Reader initialisieren
    rfidReader.PCD_Init();

    oled.clear();
    oled.printf("Attach NFC-Card\n");
    while   ( 1 ){
        // RFID Reader
        if ( rfidReader.PICC_IsNewCardPresent())
            if ( rfidReader.PICC_ReadCardSerial()) {
                // Print Card UID (2-stellig mit Vornullen, Hexadecimal)
                printf("Card UID: ");
                for ( int i = 0; i < rfidReader.uid.size; i++ )
                    printf("%02X:", rfidReader.uid.uidByte[i]);
                printf("\n");
                
                // Print Card type
                int piccType = rfidReader.PICC_GetType(rfidReader.uid.sak);
                printf("PICC Type: %s \n", rfidReader.PICC_GetTypeName(piccType) );
                
                //save uid
                sprintf( buf, "%02X:%02X:%02X:%02X", rfidReader.uid.uidByte[0], rfidReader.uid.uidByte[1], rfidReader.uid.uidByte[2], rfidReader.uid.uidByte[3] );
                publish( mqttNetwork, client, topicLOGIN );
                // sprintf( uUID, "%02X:%02X:%02X:%02X", rfidReader.uid.uidByte[0], rfidReader.uid.uidByte[1], rfidReader.uid.uidByte[2], rfidReader.uid.uidByte[3] );
                // HttpRequest* post_req = new HttpRequest(wifi, HTTP_POST, API_BASE_URL);
                //     // HttpResponse* get_res = get_req->send();
                //     char body[] = "";
                //     // sprintf( body, "%s", "{\"uid\":\"12345\"}\n",);
                //     sprintf( body, "%s%s%s", "{\"uid\":\"", uUID, "\"}\n");
                //     printf("BODY: %s\n", body);
                //     post_req->set_header("Content-Type", "application/json");
                //     HttpResponse* post_res = post_req->send(body, strlen(body));
                    
                //     // send request
                //     oled.printf("sending request...\n");

                //     //success response
                //     if (post_res){
                //         oled.printf("Login sended\n");
                //     }
                //     // Error
                //     else{
                //         printf("HttpRequest failed (error code %d)\n", post_req->get_error());
                //         oled.printf("HttpRequest failed (error code %d)\n", post_req->get_error());
                //         return 1;
                //     }
                //     delete post_req;
                oled.clear();
                // Temperator
                uint8_t id;
                float temp, hum;
                hum_temp.read_id(&id);  
                hum_temp.get_temperature(&temp);    
                if  ( type == 0 ){
                    temp -= 5.0f;
                } else if  ( type == 2 ){
                    temp += 5.0f;
                }
                oled.printf( "Temp: %2.2f \n", temp);
                oled.printf("Login sended\n");
            }         

#ifdef TARGET_K64F
        // Encoder
        encoder = wheel.getPulses();
        sprintf( buf, "%d", encoder );
        publish( mqttNetwork, client, topicENCODER );
#endif
        client.yield    ( 1000 );                   // MQTT Client darf empfangen
        thread_sleep_for( 500 );
    }
    // Verbindung beenden
    if ((rc = client.disconnect()) != 0)
        printf("rc from disconnect was %d\r\n", rc);

    mqttNetwork.disconnect();    
}
