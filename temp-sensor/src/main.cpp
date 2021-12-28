#include <Adafruit_MCP9808.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <math.h>

// state
float setTemp = 22;
float c, f;

// Create the MCP9808 temperature sensor object
Adafruit_MCP9808 tempsensor = Adafruit_MCP9808();

#define CS_PIN 17
#define IRQ_PIN 34
XPT2046_Touchscreen ts(CS_PIN, IRQ_PIN);
TS_Point p;

// setup screen and lvgl variables
#define ORIENTATION 1
#define MINPRESSURE 10
#define MAXPRESSURE 1000
#define MY_DISP_HOR_RES 320
#define MY_DISP_VER_RES 240
#define DEBOUNCE 200

uint16_t x = 0, y = 0;
TFT_eSPI tft = TFT_eSPI();

// declarations and setup for resistive touch panel
const int XP = 19, XM = 32, YP = 33, YM = 25; // 240x320 ID=0x9341
const int TS_LEFT = 140, TS_RT = 950, TS_TOP = 960, TS_BOT = 50;
// PORTRAIT  CALIBRATION     240 x 320
// x = map(p.x, LEFT=933, RT=187, 0, 240)
// y = map(p.y, TOP=116, BOT=940, 0, 320)

// LANDSCAPE CALIBRATION     320 x 240
// x = map(p.y, LEFT=116, RT=940, 0, 320)
// y = map(p.x, TOP=187, BOT=933, 0, 240)

// declarations for lvgl
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf[MY_DISP_HOR_RES * MY_DISP_VER_RES / 10];
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

// lvgl widgets
static lv_obj_t *tempLabel, *btnUpLabel, *btnDwnLabel;
static lv_obj_t *btnUp, *btnDwn;

void my_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area,
                 lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  uint32_t wh = w * h;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  while (wh--)
    tft.pushColor(color_p++->full);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  static lv_indev_data_t last = {{0, 0}, 0, 0, 0, LV_INDEV_STATE_REL};
  static unsigned long lastTouchTime = 0;

  if (millis() > lastTouchTime + DEBOUNCE && tft.getTouch(&x, &y)) {
    // Valid, change
    Serial.print("x: ");
    Serial.println(x);
    Serial.print("y: ");
    Serial.println(y);
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PR;
    lastTouchTime = millis();
    // ? use to check x and y vales after mapping
    /* Serial.println("x: "); */
    /* Serial.println(data->point.x); */
    /* Serial.println("y: "); */
    /* Serial.println(data->point.y); */
  } else {
    data->point = last.point;
    data->state = LV_INDEV_STATE_REL;
  }
  last.point = data->point;
  last.state = data->state;
}

static void eventHandlerBtnUp(lv_event_t *e) {
  if (e->code == LV_EVENT_CLICKED) {
    setTemp += 0.25;
  }
}

static void eventHandlerBtnDwn(lv_event_t *e) {
  if (e->code == LV_EVENT_CLICKED) {
    setTemp -= 0.25;
  }
}

#define TEMP_REQ_INTERVAL 10 * 1000
#define POST_COM_INTERVAL 60 * 1000
#define CONTROLLER_URL "http://192.168.4.117:80/turn"
const char *ssid = "thermostat";
const char *password = "thermostatpass";
unsigned long long readingLastTime, postLastTime;
int direction, duration, delta;
char httpRequestData[100];

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
}

void WiFiStaConnToAP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Station connected to AP");
  WiFi.softAPgetStationNum();
}

void setup(void) {
  // needed for proper touch sensor readings
  analogReadResolution(10);

  Serial.begin(115200);

  /* WiFi.mode(WIFI_STA); */
  /* WiFi.disconnect(); */
  /* WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED); */
  /* WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP); */
  /* WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED); */
  /* WiFi.begin(ssid, password); */

  WiFi.onEvent(WiFiStaConnToAP, SYSTEM_EVENT_AP_STACONNECTED);
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // initiate screen
  tft.init();
  tft.setRotation(ORIENTATION);

  /* ts.begin(); */
  /* ts.setRotation(ORIENTATION); */

  // initialize lvgl display
  lv_init();
  lv_disp_draw_buf_init(&disp_buf, buf, NULL,
                        MY_DISP_HOR_RES * MY_DISP_VER_RES / 10);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = MY_DISP_HOR_RES;
  disp_drv.ver_res = MY_DISP_VER_RES;
  disp_drv.draw_buf = &disp_buf;
  disp_drv.flush_cb = my_flush_cb;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // initialize widgets
  tempLabel = lv_label_create(lv_scr_act());
  lv_obj_align(tempLabel, LV_ALIGN_CENTER, -80, 0);
  lv_obj_set_style_text_font(tempLabel, &lv_font_montserrat_18, 0);

  btnUp = lv_btn_create(lv_scr_act());
  lv_obj_align(btnUp, LV_ALIGN_CENTER, 80, -60);
  lv_obj_set_width(btnUp, 120);
  lv_obj_set_height(btnUp, 80);
  lv_obj_add_event_cb(btnUp, eventHandlerBtnUp, LV_EVENT_ALL, NULL);

  btnUpLabel = lv_label_create(btnUp);
  lv_label_set_text(btnUpLabel, "Cieplej");
  lv_obj_center(btnUpLabel);
  lv_obj_set_style_bg_color(btnUp, lv_palette_main(LV_PALETTE_RED), 0);

  btnDwn = lv_btn_create(lv_scr_act());
  lv_obj_align(btnDwn, LV_ALIGN_CENTER, 80, 60);
  lv_obj_set_width(btnDwn, 120);
  lv_obj_set_height(btnDwn, 80);
  lv_obj_add_event_cb(btnDwn, eventHandlerBtnDwn, LV_EVENT_ALL, NULL);

  btnDwnLabel = lv_label_create(btnDwn);
  lv_label_set_text(btnDwnLabel, "Zimniej");
  lv_obj_center(btnDwnLabel);

  // initiate temp sensor
  while (!Serial)
    ; // waits for serial terminal to be open, necessary in newer arduino
      // boardst.
  Serial.println("MCP9808 demo");

  if (!tempsensor.begin(0x18)) {
    Serial.println("Couldn't find MCP9808! Check your connections and verify "
                   "the address is correct.");
    while (1)
      ;
  }

  Serial.println("Found MCP9808!");

  tempsensor.setResolution(3); // sets the resolution mode of reading, the modes
                               // are defined in the table bellow:
  // Mode Resolution SampleTime
  //  0    0.5°C       30 ms
  //  1    0.25°C      65 ms
  //  2    0.125°C     130 ms
  //  3    0.0625°C    250 ms
}

void loop() {
  lv_task_handler();
  delay(100);
  if ((millis() - readingLastTime) > TEMP_REQ_INTERVAL) {

    Serial.println("wake up MCP9808.... "); // wake up MCP9808 - power
                                            //    consumption
                                            // ~200 mikro Ampere
    tempsensor.wake();                      // wake up, ready to read!

    // Read and print out the temperature, also shows the resolution mode
    // used for
    // reading.
    Serial.print("Resolution in mode: ");
    Serial.println(tempsensor.getResolution());
    c = tempsensor.readTempC();
    f = tempsensor.readTempF();
    Serial.print("Temp: ");
    Serial.print(c, 4);
    Serial.print("*C\t and ");
    Serial.print(f, 4);
    Serial.println("*F.");

    lv_label_set_text_fmt(
        tempLabel, "Aktualna temp:\n%d.%02d\n\n\nZadana temp:\n%d.%02d\n",
        (int)c, (int)(c * 100) % 100, (int)setTemp, (int)(setTemp * 100) % 100);

    Serial.println("Shutdown MCP9808.... ");
    tempsensor.shutdown_wake(1); // shutdown MSP9808 - power consumption ~0.1
                                 // mikro Ampere, stops temperature sampling

    readingLastTime = millis();
  }
  // Send an HTTP request every 10 minutes
  if ((millis() - postLastTime) > POST_COM_INTERVAL) {
    // Check WiFi connection status
    /* if (WiFi.status() == WL_CONNECTED) { */
      Serial.println("Sending command to controller:");
      Serial.print("Direction: ");
      Serial.println(direction);
      Serial.print("Duration: ");
      Serial.println(duration);

      WiFiClient client;
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      http.begin(client, CONTROLLER_URL);

      // Specify content-type header
      http.addHeader("Content-Type", "application/json");
      // Data to send with HTTP POST
      direction = c > setTemp ? 0 : 1;
      delta = abs(setTemp - c);
      if (delta > 3) {
        duration = 20;
      } else if (delta > 2) {
        duration = 15;
      } else if (delta > 1) {
        duration = 10;
      } else {
        duration = 5;
      }
      sprintf(httpRequestData, "{\"direction\":\"%d\",\"duration\":\"%d\"}",
              direction, duration);
      // Send HTTP POST request
      int httpResponseCode = http.POST(httpRequestData);

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      // Free resources
      http.end();
    /* } else { */
      /* Serial.println("WiFi Disconnected"); */
    /* } */
    postLastTime = millis();
  }
}
