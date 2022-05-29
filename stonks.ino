#include <wifi.h> //Connect to WiFi Network
  #include <spi.h>
  #include <tft_espi.h>
  #include <mpu6050_esp32.h>
  #include<math.h>
  #include<string.h>
  
  TFT_eSPI tft = TFT_eSPI();
  const int SCREEN_HEIGHT = 160;
  const int SCREEN_WIDTH = 128;
  const int BUTTON_PIN = 5;
  const int LOOP_PERIOD = 40;
  
  const int YELLOW_LED = 32; // temporarily blue
  const int RED_LED = 13;
  const int GREEN_LED = 33;
  
  MPU6050 imu; //imu object called, appropriately, imu
  
  //char network[] = "EECS_Labs";  //SSID for 6.08 Lab
  char network[] = "MIT";
  char password[] = ""; //Password for 6.08 Lab
  
  //Some constants and some resources:
  const int RESPONSE_TIMEOUT = 6000; //ms to wait for response from host
  const uint16_t IN_BUFFER_SIZE = 3500;
  const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
  char request[IN_BUFFER_SIZE];
  char new_request[IN_BUFFER_SIZE];
  char old_response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request
  char response[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP request
  char old_response_buffer[OUT_BUFFER_SIZE];
  char response_buffer[OUT_BUFFER_SIZE];
  
  uint32_t primary_timer;
  uint32_t request_timer; // ensure we request stock data every 10 seconds
  
  char ticker[10];
  
  char* price_tok;  // stock price value and color change
  char* color_tok;
  
  int old_val;
  
  //used to get x,y values from IMU accelerometer!
  void get_angle(float* x, float* y) {
    imu.readAccelData(imu.accelCount);
    *x = imu.accelCount[0] * imu.aRes;
    *y = imu.accelCount[1] * imu.aRes;
  }
  
  class Button {
    public:
      uint32_t state_2_start_time;
      uint32_t button_change_time;
      uint32_t debounce_duration;
      uint32_t long_press_duration;
      uint8_t pin;
      uint8_t flag;
      bool button_pressed;
      uint8_t state; // This is public for the sake of convenience
      Button(int p) {
        flag = 0;
        state = 0;
        pin = p;
        state_2_start_time = millis(); //init
        button_change_time = millis(); //init
        debounce_duration = 10;
        long_press_duration = 1000;
        button_pressed = 0;
      }
      void read() {
        uint8_t button_state = digitalRead(pin);
        button_pressed = !button_state;
      }
      // update state of button and track short and long presses
      int update() {
        read();
        flag = 0;
        if (state == 0) {
          if (button_pressed) {
            state = 1;
            button_change_time = millis();
          }
        } else if (state == 1) {
          if (button_pressed and millis() - button_change_time >= debounce_duration) {
            state = 2;
            state_2_start_time = millis();
          }
          else if (!button_pressed) {
            state = 0;
            button_change_time = millis();
          }
        } else if (state == 2) {
          if (button_pressed and millis() - state_2_start_time >= long_press_duration) {
            state = 3;
          }
          else if (!button_pressed) {
            state = 4;
            button_change_time = millis();
          }
        } else if (state == 3) {
          if (!button_pressed) {
            state = 4;
            button_change_time = millis();
          }
        } else if (state == 4) {
          if (!button_pressed and millis() - button_change_time >= debounce_duration) {
            state = 0;
            if (millis() - state_2_start_time < long_press_duration) {
              flag = 1;
            }
            else {
              flag = 2;
            }
          }
          else if (button_pressed and millis() - state_2_start_time < long_press_duration) {
            state = 2;
            button_change_time = millis();
          }
          else if (button_pressed and millis() - state_2_start_time >= long_press_duration) {
            state = 3;
            button_change_time = millis();
          }
        }
        return flag;
      }
  };
  
  class StockGetter {
      char alphabet[50] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      char msg[400] = {0}; //contains previous query response
      char query_string[50] = {0};
      int char_index;
      int state;
      uint32_t scrolling_timer;
      const int scrolling_threshold = 150;
      const float angle_threshold = 0.3;
  
    public:
      StockGetter() {
        state = 0;
        memset(msg, 0, sizeof(msg));
        strcat(msg, "Welcome to Stonks!");
        char_index = 0;
        scrolling_timer = millis();
      }
      void update(float angle, int button, char* output) {
        //YOUR CODE FOR EX05 GOES HERE!
        int start_timer;
        if (state == 0) {   // welcome screen
          strcpy(output, msg);
          if (button == 2) {
            start_timer = millis();
            state = 1;
          }
        }
  
        else if (state == 1) {  // build query
  
          if (millis() - scrolling_timer >= scrolling_threshold and millis() - start_timer >= scrolling_threshold) {
            if (angle > 0 and abs(angle) >= angle_threshold) {
  
              strcpy(output, query_string);
              char_index ++;
              if (char_index > 36) {
                char_index = char_index - 37;
              }
              strncat(output, &alphabet[char_index], 1);
  
            }
            else if (angle < 0 and abs(angle) >= angle_threshold) {
              strcpy(output, query_string);
              char_index --;
              if (char_index < 0) {
                char_index = char_index + 37;
              }
              strncat(output, &alphabet[char_index], 1);
            }
  
            scrolling_timer = millis();
          }
  
          if (button == 1) {  // set query_string
            strncat(query_string, &alphabet[char_index], 1);
            strcpy(output, query_string);
            char_index = 0;
          }
  
          else if (button == 2) {
            strcpy(output, "");
            state = 2;
          }
  
        }
        else if (state == 2) {
          strcpy(output, "Sending Query");  // transition state
          state = 3;
        }
        else if (state == 3) {
          strcpy(ticker, query_string); // add query to ticker
          if (button == 2) {   // build new query
            for (int i = 0; i < sizeof(ticker); i++) {
              ticker[i] = '\0';
            }
            for (int i = 0; i < sizeof(query_string); i++) {
              query_string[i] = '\0';
            }
            strcpy(output, "New Query");
            start_timer = millis();
            state = 1;
          }
        }
      }
  };
  
  StockGetter sg; //stock object
  Button button(BUTTON_PIN); //button object!
  
  
  void setup() {
    Serial.begin(115200); //for debugging if needed.
    WiFi.begin(network, password); //attempt to connect to wifi
    uint8_t count = 0; //count used for Wifi check times
    Serial.print("Attempting to connect to ");
    Serial.println(network);
    while (WiFi.status() != WL_CONNECTED && count < 12) {
      delay(500);
      Serial.print(".");
      count++;
    }
    delay(2000);
    if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
      Serial.println("CONNECTED!");
      Serial.printf("%d:%d:%d:%d (%s) (%s)\n", WiFi.localIP()[3], WiFi.localIP()[2],
                    WiFi.localIP()[1], WiFi.localIP()[0],
                    WiFi.macAddress().c_str() , WiFi.SSID().c_str());    delay(500);
    } else { //if we failed to connect just Try again.
      Serial.println("Failed to Connect :/  Going to restart");
      Serial.println(WiFi.status());
      ESP.restart(); // restart the ESP (proper way)
    }
    if (imu.setupIMU(1)) {
      Serial.println("IMU Connected!");
    } else {
      Serial.println("IMU Not Connected :/");
      Serial.println("Restarting");
      ESP.restart(); // restart the ESP (proper way)
    }
    tft.init();
    tft.setRotation(2);
    tft.setTextSize(1.5);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK); //set color of font to green foreground, black background
    pinMode(BUTTON_PIN, INPUT_PULLUP);
  
    // set LED pins
    pinMode(YELLOW_LED, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
  
    primary_timer = millis();
    request_timer = millis();
  }
  
  void loop() {
    if (strlen(ticker) > 0 and millis() - request_timer >= 10000) { // Make POST request to server every 10 seconds
      char post_body[100];
      sprintf(post_body, "ticker=%s", ticker);
      new_request[0] = '\0';
      int offset = 0;
      offset += sprintf(new_request + offset, "POST http://608dev-2.net/sandbox/sc/sophiez/stonks.py HTTP/1.1\r\n");
      offset += sprintf(new_request + offset, "Host: 608dev-2.net\r\n");
      offset += sprintf(new_request + offset, "Content-Type: application/x-www-form-urlencoded\r\n");
      offset += sprintf(new_request + offset, "Content-Length: %d\r\n\r\n", strlen(post_body));
      offset += sprintf(new_request + offset, post_body);
      do_http_request("608dev-2.net", new_request, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, false);
      Serial.println(response_buffer);
      request_timer = millis();
    }
  
    float x, y;
    get_angle(&x, &y); //get angle values
    int bv = button.update(); //get button value
    sg.update(-y, bv, response); //input: angle and button, output String to display on this timestep
    if (strcmp(response, old_response) != 0) {//only draw if changed!
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0, 1);
      tft.println(response);
    }
  
    // update screen if ticker and/or price changes
    if (strcmp(response_buffer, old_response_buffer) != 0) {
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0, 1);
      char* token = strtok(response_buffer, "', ");
      int count = 1;
      while (token != NULL) {
        count++;
        if (count == 3) {
          color_tok = token;
        }
        if (count == 4) {
          price_tok = token;
        }
        token = strtok(NULL, "', ");
      }
      tft.printf("Ticker: %s\r\nPrice: %s", ticker, price_tok);
      // Show yellow if no change, red if price went down, green if price went up
      if (strcmp(color_tok, "Y") == 0) {
        digitalWrite(RED_LED, HIGH);
        digitalWrite(GREEN_LED, HIGH);
        digitalWrite(YELLOW_LED, LOW);
      } else if (strcmp(color_tok, "R") == 0) {
        digitalWrite(GREEN_LED, LOW);
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(RED_LED, HIGH);
      } else if (strcmp(color_tok, "G") == 0) {
        digitalWrite(YELLOW_LED, LOW);
        digitalWrite(RED_LED, LOW);
        digitalWrite(GREEN_LED, HIGH);
      }
    }
  
    memset(old_response_buffer, 0, sizeof(old_response_buffer));
    strcat(old_response_buffer, response_buffer);
    memset(old_response, 0, sizeof(old_response));
    strcat(old_response, response);
    while (millis() - primary_timer < LOOP_PERIOD); //wait for primary timer to increment
    primary_timer = millis();
  }
