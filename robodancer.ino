#include <Adafruit_Arcada.h>
#include <PDM.h>
#include <Servo.h>
#include <CircularBuffer.h>

//Servo stuff
const int SERVO_PIN0 = 0;
const int SERVO_PIN1 = 1;
Servo myservo;  // create servo object to control a servo
Servo myservo2;  // create servo object to control a servo
int servo_pos = 0;    // variable to store the servo position
#define DELAY_PER_SERVO 15

// Color definitions
#define BACKGROUND_COLOR __builtin_bswap16(ARCADA_BLACK)
#define BORDER_COLOR __builtin_bswap16(ARCADA_BLUE)
#define PLOT_COLOR_1 __builtin_bswap16(ARCADA_YELLOW)
#define TITLE_COLOR __builtin_bswap16(ARCADA_WHITE)
#define TICKTEXT_COLOR __builtin_bswap16(ARCADA_WHITE)
#define TICKLINE_COLOR __builtin_bswap16(ARCADA_DARKGREY)

// Buffers surrounding the plot area
#define PLOT_TOPBUFFER 20
#define PLOT_LEFTBUFFER 40
#define PLOT_BOTTOMBUFFER 20
#define PLOT_W (ARCADA_TFT_WIDTH - PLOT_LEFTBUFFER)
#define PLOT_H (ARCADA_TFT_HEIGHT - PLOT_BOTTOMBUFFER - PLOT_TOPBUFFER)

// Which pin to plot
#define ANALOG_INPUT A2
// millisecond delay between samples (min 50 for CLUE)
#define DELAY_PER_SAMPLE 50

// Buffer for our plot data
CircularBuffer<float, PLOT_W> data_buffer;

extern PDMClass PDM;

Adafruit_Arcada arcada;

void setup() {
  myservo.attach(SERVO_PIN0);
  myservo2.attach(SERVO_PIN1);
  Serial.begin(115200);
  Serial.print("Hello! Arcada Plotter");

  // Start TFT and fill black
  if (!arcada.arcadaBegin()) {
    Serial.print("Failed to begin");
    while (1) delay(10);
  }
  arcada.displayBegin();
  
  // Turn on backlight
  arcada.setBacklight(255);

  if (! arcada.createFrameBuffer(ARCADA_TFT_WIDTH, ARCADA_TFT_HEIGHT)) {
    Serial.print("Failed to allocate framebuffer");
    while (1);
  }
  
  PDM.onReceive(onPDMdata);
  if (!PDM.begin(1, 16000)) {
    Serial.println("**Failed to start PDM!");
  }
  
}

uint32_t timestamp = 0;
uint32_t servo_time = 0;
void loop() {
  if (millis() - timestamp < DELAY_PER_SAMPLE) {
    return;
  }
 
  uint32_t pdm_vol = getPDMwave(256);
  timestamp = millis();
  int reading = pdm_vol;
  Serial.print("reading:" + String(reading));
  //servo_pos = map(reading, 0, 500, 0, 180); //crazy
  servoSweep(millis(), servo_time, 1, map(reading, 30, 300, 0, 10));
  data_buffer.push(reading);
  plotBuffer(arcada.getCanvas(), data_buffer, "Analog Input");
  arcada.blitFrameBuffer(0, 0, false, true);
  Serial.print("Drew in ");
  Serial.print(millis()-timestamp);
  Serial.println(" ms");
}

/**********************************************************************************/


// sweep servo back and forth without blocking
// example: 
// servoSweep(millis(), servo_time, 10, 1);
void servoSweep(int cur_time, uint32_t& servo_time, int delay_ms, int step_size)
{
  static int servo_fwd = true;
   if (cur_time - servo_time >= delay_ms) {
    if (servo_pos < 0) {
      servo_fwd = true;
      servo_pos = 0;
    }
    if (servo_pos > 180) {
      servo_fwd = false;
      servo_pos = 180;
    }
    if (servo_fwd) {
      servo_pos += step_size;
    }
    else {
      servo_pos -= step_size;
    }
    myservo.write(servo_pos);
    myservo2.write(servo_pos);
    servo_time = millis();
  }
}

void plotBuffer(GFXcanvas16 *_canvas, CircularBuffer<float, PLOT_W> &buffer, char *title) {
  _canvas->fillScreen(BACKGROUND_COLOR);
  _canvas->drawLine(PLOT_LEFTBUFFER-1, PLOT_TOPBUFFER, 
                    PLOT_LEFTBUFFER-1, PLOT_H+PLOT_TOPBUFFER, BORDER_COLOR);
  _canvas->drawLine(PLOT_LEFTBUFFER-1, PLOT_TOPBUFFER+PLOT_H+1, 
                    ARCADA_TFT_WIDTH, PLOT_TOPBUFFER+PLOT_H+1, BORDER_COLOR);
  _canvas->setTextSize(2);
  _canvas->setTextColor(TITLE_COLOR);
  uint16_t title_len = strlen(title) * 12;
  _canvas->setCursor((_canvas->width()-title_len)/2, 0);
  _canvas->print(title);
  
  float minY = 0;
  float maxY = 0;

  if (buffer.size() > 0) {
    maxY = minY = buffer[0];
  }
  for (int i=0; i< buffer.size(); i++) {
    minY = min(minY, buffer[i]);
    maxY = max(maxY, buffer[i]);
  }
  //Serial.printf("Data range: %f ~ %f\n", minY, maxY);

  float MIN_DELTA = 10.0;
  if (maxY - minY < MIN_DELTA) {
     float mid = (maxY + minY) / 2;
     maxY = mid + MIN_DELTA / 2;
     minY = mid - MIN_DELTA / 2;
  } else {
    float extra = (maxY - minY) / 10;
    maxY += extra;
    minY -= extra;
  }
  //Serial.printf("Y range: %f ~ %f\n", minY, maxY);

  printTicks(_canvas, 5, minY, maxY);

  int16_t last_y = 0, last_x = 0;
  for (int i=0; i<buffer.size(); i++) {
    int16_t y = map(buffer[i], minY, maxY, PLOT_TOPBUFFER+PLOT_H, PLOT_TOPBUFFER);

    //Serial.printf("%0.1f -> %d, ", cbuffer[i], y);
    int16_t x = PLOT_LEFTBUFFER+i;
    //y = TFT_H - y + PLOT_BOTTOMBUFFER - PLOT_TOPBUFFER;
    if (i == 0) {
      last_y = y;
      last_x = x;
    }
    _canvas->drawLine(last_x, last_y, x, y, PLOT_COLOR_1);
    last_x = x;
    last_y = y;
  }

}


void printTicks(GFXcanvas16 *_canvas, uint8_t ticks, float minY, float maxY) {
  _canvas->setTextSize(1);
  _canvas->setTextColor(TICKTEXT_COLOR);
  // Draw ticks
  for (int t=0; t<ticks; t++) {
    float v = map(t, 0, ticks-1, minY, maxY);
    uint16_t y = map(t, 0, ticks-1, ARCADA_TFT_HEIGHT - PLOT_BOTTOMBUFFER - 4, PLOT_TOPBUFFER);
    printLabel(_canvas, 0, y, v);
    uint16_t line_y = map(t, 0, ticks-1, ARCADA_TFT_HEIGHT - PLOT_BOTTOMBUFFER, PLOT_TOPBUFFER);
    _canvas->drawLine(PLOT_LEFTBUFFER, line_y, ARCADA_TFT_WIDTH, line_y, TICKLINE_COLOR);
  }
}

void printLabel(GFXcanvas16 *_canvas, uint16_t x, uint16_t y, float val) {
  char label[20];
  if (abs(val) < 1) {
    snprintf(label, 19, "%0.2f", val);
  } else if (abs(val) < 10) {
    snprintf(label, 19, "%0.1f", val);
  } else {
    snprintf(label, 19, "%d", (int)val);
  }
  
  _canvas->setCursor(PLOT_LEFTBUFFER-strlen(label)*6-5, y);
  _canvas->print(label);
}

/**********************************************************************************/

int16_t minwave, maxwave;
short sampleBuffer[256];// buffer to read samples into, each sample is 16-bits
volatile int samplesRead;// number of samples read

int32_t getPDMwave(int32_t samples) {
  minwave = 30000;
  maxwave = -30000;
  
  while (samples > 0) {
    if (!samplesRead) {
      yield();
      continue;
    }
    for (int i = 0; i < samplesRead; i++) {
      minwave = min(sampleBuffer[i], minwave);
      maxwave = max(sampleBuffer[i], maxwave);
      //Serial.println(sampleBuffer[i]);
      samples--;
    }
    // clear the read count
    samplesRead = 0;
  }
  return maxwave-minwave;  
}


void onPDMdata() {
  // query the number of bytes available
  int bytesAvailable = PDM.available();

  // read into the sample buffer
  PDM.read(sampleBuffer, bytesAvailable);

  // 16-bit, 2 bytes per sample
  samplesRead = bytesAvailable / 2;
}
