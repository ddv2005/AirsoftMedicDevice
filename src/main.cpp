#include "config.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <driver/rtc_io.h>
#include <Button2.h>
#include "buzzer.h"
#include <EEPROM.h>
#include "AudioOutputI2S.h"
#include "audioPlayer.h"
#include <driver/dac.h>
#ifdef USE_I2C
#include <Wire.h>
#endif
#ifdef USE_ST25DV
#include "libs/ST25DV/ST25DVSensor.h"
#endif

#include <menu.h>
#include <menuIO/serialIO.h>
#include <menuIO/TFT_eSPIOut.h>
#include <menuIO/esp8266Out.h> //must include this even if not doing web output..
#include "led_controller.h"


struct sConfig
{
  int32_t medicTime;
  int32_t idleTime;
  int32_t holdTime;
  int32_t setupTimeout;
  int32_t askSetupPin;
  int32_t askStartupPin;
  uint16_t setupPin;
  uint16_t startupPin;
  int32_t maxRevivalsCount;
  int32_t sfxDelay;
  int32_t medicDownDelay;
  int32_t medicDownTime;
  sConfig()
  {
    medicTime = 10000;
    idleTime = 4000;
    holdTime = 2000;
    setupTimeout = 30000;
    askSetupPin = 0;
    askStartupPin = 0;
    startupPin = 0;
    setupPin = 000;
    maxRevivalsCount = 0;
    sfxDelay = 1500;
    medicDownDelay = 5000;
    medicDownTime = 60000;
  }
};


int8_t appState = AS_IDLE;
sConfig appConfig;

esp_sleep_wakeup_cause_t wakeup_reason;
TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);
Button2 btn1(BTN1_PIN);
Button2 btn2(BTN2_PIN);

static DRAM_ATTR uint8_t  sbtnLedScript_ID01[] = { LCSC_ON,  0, 0, 0, LCSC_FADE,  0, 0, 0, LCS_TIME_MS(0)};
static DRAM_ATTR uint8_t  sbtnLedScript_ID02[] = { LCSC_ON,  0, 0, 0, LCSC_FADE,  0, 0, 0, LCS_TIME_MS(1000),LCSC_GOTO, 4};
#define LED_SCRIPT(N,ID) { sizeof N, ID, N }
static ledScript_t  sbtnLedScriptsArray[] = {LED_SCRIPT(sbtnLedScript_ID01,1),LED_SCRIPT(sbtnLedScript_ID02,2)};
static ledScripts_t  sbtnLedScripts = { (sizeof sbtnLedScriptsArray)/sizeof(ledScript_t) , sbtnLedScriptsArray };


ledController_c secondBtnLED(SECOND_BTN_R_PIN,SECOND_BTN_G_PIN,SECOND_BTN_B_PIN,0,&sbtnLedScripts);

void sbtnOff()
{
  secondBtnLED.setOFF();
}

void sbtnOn(uint8_t R, uint8_t G, uint8_t B)
{
  secondBtnLED.setColor(LCS_COLOR(R, G, B));
  secondBtnLED.setON();
}

void sbtnFade(uint8_t R1, uint8_t G1, uint8_t BL1, uint8_t R2, uint8_t G2, uint8_t BL2, uint16_t time)
{
  uint8_t  script[] = { LCSC_ON,  R1, G1, BL1, LCSC_FADE,  R2, G2, BL2, LCS_TIME_MS(time)};
  memcpy(&sbtnLedScript_ID01 ,&script, sizeof(script));
  secondBtnLED.runScript(1);
}

void sbtnPulse(uint8_t R, uint8_t G, uint8_t B)
{
  uint8_t  script[] = { LCSC_ON,  R, G, B, LCSC_FADE,  0, 0, 0, LCS_TIME_MS(1000),LCSC_GOTO, 0};
  memcpy(&sbtnLedScript_ID02 ,&script, sizeof(script));
  secondBtnLED.runScript(2);
}


#ifdef USE_I2S
AudioOutputI2S *audioOut;
cAudioPlayer *aPlayer;
#else
cBuzzer buzzer(BUZZER_PIN);
#endif

RTC_DATA_ATTR bool firstStart = true;
RTC_DATA_ATTR int32_t currentRevivalsCount = 0;

void scanI2Cdevice(TwoWire &wd)
{
        byte err, addr;
        int nDevices = 0;
        for (addr = 1; addr < 127; addr++)
        {
            wd.beginTransmission(addr);
            err = wd.endTransmission();
            if (err == 0)
            {
                DEBUG_MSG("I2C device found at address 0x%x\n", addr);

                nDevices++;
            }
            else if (err == 4)
            {
                DEBUG_MSG("Unknow error at address 0x%x\n", addr);
            }
        }
        if (nDevices == 0)
            DEBUG_MSG("No I2C devices found\n");
        else
            DEBUG_MSG("done\n");
}


void print_wakeup_reason()
{
  switch (wakeup_reason)
  {
  case ESP_SLEEP_WAKEUP_EXT0:
    DEBUG_MSG("Wakeup caused by external signal using RTC_IO\n");
    break;
  case ESP_SLEEP_WAKEUP_EXT1:
    DEBUG_MSG("Wakeup caused by external signal using RTC_CNTL\n");
    break;
  case ESP_SLEEP_WAKEUP_TIMER:
    DEBUG_MSG("Wakeup caused by timer\n");
    break;
  case ESP_SLEEP_WAKEUP_TOUCHPAD:
    DEBUG_MSG("Wakeup caused by touchpad\n");
    break;
  case ESP_SLEEP_WAKEUP_ULP:
    DEBUG_MSG("Wakeup caused by ULP program\n");
    break;
  case ESP_SLEEP_WAKEUP_GPIO:
    DEBUG_MSG("Wakeup caused by GPIO\n");
    break;
  default:
    DEBUG_MSG("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
    break;
  }
}

#ifdef USE_I2S
bool playerIsIdle()
{
  return aPlayer->isIdle();
}

void playerEnable()
{
  dac_output_voltage(DAC_CHANNEL_1, 0.25 * 255 / 3.3);
}

void forcePlayerStop()
{
  dac_output_voltage(DAC_CHANNEL_1, 0);
}

void playStop()
{
  aPlayer->playStop();
}

void playOK()
{
  aPlayer->playFile2("/beep_ok.mp3", "/hc.mp3");
}

void playOKTone()
{
  aPlayer->playFile("/beep_ok.mp3");
}

void playBeepShort()
{
  aPlayer->playFile("/beep_short.mp3");
}

void playErrorTone()
{
  aPlayer->playFile("/beep_err.mp3");
}

void playError2Tone()
{
  aPlayer->playFile("/beep_err2.mp3");
}

void playMedicDown()
{
  aPlayer->playFile("/medicdown.mp3");
}

void playMedicAlive()
{
  aPlayer->playFile2("/beep_ok.mp3","/medicalive.mp3");
}

void playActivatingMedicDown()
{
  aPlayer->playFile("/amedicdown.mp3");
}

void playErrorMessage()
{
  aPlayer->playFile("/error.mp3");
}

void playStartTone()
{
  aPlayer->playFile("/beep.mp3");
}

void playStartMessage()
{
  aPlayer->playFile2("/beep.mp3", "/start.mp3");
}

#else
void playTone(int16_t freq, int16_t count, int16_t duration, int16_t idle)
{
  sBuzzerEvent be;
  be.action = BE_PLAY;
  be.param1 = freq;
  be.param2 = count;
  be.param3 = duration;
  be.param4 = idle;
  buzzer.sendEvent(be);
}

void playStop()
{
  sBuzzerEvent be;
  be.action = BE_STOP;
  buzzer.sendEvent(be);
}

void playOKTone()
{
  playTone(2000, 3, 600, 200);
}

void playErrorTone()
{
  playTone(1950, 30, 50, 50);
}

void playStartTone()
{
  playTone(2050, 1, 50, 50);
}
#endif

void sayTime(uint16_t t)
{
#ifdef USE_I2S
  aPlayer->sayNumber(t, NULL);
#endif
}

class amdStorage
{
protected:
  uint16_t m_baseAddress;
  bool m_hasST25DV;  
public:  
  amdStorage():m_hasST25DV(false)
  {
  }

  bool isReliableStorage()
  {
    return m_hasST25DV;
  }

  void begin()
  {
#ifdef USE_I2C
    if(firstStart)
    {
      Wire.begin(I2C_SDA,I2C_SCL);
#ifdef USE_ST25DV
      auto ret = st25dv.begin(ST25DV_GPIO,-1,&Wire);
      m_hasST25DV = (ret==NDEF_OK);
      if(m_hasST25DV)
      {
        DEBUG_MSG("ST25DV found\n");
        m_baseAddress = 128;
      }
#endif
    }
#endif
    if(!m_hasST25DV)
    {
      m_baseAddress = 0;
      EEPROM.begin(64);
    }
  }

  int32_t readInt(uint16_t addr)
  {
    if(m_hasST25DV)
    {
      int32_t result;
      st25dv.readData((uint8_t*)&result,m_baseAddress+addr,sizeof(int32_t));
      return result;
    }
    else
      return EEPROM.readInt(m_baseAddress+addr);
  }

  void write(void *data,uint16_t addr, uint16_t size)
  {
    if(m_hasST25DV)
    {
      st25dv.writeData((uint8_t*)data,m_baseAddress+addr,size);
    }
    else
      EEPROM.writeBytes(m_baseAddress+addr,data,size);
  }

  void writeInt(uint16_t addr,int32_t data)
  {
    write((uint8_t*)&data,addr,sizeof(int32_t));
  }  

  void commit()
  {
    if(!m_hasST25DV)
      EEPROM.commit();
  }
};

amdStorage storage;

void saveSettings()
{
  storage.writeInt(IDLE_TIME_OFFSET,appConfig.idleTime);
  storage.writeInt(MEDIC_TIME_OFFSET,appConfig.medicTime);
  storage.writeInt(REVIVALS_COUNT_OFFSET,appConfig.maxRevivalsCount);
  uint32_t flags = 0;
  if(appConfig.askStartupPin)
    flags |= 1;
  if(appConfig.askSetupPin)
    flags |= 2;    
  storage.writeInt(FLAGS_OFFSET,flags);
  storage.writeInt(STARTUP_PIN_OFFSET,appConfig.startupPin);
  storage.writeInt(SETUP_PIN_OFFSET,appConfig.setupPin);
  storage.writeInt(MEDIC_DOWN_DELAY_OFFSET,appConfig.medicDownDelay);
  storage.writeInt(MEDIC_DOWN_TIME_OFFSET,appConfig.medicDownTime);
  storage.commit();
}

void loadSettings()
{
  bool settingsChanged = false;
  //  appConfig.idleTime = storage.readInt(IDLE_TIME_OFFSET);
  appConfig.medicTime = storage.readInt(MEDIC_TIME_OFFSET);
  appConfig.maxRevivalsCount = storage.readInt(REVIVALS_COUNT_OFFSET);
  uint32_t flags = storage.readInt(FLAGS_OFFSET);
  appConfig.askStartupPin = (flags & 1)?1:0;
  appConfig.askSetupPin = (flags & 2)?1:0;
  appConfig.startupPin = storage.readInt(STARTUP_PIN_OFFSET);
  appConfig.setupPin = storage.readInt(SETUP_PIN_OFFSET);
  appConfig.sfxDelay = storage.readInt(SFX_DELAY_OFFSET);
  appConfig.medicDownDelay = storage.readInt(MEDIC_DOWN_DELAY_OFFSET);
  appConfig.medicDownTime = storage.readInt(MEDIC_DOWN_TIME_OFFSET);

  if(flags>3)
  {
    flags = 0;
    settingsChanged = true;
  }

  if(appConfig.startupPin>999)
  {
    settingsChanged = true;
    appConfig.startupPin = 0;
  }
  if(appConfig.setupPin>999)
  {
    settingsChanged = true;
    appConfig.setupPin = 0;
  }

  if (appConfig.idleTime < 1000)
  {
    settingsChanged = true;
    appConfig.idleTime = 4000;
  }

  if (appConfig.medicTime < 10000)
  {
    settingsChanged = true;
    appConfig.medicTime = 10000;
  }

  if (appConfig.maxRevivalsCount<0)
  {
    settingsChanged = true;
    appConfig.maxRevivalsCount = 0;
  }
  else
  if(appConfig.maxRevivalsCount>1000)
  {
    settingsChanged = true;
    appConfig.maxRevivalsCount = 1000;
  }

  if(appConfig.sfxDelay<0)
  {
    settingsChanged = true;
    appConfig.sfxDelay = 1500;
  }
  else
  if(appConfig.sfxDelay>10000)
  {
    settingsChanged = true;
    appConfig.sfxDelay = 10000;
  }

  if(appConfig.medicDownDelay<5000)
  {
    settingsChanged = true;
    appConfig.medicDownDelay = 5000;
  }
  else
  if(appConfig.medicDownDelay>10000)
  {
    settingsChanged = true;
    appConfig.medicDownDelay = 10000;
  }  

  if(appConfig.medicDownTime<10000)
  {
    settingsChanged = true;
    appConfig.medicDownTime = 60000;
  }
  else
  if(appConfig.medicDownTime>600000)
  {
    settingsChanged = true;
    appConfig.medicDownTime = 60000;
  }  

  if(settingsChanged)
    saveSettings();

  if(storage.isReliableStorage())
  {
    currentRevivalsCount = storage.readInt(CURRENT_REVIVALS_OFFSET);
  }
}

void setup()
{
#ifdef DEBUG_PORT
  DEBUG_PORT.begin(115200);
#endif  
  storage.begin();
  loadSettings();

#ifdef USE_I2S
  SPIFFS.begin();
  audioOut = new AudioOutputI2S();
  audioOut->SetOutputModeMono(true);
  audioOut->SetPinout(I2S_BCLK, I2S_WCLK, I2S_DOUT);
  audioOut->SetGain(1);
  dac_output_enable(DAC_CHANNEL_1);
  playerEnable();

  aPlayer = new cAudioPlayer(audioOut);
  aPlayer->begin();
#else
  buzzer.begin();
#endif

  wakeup_reason = esp_sleep_get_wakeup_cause();
  rtc_gpio_pullup_en(MAIN_BTN_PIN);
  pinMode(MAIN_BTN_PIN, INPUT_PULLUP);

  rtc_gpio_pullup_en(SECOND_BTN_PIN);
  pinMode(SECOND_BTN_PIN, INPUT_PULLUP);
  pinMode(SECOND_BTN_R_PIN, OUTPUT);
  pinMode(SECOND_BTN_G_PIN, OUTPUT);
  pinMode(SECOND_BTN_B_PIN, OUTPUT);

  digitalWrite(SECOND_BTN_R_PIN,LOW);
  digitalWrite(SECOND_BTN_G_PIN,LOW);
  digitalWrite(SECOND_BTN_B_PIN,LOW);

  rtc_gpio_pullup_en(BTN1_PIN);
  pinMode(BTN1_PIN, INPUT_PULLUP);

  rtc_gpio_pullup_en(BTN2_PIN);
  pinMode(BTN2_PIN, INPUT_PULLUP);

#ifdef DEBUG_PORT
  print_wakeup_reason();
#endif

  tft.init();
  tft.setRotation(1);
}

void deepSleep()
{
  tft.fillScreen(TFT_BLACK);

  gpio_wakeup_enable(MAIN_BTN_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(SECOND_BTN_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(ST25DV_GPIO, GPIO_INTR_LOW_LEVEL);

  sbtnOff();
  digitalWrite(SECOND_BTN_R_PIN,LOW);
  digitalWrite(SECOND_BTN_G_PIN,LOW);
  digitalWrite(SECOND_BTN_B_PIN,LOW);

  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_ext1_wakeup(0x800000001, ESP_EXT1_WAKEUP_ALL_LOW);
#ifdef USE_I2S
  forcePlayerStop();
#else
  buzzer.forceStopTone();
#endif
  //esp_deep_sleep_start();
  digitalWrite(TFT_BL,LOW);
  esp_light_sleep_start();
#ifdef DEBUG_PORT
  wakeup_reason = esp_sleep_get_wakeup_cause();
  print_wakeup_reason();
#endif

  loadSettings();
  digitalWrite(TFT_BL,HIGH);
  playerEnable();
}

void drawBattery()
{
  uint16_t v = analogRead(ADC_PIN);
  float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);
  int16_t batteryChargePercent = clamp((int)(((battery_voltage * 1000 - BAT_MILLIVOLTS_EMPTY) * 1e2) / (BAT_MILLIVOLTS_FULL - BAT_MILLIVOLTS_EMPTY)), 0, 100);
  String voltage = " "+String(batteryChargePercent) + "% ( " + String(battery_voltage) + "V )";
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawRightString(voltage, tft.width(), 0, 2);
  String version = "v"+String(SOFTWARE_VERSION_MAJOR)+"."+String(SOFTWARE_VERSION_MINOR)+"."+String(SOFTWARE_VERSION_PATCH);
  tft.drawRightString(version, tft.width(), tft.height()-16, 2);
}


#define CMD_UP    1
#define CMD_DOWN  2
#define CMD_ENTER 3
struct sKey
{
  uint8_t key;
};

std::queue<sKey> m_inputQueue;
int32_t enterPin(const char *title,int digits,uint32_t timeout)
{
  int32_t result = 0;
  int64_t actionTime = millis();
  int8_t position = 0;
  int8_t currentNumber = 0;

  btn2.setClickHandler([](Button2 &b) {
    sKey key;
    key.key = CMD_UP;
    m_inputQueue.push(key);
  });

  btn2.setLongClickHandler([](Button2 &b) {
    sKey key;
    key.key = CMD_ENTER;
    m_inputQueue.push(key);
  });

  btn1.setClickHandler([](Button2 &b) {
    sKey key;
    key.key = CMD_DOWN;
    m_inputQueue.push(key);
  });

  btn1.setLongClickHandler([](Button2 &b) {
    sKey key;
    key.key = CMD_ENTER;
    m_inputQueue.push(key);
  });

  tft.fillScreen(TFT_BLUE);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawCentreString(title, tft.width() / 2, 20, 4);
  auto drawScreen = [&]()
  {
    int i = 0;
    char buffer[digits+1];
    buffer[digits] = 0;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    for(i=0; i<position; i++)
      buffer[i] = '*';
    buffer[i] = 0x30+currentNumber;
    for(i++; i<digits; i++)
      buffer[i] = '-';
    tft.setFreeFont(&FreeMono24pt7b);
    tft.setTextSize(1);    
    tft.drawCentreString(buffer, tft.width() / 2, 55,1);
    tft.setTextFont(1);
    drawBattery();
  };

  drawScreen();
  while(true)
  {
    btn1.loop();
    btn2.loop();
    while(m_inputQueue.size()>0)
    {
      actionTime = millis();
      sKey key = m_inputQueue.front();
      m_inputQueue.pop();
      switch (key.key)
      {
        case CMD_UP:
          currentNumber++;
          if(currentNumber>9)
            currentNumber = 0;
          break;

        case CMD_DOWN:
          currentNumber--;
          if(currentNumber<0)
            currentNumber = 9;        
          break;

        case CMD_ENTER:
        {
          result = result*10+currentNumber;
          position++;
          currentNumber = 0;
          break;
        }
      }
      if(position>=digits)
        return result;
      else
        drawScreen();
    }
    if((millis()-actionTime)>timeout)
      return -1;
    delay(10);
  }
  return result;
}

class cButton
{
  protected:
    int64_t m_start;
    uint8_t m_pin;

  public:
    cButton(uint8_t pin):m_start(0),m_pin(pin)
    {}

    bool getState(bool useDebounce = false)
    {
      bool raw = digitalRead(m_pin) == LOW;
      if (raw)
      {
        m_start = millis();
        return true;
      }
      else
      {
        if ((m_start == 0) || !useDebounce)
          return false;
        else
        {
          if ((millis() - m_start) >= 50)
          {
            m_start = 0;
            return false;
          }
          else
            return true;
        }
      }
    }
};

cButton mainBtn(MAIN_BTN_PIN);
cButton secondBtn(SECOND_BTN_PIN);

void medicState()
{
  char displayString[48];
  int32_t leftRevivalsCount = 0;
  int64_t startTime = millis();
  int64_t lastSayTime = millis();
  int64_t pauseTime = 0;
  bool finished = false;
  bool tonePlay = false;
  loadSettings();
  playStop();

  if(storage.isReliableStorage())
  {
    currentRevivalsCount = storage.readInt(CURRENT_REVIVALS_OFFSET);
  }

  auto canPlaySFX=[&]() -> bool
  {
    return (pauseTime - startTime)>=appConfig.sfxDelay;
  };

  auto drawRevivals=[&leftRevivalsCount,&displayString](uint16_t textcolor=TFT_WHITE, uint16_t color = TFT_RED)
  {
    if(appConfig.maxRevivalsCount>0)
    {
      tft.setTextSize(1);
      tft.setTextColor(textcolor, color);

      sprintf(displayString, "%d of %d (%d left)", currentRevivalsCount+1,appConfig.maxRevivalsCount,leftRevivalsCount);
      tft.drawCentreString(displayString, tft.width() / 2, 20, 4);
    }
  };

  tft.fillScreen(TFT_RED);
  sbtnOn(0xFF,0,0);
  if(appConfig.maxRevivalsCount>0)
  {
    leftRevivalsCount = appConfig.maxRevivalsCount-currentRevivalsCount;
    if(leftRevivalsCount<=0)
    {
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE, TFT_RED);

      tft.drawCentreString("No more revivals", tft.width() / 2, 20, 4);
      tft.drawCentreString("Please reset", tft.width() / 2, 50, 4);
      playErrorTone();
      delay(appConfig.idleTime);
      appState = AS_IDLE;
      return;
    }
    else
    {
      drawRevivals();
    }
  }
  sbtnPulse(0xFF,00,00);

  while (true)
  {
    secondBtnLED.loop();
    if (mainBtn.getState(!finished))
    {
      int64_t now = millis();
      if (pauseTime > 0)
      {
        startTime += (now - pauseTime);
        pauseTime = 0;
        tft.fillScreen(TFT_RED);
        sbtnPulse(0xFF,00,00);
        if(appConfig.maxRevivalsCount>0)
          drawRevivals();
        playStartTone();
      }
      if (finished || ((now - startTime) >= appConfig.medicTime))
      {
        if (!finished)
        {
          tft.fillScreen(TFT_GREEN);
          sbtnOn(0,0xFF,0);
          tft.setTextSize(1);
          tft.setTextColor(TFT_WHITE, TFT_BLACK);
          float time = appConfig.medicTime / 1000.0;
          sprintf(displayString, "%02.1f", time);
          tft.drawCentreString(displayString, tft.width() / 2, 50, 7);
          if(storage.isReliableStorage())
          {
            currentRevivalsCount = storage.readInt(CURRENT_REVIVALS_OFFSET);
            currentRevivalsCount++;
            storage.writeInt(CURRENT_REVIVALS_OFFSET,currentRevivalsCount);
          }
          else
            currentRevivalsCount++;

          drawBattery();
          if(appConfig.maxRevivalsCount>0)
          {
            tft.setTextColor(TFT_BLACK, TFT_GREEN);
            leftRevivalsCount = appConfig.maxRevivalsCount-currentRevivalsCount;
            if(leftRevivalsCount>0)
            {
              sprintf(displayString, "%0d more revivals", leftRevivalsCount);
              tft.drawCentreString(displayString, tft.width() / 2, 20, 4);
            }
            else
              tft.drawCentreString("No more revivals", tft.width() / 2, 20, 4);
          }

          playOK();
          finished = true;
        }
      }
      else
      {
        if (!tonePlay && ((now - startTime) >= 10))
        {
          tonePlay = true;
          if(appConfig.sfxDelay>0)
            playStartTone();
          else
            playStartMessage();
        }
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        int64_t rtime = (now - startTime);
        float time = rtime / 1000.0;
        rtime += 100;
        int64_t mstime = rtime - rtime / 1000 * 1000;
        sprintf(displayString, "%02.1f", time);
        tft.drawCentreString(displayString, tft.width() / 2, 50, 7);
        if ((now - lastSayTime) >= 1500 && mstime < 100 && (rtime <= (appConfig.medicTime - 1000)))
        {
          lastSayTime = now;
          sayTime(rtime / 1000);
        }

        drawBattery();
      }
      delay(50);
    }
    else
    {
      if (!finished)
      {
        if (appConfig.holdTime > 0)
        {
          int64_t now = millis();
          if (pauseTime == 0)
          {
            pauseTime = now;
            if(canPlaySFX())
              playErrorTone();
            tft.fillScreen(TFT_YELLOW);
            sbtnOn(0xFF,0xFF,0);
            tft.setTextSize(1);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            float time = (pauseTime - startTime) / 1000.0;
            sprintf(displayString, "%02.1f", time);
            tft.drawCentreString(displayString, tft.width() / 2, 50, 7);
            drawBattery();
            drawRevivals(TFT_BLACK,TFT_YELLOW);

            delay(25);
            continue;
          }
          else if ((now - pauseTime) < appConfig.holdTime)
          {
            delay(25);
            continue;
          }
        }

        tft.fillScreen(TFT_RED);
        sbtnOn(0xFF,0,0);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        float time = (pauseTime - startTime) / 1000.0;
        sprintf(displayString, "%02.1f", time);
        tft.drawCentreString(displayString, tft.width() / 2, 50, 7);
        drawBattery();
        drawRevivals();
        if(canPlaySFX())
          playErrorMessage();
      }
      break;
    }
  }
  sbtnOff();
  appState = AS_IDLE;
}


void medicDownState()
{
  char displayString[48];
  int32_t leftRevivalsCount = 0;
  int64_t startTime = millis();
  int64_t lastSayTime = millis();
  int64_t pauseTime = 0;
  bool finished = false;
  bool tonePlay = false;
  loadSettings();
  playStop();

  if(storage.isReliableStorage())
  {
    currentRevivalsCount = storage.readInt(CURRENT_REVIVALS_OFFSET);
  }

  auto drawRevivals=[&leftRevivalsCount,&displayString](uint16_t textcolor=TFT_WHITE, uint16_t color = TFT_RED)
  {
    if(appConfig.maxRevivalsCount>0)
    {
      tft.setTextSize(1);
      tft.setTextColor(textcolor, color);

      sprintf(displayString, "%d of %d (%d left)", currentRevivalsCount+1,appConfig.maxRevivalsCount,leftRevivalsCount);
      tft.drawCentreString(displayString, tft.width() / 2, 20, 4);
    }
  };

  if(appConfig.maxRevivalsCount>0)
  {
    leftRevivalsCount = appConfig.maxRevivalsCount-currentRevivalsCount;
    if(leftRevivalsCount<=0)
    {
      tft.fillScreen(TFT_RED);
      sbtnOn(0xFF,0,0);

      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE, TFT_RED);

      tft.drawCentreString("No more revivals", tft.width() / 2, 20, 4);
      tft.drawCentreString("Please reset", tft.width() / 2, 50, 4);
      playErrorTone();
      delay(appConfig.idleTime);
      appState = AS_IDLE;
      return;
    }
    else
    {
      drawRevivals();
    }
  }
  tft.fillScreen(TFT_WHITE);
  sbtnPulse(0xFF,0xFF,0xFF);
  playActivatingMedicDown();

  int lastNumber = 0;
  while (true)
  {
    secondBtnLED.loop();
    if (secondBtn.getState(true))
    {
      int64_t now = millis();
      if ((now - startTime) >= appConfig.medicDownDelay)
      {
        break;
      }
      else
      {
        tft.setTextSize(1);
        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        int64_t rtime = appConfig.medicDownDelay - (now - startTime);
        if(rtime<0)
          rtime = 0;
        float time = rtime / 1000.0;
        rtime += 100;
        int64_t mstime = rtime - rtime / 1000 * 1000;
        sprintf(displayString, "%02.1f", time);
        tft.drawCentreString(displayString, tft.width() / 2, 50, 7);
        drawBattery();        
        if(playerIsIdle() && rtime>=1000)
        {
          if(lastNumber!=(rtime / 1000) && (now-lastSayTime)>=950)
          {
            lastNumber = rtime / 1000;
            sayTime(lastNumber);
            lastSayTime = millis();
          }
        }
      }
    }
    else
    {
      tft.fillScreen(TFT_RED);
      sbtnOn(0xFF,0,0);
      tft.setTextSize(1);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawCentreString("ABORT", tft.width() / 2, 50, 4);
      drawBattery();
      drawRevivals();    
      playOKTone();        
      sbtnOff();
      appState = AS_IDLE;
      return;
    }
    delay(20);
  }

  playMedicDown();

  lastSayTime = millis();
  bool mainBtnState = false;
  while (true)
  {
      secondBtnLED.loop();
      int64_t now = millis();
      if (((now - startTime) >= appConfig.medicDownTime))
      {
        tft.fillScreen(TFT_GREEN);
        sbtnOn(0,0xFF,0);
        tft.setTextSize(1);
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        tft.drawCentreString("MEDIC ALIVE", tft.width() / 2, 50, 4);
        if(storage.isReliableStorage())
        {
          currentRevivalsCount = storage.readInt(CURRENT_REVIVALS_OFFSET);
          currentRevivalsCount++;
          storage.writeInt(CURRENT_REVIVALS_OFFSET,currentRevivalsCount);
        }
        else
          currentRevivalsCount++;

        drawBattery();
        if(appConfig.maxRevivalsCount>0)
        {
          tft.setTextColor(TFT_BLACK, TFT_GREEN);
          leftRevivalsCount = appConfig.maxRevivalsCount-currentRevivalsCount;
          if(leftRevivalsCount>0)
          {
            sprintf(displayString, "%0d more revivals", leftRevivalsCount);
            tft.drawCentreString(displayString, tft.width() / 2, 20, 4);
          }
          else
            tft.drawCentreString("No more revivals", tft.width() / 2, 20, 4);
        }

        playMedicAlive();
        appState = AS_IDLE;
        return;
      }
      else
      {
        bool b = mainBtn.getState(true);
        if(b!=mainBtnState)
        {
          mainBtnState = b;
          if(b)
            playError2Tone();
        }
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        int64_t rtime = (now - startTime);
        float time = rtime / 1000.0;
        rtime += 100;
        int64_t mstime = rtime - rtime / 1000 * 1000;
        sprintf(displayString, "%02.1f", time);
        tft.drawCentreString(displayString, tft.width() / 2, 50, 7);
        if(rtime<(appConfig.medicDownTime-10000))
        {
          if ((now - lastSayTime) >= 5500 && mstime < 100 && (rtime <= (appConfig.medicDownTime - 2000)))
          {
            lastSayTime = now;
            sayTime(rtime / 1000);
          }
        }
        else
        {
          #define BEEP_MIN_DELAY  150
          double delay = (appConfig.medicDownTime-rtime)*(1000.0-BEEP_MIN_DELAY)/10000.0+BEEP_MIN_DELAY;
          if ((now - lastSayTime) >= delay && (rtime <= (appConfig.medicDownTime - BEEP_MIN_DELAY/2)))
          {
            lastSayTime = now;
            playBeepShort();
          }

        }

        drawBattery();
      }
      delay(20);
  }
  sbtnOff();
  appState = AS_IDLE;
}

void idleState()
{
  int64_t startTime = millis();
  while (true)
  {
    int64_t now = millis();
    if (mainBtn.getState())
    {
      appState = AS_MEDIC;
      break;
    }
    if (secondBtn.getState())
    {
      appState = AS_MEDIC_DOWN;
      break;
    }
    if (btn1.isPressedRaw() && btn2.isPressedRaw())
    {
      appState = AS_SETUP;
      break;
    }
    if ((now - startTime) >= appConfig.idleTime)
    {
      deepSleep();
      startTime = millis();
    }
    delay(25);
  }
}

/*
void drawTimeScreen()
{
  tft.fillScreen(TFT_BLUE);
  drawBattery();
  char displayString[16];
  sprintf(displayString, "%02d s", appConfig.medicTime/1000);
  tft.drawCentreString(displayString,  tft.width()/2, 40,7);
}
*/

#define Black RGB565(0, 0, 0)
#define Red RGB565(255, 0, 0)
#define Green RGB565(0, 255, 0)
#define Blue RGB565(0, 0, 255)
#define Gray RGB565(128, 128, 128)
#define LighterRed RGB565(255, 150, 150)
#define LighterGreen RGB565(150, 255, 150)
#define LighterBlue RGB565(150, 150, 255)
#define DarkerRed RGB565(150, 0, 0)
#define DarkerGreen RGB565(0, 150, 0)
#define DarkerBlue RGB565(0, 0, 150)
#define Cyan RGB565(0, 255, 255)
#define Magenta RGB565(255, 0, 255)
#define Yellow RGB565(255, 255, 0)
#define White RGB565(255, 255, 255)

const colorDef<uint16_t> colors[6] MEMMODE = {
    {{(uint16_t)Black,
      (uint16_t)Black},
     {(uint16_t)Black,
      (uint16_t)DarkerBlue,
      (uint16_t)DarkerBlue}}, //bgColor
    {
        {(uint16_t)Gray,
         (uint16_t)Gray},
        {(uint16_t)White,
         (uint16_t)White,
         (uint16_t)White}}, //fgColor
    {
        {(uint16_t)White,
         (uint16_t)Black},
        {(uint16_t)Yellow,
         (uint16_t)Yellow,
         (uint16_t)Red}}, //valColor
    {
        {(uint16_t)White,
         (uint16_t)Black},
        {(uint16_t)White,
         (uint16_t)Yellow,
         (uint16_t)Yellow}}, //unitColor
    {
        {(uint16_t)White,
         (uint16_t)Gray},
        {(uint16_t)Black,
         (uint16_t)Blue,
         (uint16_t)White}}, //cursorColor
    {
        {(uint16_t)White,
         (uint16_t)Yellow},
        {(uint16_t)DarkerRed,
         (uint16_t)White,
         (uint16_t)White}}, //titleColor
};


class passwordTextField:public textField {
public:  
      inline passwordTextField(constMEM textFieldShadow& shadow):textField(shadow) {}
      inline passwordTextField(
        constText*label,
        char* b,
        idx_t sz,
        char* const* v,
        action a=doNothing,
        eventMask e=noEvent,
        styles style=noStyle,
        systemStyles ss=(Menu::systemStyles)(_noStyle|_canNav|_parentDraw)
      ):textField(label,b,sz,v,a,e,style,ss) {}

      void doNav(navNode& nav,navCmd cmd) override
      {
        trace(MENU_DEBUG_OUT<<"textField::doNav:"<<cmd.cmd<<endl);
        switch(cmd.cmd) {
          case enterCmd:
            if(cursor<(idx_t)strlen(buffer())-1) 
              cursor++;
            else
            {
              edited=false;
              cursor=0;
              nav.root->exit();
            }
            dirty=true;
            break;
          case escCmd:
            edited=false;
            cursor=0;
            nav.root->exit();
            break;
          case upCmd:
          {
            const char* v=validator(cursor);
            char *at=strchr(v,buffer()[cursor]);
            idx_t pos=at?at-v+1:1;
            if (pos>=(idx_t)strlen(v)) pos=0;
            buffer()[cursor]=v[pos];
            dirty=true;
            break;
          }
          case downCmd:
          {
            const char* v=validator(cursor);
            char *at=strchr(v,buffer()[cursor]);//at is not stored on the class to save memory
            idx_t pos=at?at-v-1:0;
            if (pos<0) pos=strlen(v)-1;
            buffer()[cursor]=v[pos];
            dirty=true;
            break;
          }
          default:break;
        }
        trace(MENU_DEBUG_OUT<<"cursor:"<<cursor<<endl);
      }

      Used printTo(navRoot &root,bool sel,menuOut& out, idx_t idx,idx_t len,idx_t panelNr=0) override
      {
        trace(MENU_DEBUG_OUT<<*this<<" textField::printTo"<<endl);
        // out.fmtStart(menuOut::fmtPrompt,root.node(),idx);
        bool editing=this==root.navFocus;
        trace(MENU_DEBUG_OUT<<"editing:"<<editing<<" len:"<<len;)
        idx_t l=navTarget::printTo(root,sel,out,idx,len,panelNr);
        #ifdef MENU_FMT_WRAPS
          out.fmtStart(*this,menuOut::fmtEditCursor,root.node(),idx);
        #endif
        if (l<len) {
          out.write(editing?":":" ");
          l++;
        }
        #ifdef MENU_FMT_WRAPS
          out.fmtEnd(*this,menuOut::fmtEditCursor,root.node(),idx);
        #endif
        // idx_t c=l;
        //idx_t top=out.tops[root.level];
        #ifdef MENU_FMT_WRAPS
          out.fmtStart(*this,menuOut::fmtTextField,root.node(),idx);
        #endif
        idx_t tit=hasTitle(root.node())?1:0;
        idx_t c=l+1;//initial cursor, after label
        idx_t line=idx+tit;//-out.tops[root.level];
        idx_t ew=len-c;//editable width
        idx_t at=cursor>=ew?cursor-ew:0;//adjust print start
        while(buffer()[at]&&l++<len) {//while not string terminated or buffer length reached
          trace(if (editing) MENU_DEBUG_OUT<<endl<<"{"<<at<<"}");
          if (at==cursor&&editing) {//edit cursor
            trace(MENU_DEBUG_OUT<<"ew:"<<ew<<" cursor:"<<cursor<<" l:"<<l<<" len:"<<len<<endl);
            c=l;//store cursor position
            l+=out.startCursor(root,l,line,true);//draw text cursor or color code start
            out.write(buffer()[at++]);//draw focused character
            l+=out.endCursor(root,l,line,true);//draw text cursor or color code end
          } else 
          if(editing)
          {
            out.write(buffer()[at++]);//just the character
          }
          else
            if(buffer()[at++]) out.write('*');
        }
        //this is the cursor frame
        out.editCursor(root,c,line,editing,charEdit);//reposition a gfx cursor
        #ifdef MENU_FMT_WRAPS
          out.fmtEnd(*this,menuOut::fmtTextField,root.node(),idx);
        #endif
        return l;
      }
};

#define MAX_DEPTH 2

using namespace Menu;

bool setupFinished;
int32_t setupMedicTime = 0;
int32_t setupMedicDownTime = 0;
char setupSetupPin[4];
char setupStartupPin[4];

result onSetupMDTime()
{
  appConfig.medicDownTime = setupMedicDownTime * 1000;
  storage.writeInt(MEDIC_DOWN_TIME_OFFSET, appConfig.medicDownTime);
  return proceed;
}

result onSetupMedicTime()
{
  appConfig.medicTime = setupMedicTime * 1000;
  storage.writeInt(MEDIC_TIME_OFFSET, appConfig.medicTime);
  return proceed;
}


result onSetupMDDelay()
{
  storage.writeInt(MEDIC_DOWN_TIME_OFFSET, appConfig.medicDownDelay);
  return proceed;
}

result onSetupSFXDelay()
{
  storage.writeInt(SFX_DELAY_OFFSET, appConfig.sfxDelay);
  return proceed;
}

result onSetupRevivalsCount()
{
  storage.writeInt(REVIVALS_COUNT_OFFSET, appConfig.maxRevivalsCount);
  return proceed;
}

result onSetupStartupPin()
{
  appConfig.startupPin = atol(setupStartupPin);
  storage.writeInt(STARTUP_PIN_OFFSET, appConfig.startupPin);
  return proceed;
}

result onSetupSetupPin()
{
  appConfig.setupPin = atol(setupSetupPin);
  storage.writeInt(SETUP_PIN_OFFSET, appConfig.setupPin);
  return proceed;
}

result onSetupResetCounter()
{
  currentRevivalsCount = 0;
  if(storage.isReliableStorage())
  {
    storage.writeInt(CURRENT_REVIVALS_OFFSET,0);
  }
  playOKTone();
  return proceed;
}

result onSetupExit()
{
  setupFinished = true;
  storage.commit();
  return proceed;
}

result onSetupFlagsChanged()
{
  int32_t flags = 0;
  if(appConfig.askStartupPin)
    flags |= 1;
  if(appConfig.askSetupPin)
    flags |= 2;
  storage.writeInt(FLAGS_OFFSET, flags);
  return proceed;
}


TOGGLE(appConfig.askStartupPin,setAskStartup,"Ask Startup: ",doNothing,noEvent,noStyle
  ,VALUE("On",1,onSetupFlagsChanged,noEvent)
  ,VALUE("Off",0,onSetupFlagsChanged,noEvent)
);

TOGGLE(appConfig.askSetupPin,setAskSetup,"Ask Setup: ",doNothing,noEvent,noStyle
  ,VALUE("On",1,onSetupFlagsChanged,noEvent)
  ,VALUE("Off",0,onSetupFlagsChanged,noEvent)
);

const char* const  digit[] MEMMODE={"0123456789"};
//const char* constMEM validData[] MEMMODE={digit,digit,digit};

MENU(setupMenu, "Setup menu", doNothing, noEvent, wrapStyle
,OP("Reset Revivals...", onSetupResetCounter, enterEvent)
,FIELD(setupMedicTime, "Medic Time ", "s", 10, 200, 5, 1, onSetupMedicTime, exitEvent, wrapStyle)
,FIELD(appConfig.maxRevivalsCount, "Revivals ", "", 0, 1000, 5, 1, onSetupRevivalsCount, exitEvent, wrapStyle)
,SUBMENU(setAskStartup)
,EDIT_(__COUNTER__,passwordTextField,(Menu::systemStyles)(_noStyle|_canNav|_parentDraw),"Startup Pin",setupStartupPin,digit,onSetupStartupPin,exitEvent,noStyle)
,SUBMENU(setAskSetup)
,EDIT_(__COUNTER__,passwordTextField,(Menu::systemStyles)(_noStyle|_canNav|_parentDraw),"Setup Pin",setupSetupPin,digit,onSetupSetupPin,exitEvent,noStyle)
,FIELD(appConfig.sfxDelay, "SFX Delay ", "ms", 0, 10000, 1000, 100, onSetupSFXDelay, exitEvent, wrapStyle)
,FIELD(appConfig.medicDownDelay, "MD Delay ", "ms", 5000, 10000, 1000, 100, onSetupMDDelay, exitEvent, wrapStyle)
,FIELD(setupMedicDownTime, "MD Time ", "s", 10, 600, 5, 1, onSetupMDTime, exitEvent, wrapStyle)
,OP("<Exit", onSetupExit, enterEvent)
);

#define fontW 14
#define fontH 24

const panel panels[] MEMMODE = {{0, 0, TFT_HEIGHT / fontW + 1, TFT_WIDTH / fontH}};
navNode *nodes[sizeof(panels) / sizeof(panel)]; //navNodes to store navigation status
panelsList pList(panels, nodes, 1);             //a list of panels and nodes
idx_t eSpiTops[MAX_DEPTH] = {0};
TFT_eSPIOut eSpiOut(tft, colors, eSpiTops, pList, fontW, fontH+2);
menuOut *constMEM outputs[] MEMMODE = {&eSpiOut};              //list of output devices
outputsList out(outputs, sizeof(outputs) / sizeof(menuOut *)); //outputs list controller

noInput input;

NAVROOT(nav, setupMenu, MAX_DEPTH, input, out);

int64_t setupTime;
void setupState()
{
  loadSettings();
  tft.fillScreen(TFT_BLUE);
  drawBattery();

  while (btn1.isPressedRaw() || btn2.isPressedRaw())
  {
    delay(25);
  }

  if(appConfig.askSetupPin)
  {
    bool pinMatch = false;
    int i = 0;
    while(!pinMatch && i<3)
    {
      i++;
      int pin = enterPin("Admin pin",3,10000);
      pinMatch = appConfig.setupPin == pin;
      if(!pinMatch)
        playErrorTone();
      if(pin==-1)
        break;
    }
    if(!pinMatch)
    {
      appState = AS_IDLE;
      return;
    }
    else
    {
      playOKTone();
    }
  }
   
  setupTime = millis();
  setupFinished = false;
  btn2.setClickHandler([](Button2 &b) {
    nav.doNav(upCmd);
    setupTime = millis();
  });

  btn2.setLongClickHandler([](Button2 &b) {
    nav.doNav(enterCmd);
    setupTime = millis();
  });

  btn1.setClickHandler([](Button2 &b) {
    nav.doNav(downCmd);
    setupTime = millis();
  });

  btn1.setLongClickHandler([](Button2 &b) {
    nav.doNav(enterCmd);
    setupTime = millis();
  });

  setupMedicDownTime = appConfig.medicDownTime / 1000;
  setupMedicTime = appConfig.medicTime / 1000;
  snprintf(setupSetupPin,sizeof(setupSetupPin),"%03d",appConfig.setupPin);
  snprintf(setupStartupPin,sizeof(setupStartupPin),"%03d",appConfig.startupPin);

  tft.setFreeFont(&FreeMono12pt7b);
  tft.setTextSize(1);
  eSpiOut.fontMarginY = 18;

  tft.fillScreen(TFT_BLACK);
  delay(50);
  nav.reset();
  while (!setupFinished && (millis() - setupTime) < appConfig.setupTimeout)
  {
    btn1.loop();
    btn2.loop();
    nav.poll();
    delay(25);
  }
  tft.setTextFont(1);
  tft.fillScreen(TFT_BLACK);
  drawBattery();
  appState = AS_IDLE;
}

void loop()
{
  if(firstStart)
  {
    if(appConfig.askStartupPin)
    {
      bool pinMatch = false;
      int i = 0;
      while(!pinMatch && i<3)
      {
        i++;
        int pin = enterPin("Startup pin",3,10000);
        pinMatch = appConfig.startupPin == pin;
        if(!pinMatch)
          playErrorTone();
        if(pin==-1)
          break;
      }
      if(!pinMatch)
      {
        delay(500);
        deepSleep();
        return;
      }
      else
      {
        firstStart = false;
        playOKTone();
      }
    }
    else
      firstStart = false;
  }
  tft.fillScreen(TFT_BLACK);
  drawBattery();

  while (true)
  {
    switch (appState)
    {
    case AS_IDLE:
    {
      idleState();
      break;
    }
    case AS_MEDIC:
    {
      medicState();
      break;
    }
    case AS_MEDIC_DOWN:
    {
      medicDownState();
      break;
    }
    case AS_SETUP:
    {
      setupState();
      break;
    }
    }
    delay(25);
  }
}